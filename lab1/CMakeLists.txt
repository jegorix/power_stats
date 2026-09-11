cmake_minimum_required(VERSION 3.10)
project(PowerMonitorLab)

# Используем стандарт C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Включаем автоматическую обработку макросов Qt (MOC)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

# Ищем библиотеку libudev для power_monitor
find_package(PkgConfig REQUIRED)
pkg_check_modules(UDEV REQUIRED libudev)

# Ищем компоненты Qt5 для красивого GUI (Widgets) и сокетов (Network)
find_package(Qt5 COMPONENTS Widgets Network REQUIRED)

# ---------------------------------------------------------
# Сборка консольного монитора (power_monitor)
# ---------------------------------------------------------
add_executable(power_monitor power_monitor.cpp)
# Линкуем с udev и потоками
target_link_libraries(power_monitor ${UDEV_LIBRARIES} pthread)
target_include_directories(power_monitor PRIVATE ${UDEV_INCLUDE_DIRS})

# ---------------------------------------------------------
# Сборка графического приложения (power_gui)
# ---------------------------------------------------------
add_executable(power_gui power_gui.cpp)
# Линкуем с графическим и сетевым модулями Qt
target_link_libraries(power_gui Qt5::Widgets Qt5::Network)