#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QComboBox>
#include <QLocalSocket>
#include <QProcess>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>

// Главный класс графического интерфейса
class PowerGui : public QWidget {
    Q_OBJECT

public:
    PowerGui(QWidget *parent = nullptr) : QWidget(parent) {
        setup_ui();
        setup_network();
    }

private:
    QLabel *statusLabel;
    QProgressBar *batteryBar;
    QLocalSocket *socket;

    void setup_ui() {
        this->setWindowTitle("Монитор энергопитания");
        this->resize(400, 250);
        
        // Красивая темная тема оформления (QSS)
        this->setStyleSheet(R"(
            QWidget { background-color: #2b2b2b; color: #ffffff; font-family: 'Segoe UI', Arial, sans-serif; }
            QLabel { font-size: 14px; font-weight: bold; }
            QPushButton { 
                background-color: #3a3a3a; border: 1px solid #555; 
                border-radius: 5px; padding: 8px; font-size: 13px; 
            }
            QPushButton:hover { background-color: #505050; }
            QPushButton:pressed { background-color: #202020; }
            QComboBox { 
                background-color: #3a3a3a; border: 1px solid #555; 
                border-radius: 5px; padding: 4px; 
            }
            QProgressBar {
                border: 1px solid #555; border-radius: 5px; 
                text-align: center; font-weight: bold; background-color: #1a1a1a;
            }
        )");

        QVBoxLayout *mainLayout = new QVBoxLayout(this);

        // Информационный блок
        statusLabel = new QLabel("Ожидание подключения к демону...", this);
        statusLabel->setAlignment(Qt::AlignCenter);
        mainLayout->addWidget(statusLabel);

        batteryBar = new QProgressBar(this);
        batteryBar->setRange(0, 100);
        batteryBar->setValue(0);
        batteryBar->setFormat("Нет данных");
        batteryBar->setFixedHeight(30);
        mainLayout->addWidget(batteryBar);

        // Панель кнопок сна и гибернации
        QHBoxLayout *powerLayout = new QHBoxLayout();
        QPushButton *btnSleep = new QPushButton("Спящий режим", this);
        QPushButton *btnHibernate = new QPushButton("Гибернация", this);
        powerLayout->addWidget(btnSleep);
        powerLayout->addWidget(btnHibernate);
        mainLayout->addLayout(powerLayout);

        // Панель смены governor (режима процессора)
        QHBoxLayout *govLayout = new QHBoxLayout();
        QLabel *govLabel = new QLabel("Governor (CPU):", this);
        QComboBox *govBox = new QComboBox(this);
        govBox->addItem("powersave");
        govBox->addItem("performance");
        govLayout->addWidget(govLabel);
        govLayout->addWidget(govBox);
        mainLayout->addLayout(govLayout);

        // Привязываем кнопки к системным командам
        connect(btnSleep, &QPushButton::clicked, this, []() {
            QProcess::execute("systemctl", {"suspend"});
        });
        connect(btnHibernate, &QPushButton::clicked, this, []() {
            QProcess::execute("systemctl", {"hibernate"});
        });
        
        // Смена профиля производительности. В реальной системе требует прав root (pkexec).
        connect(govBox, &QComboBox::currentTextChanged, this, [](const QString &text) {
            QString cmd = QString("echo %1 | pkexec tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor").arg(text);
            QProcess::startDetached("sh", {"-c", cmd});
        });
    }

    void setup_network() {
        socket = new QLocalSocket(this);
        
        // Подключаемся к Unix-сокету демона
        socket->connectToServer("/tmp/power_monitor.sock");
        
        connect(socket, &QLocalSocket::readyRead, this, &PowerGui::read_data);
        
        connect(socket, &QLocalSocket::errorOccurred, this, [this]() {
            batteryBar->setValue(0);
            batteryBar->setFormat("Нет данных");
            statusLabel->setText("Ошибка: Нет связи с power_monitor!");
            statusLabel->setStyleSheet("color: #ff5555;"); // Красный текст ошибки
        });
    }

    // Чтение и парсинг входящих данных
    void read_data() {
        while (socket->canReadLine()) {
            QString line = QString::fromUtf8(socket->readLine()).trimmed();
            QStringList parts = line.split(";");
            
            if (parts.size() == 2) {
                bool valid = false;
                int capacity = parts[0].toInt(&valid);
                QString status = parts[1];
                if (status == "Battery not found") status = "Батарея не обнаружена";
                else if (status == "Battery data unavailable") status = "Данные батареи недоступны";

                // Обновляем текст
                statusLabel->setText(QString("Статус: %1").arg(status));
                statusLabel->setStyleSheet("color: #ffffff;");
                
                // Обновляем прогресс бар
                if (!valid || capacity < 0 || capacity > 100) {
                    batteryBar->setValue(0);
                    batteryBar->setFormat("Заряд недоступен");
                    continue;
                }
                batteryBar->setFormat("%p%");
                batteryBar->setValue(capacity);

                // Динамическое изменение цвета (согласно заданию: зеленая, желтая, красная зона)
                QString barColor;
                if (capacity >= 50) barColor = "#4CAF50";      // Зеленый
                else if (capacity >= 20) barColor = "#FFC107"; // Желтый
                else barColor = "#F44336";                     // Красный

                batteryBar->setStyleSheet(QString(
                    "QProgressBar::chunk { background-color: %1; border-radius: 4px; }"
                ).arg(barColor));
            }
        }
    }
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    PowerGui window;
    window.show();
    
    return app.exec();
}

// Этот инклуд обязателен, если классы Qt объявлены в .cpp файле (для корректной работы MOC)
#include "power_gui.moc"
