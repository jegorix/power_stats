#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <vector>
#include <mutex>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <libudev.h>
#include <poll.h>

namespace fs = std::filesystem;

// Глобальные переменные для хранения текущего состояния
std::string current_status = "Unknown";
int current_capacity = 0;
std::mutex data_mutex;

const std::string LOG_FILE = "power.log";
const size_t MAX_LOG_SIZE = 1024 * 1024; // 1 Мегабайт
const std::string SOCKET_PATH = "/tmp/power_monitor.sock";

// Функция для чтения значений из псевдофайловой системы sysfs
std::string read_sysfs(const std::string& path) {
    std::ifstream file(path);
    std::string value;
    if (file.is_open()) {
        file >> value;
    }
    return value;
}

// Обновление текущих данных о батарее
void update_battery_data() {
    std::lock_guard<std::mutex> lock(data_mutex);
    // Читаем текущий уровень заряда и статус (Заряжается / Разряжается)
    std::string cap_str = read_sysfs("/sys/class/power_supply/BAT0/capacity");
    if (!cap_str.empty()) current_capacity = std::stoi(cap_str);
    
    std::string stat = read_sysfs("/sys/class/power_supply/BAT0/status");
    if (!stat.empty()) current_status = stat;
}

// Функция для записи лога с поддержкой ротации
void write_log(const std::string& message) {
    std::lock_guard<std::mutex> lock(data_mutex);
    
    // Проверяем размер файла для ротации
    if (fs::exists(LOG_FILE) && fs::file_size(LOG_FILE) > MAX_LOG_SIZE) {
        fs::rename(LOG_FILE, LOG_FILE + ".1"); // Переименовываем старый лог
    }

    std::ofstream log(LOG_FILE, std::ios::app);
    if (log.is_open()) {
        auto now = std::chrono::system_clock::now();
        std::time_t now_time = std::chrono::system_clock::to_time_t(now);
        log << std::put_time(std::localtime(&now_time), "%Y-%m-%d %H:%M:%S") 
            << " | " << message << std::endl;
    }
}

// Поток для работы с Unix сокетом (отправка данных клиентам)
void socket_server_thread() {
    int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("Ошибка создания сокета");
        return;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH.c_str(), sizeof(addr.sun_path) - 1);

    // Удаляем старый сокет, если он остался от прошлого запуска
    unlink(SOCKET_PATH.c_str());

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("Ошибка привязки сокета");
        return;
    }

    if (listen(server_fd, 5) == -1) {
        perror("Ошибка listen");
        return;
    }

    std::cout << "Unix-сокет запущен на " << SOCKET_PATH << "\n";

    while (true) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd == -1) continue;

        // Как только клиент подключился, отправляем ему данные циклично
        while (true) {
            std::string data_to_send;
            {
                std::lock_guard<std::mutex> lock(data_mutex);
                // Формируем простую строку: "Capacity;Status"
                data_to_send = std::to_string(current_capacity) + ";" + current_status + "\n";
            }
            
            ssize_t bytes_sent = send(client_fd, data_to_send.c_str(), data_to_send.length(), MSG_NOSIGNAL);
            if (bytes_sent <= 0) {
                break; // Клиент отключился
            }
            std::this_thread::sleep_for(std::chrono::seconds(2)); // Обновляем GUI раз в 2 секунды
        }
        close(client_fd);
    }
    close(server_fd);
}

int main() {
    std::cout << "Запуск power_monitor (работает в фоне)...\n";
    write_log("Сервис power_monitor запущен");

    // Читаем начальные данные
    update_battery_data();

    // Запускаем серверный поток для общения с GUI
    std::thread server(socket_server_thread);
    server.detach();

    // Настраиваем udev для мониторинга событий подсистемы power_supply
    struct udev* udev = udev_new();
    if (!udev) {
        std::cerr << "Не удалось инициализировать udev\n";
        return 1;
    }

    struct udev_monitor* mon = udev_monitor_new_from_netlink(udev, "udev");
    udev_monitor_filter_add_match_subsystem_devtype(mon, "power_supply", NULL);
    udev_monitor_enable_receiving(mon);

    int fd = udev_monitor_get_fd(mon);
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN;

    // Главный цикл мониторинга
    while (true) {
        // Ожидаем события от железа
        int ret = poll(&pfd, 1, -1);
        if (ret > 0 && (pfd.revents & POLLIN)) {
            struct udev_device* dev = udev_monitor_receive_device(mon);
            if (dev) {
                // Если произошло событие с батареей или блоком питания
                update_battery_data();
                
                std::string msg = "Событие udev! Текущий заряд: " + std::to_string(current_capacity) + 
                                  "%, Статус: " + current_status;
                write_log(msg);
                std::cout << "Зарегистрировано изменение: " << msg << std::endl;
                
                udev_device_unref(dev);
            }
        }
    }

    udev_monitor_unref(mon);
    udev_unref(udev);
    return 0;
}