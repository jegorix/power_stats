#pragma once

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

struct BatteryData {
    int capacity = -1; // Недоступный заряд отличается от реальных 0%.
    std::string status = "Battery not found";
};

inline std::string read_sysfs(const std::filesystem::path& path) {
    std::ifstream file(path);
    std::string value;
    std::getline(file, value); // Сохраняем статусы вроде "Not charging".
    return value;
}

inline double read_number(const std::filesystem::path& path) {
    std::istringstream input(read_sysfs(path));
    double value;
    if (!(input >> value) || !std::isfinite(value) || value < 0) return -1;
    input >> std::ws;
    return input.eof() ? value : -1;
}

inline int read_capacity(const std::filesystem::path& path) {
    double capacity = read_number(path / "capacity");
    if (capacity >= 0 && capacity <= 100) return static_cast<int>(std::lround(capacity));
    for (const auto* prefix : {"energy", "charge"}) {
        const double now = read_number(path / (std::string(prefix) + "_now"));
        const double full = read_number(path / (std::string(prefix) + "_full"));
        if (now >= 0 && full > 0) {
            return static_cast<int>(std::lround(std::min(now / full, 1.0) * 100));
        }
    }
    return -1;
}

inline BatteryData read_battery_data(
    const std::filesystem::path& root = "/sys/class/power_supply") {
    std::error_code error;
    std::vector<std::filesystem::path> batteries;
    std::filesystem::directory_iterator it(root, error), end;
    if (error) return {-1, "Battery data unavailable"};
    for (; it != end; it.increment(error)) {
        if (error) return {-1, "Battery data unavailable"};
        const auto path = it->path();
        if (read_sysfs(path / "type") == "Battery" &&
            read_sysfs(path / "scope") != "Device" &&
            read_sysfs(path / "present") != "0") {
            batteries.push_back(path);
        }
    }
    if (error) return {-1, "Battery data unavailable"};
    // Интерфейс показывает одну батарею; выбираем первую с доступным зарядом.
    std::sort(batteries.begin(), batteries.end());
    BatteryData result;
    for (const auto& path : batteries) {
        const auto status = read_sysfs(path / "status");
        result = {read_capacity(path), status.empty() ? "Unknown" : status};
        if (result.capacity >= 0) break;
    }
    return result;
}
