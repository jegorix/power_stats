#include "battery_data.h"
#include <chrono>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;

int main() {
    const auto root = fs::temp_directory_path() / ("battery-test-" + std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count()));
    auto put = [&](const std::string& name, const std::string& value) {
        fs::create_directories((root / name).parent_path());
        std::ofstream(root / name) << value << '\n';
    };
    auto check = [](bool condition) {
        if (!condition) throw std::runtime_error("Battery regression test failed");
    };
    try {
        check(read_battery_data(root).capacity == -1);
        fs::create_directories(root);
        check(read_battery_data(root).status == "Battery not found");
        put("AC/type", "Mains");
        put("AC/capacity", "100");
        put("mouse/type", "Battery");
        put("mouse/scope", "Device");
        put("mouse/capacity", "95");
        check(read_battery_data(root).capacity == -1);
        put("BAT1/type", "Battery");
        put("BAT1/status", "Not charging");
        put("BAT1/capacity", "73");
        check(read_battery_data(root).capacity == 73);
        check(read_battery_data(root).status == "Not charging");
        put("BAT1/capacity", "0");
        check(read_battery_data(root).capacity == 0);
        put("BAT1/capacity", "bad");
        check(read_battery_data(root).capacity == -1);
        put("BAT1/energy_now", "30000000");
        put("BAT1/energy_full", "60000000");
        check(read_battery_data(root).capacity == 50);
        put("BAT1/energy_full", "0");
        put("BAT1/charge_now", "1500");
        put("BAT1/charge_full", "6000");
        check(read_battery_data(root).capacity == 25);
        put("BAT1/charge_now", "7000");
        check(read_battery_data(root).capacity == 100);
        put("BAT1/charge_now", "25garbage");
        check(read_battery_data(root).capacity == -1);
        put("BAT1/present", "0");
        check(read_battery_data(root).status == "Battery not found");
        fs::remove_all(root / "BAT1");
        put("vendor-battery/type", "Battery");
        put("vendor-battery/capacity", "42");
        check(read_battery_data(root).capacity == 42);
        fs::remove_all(root / "vendor-battery");
        check(read_battery_data(root).capacity == -1);
    } catch (...) {
        fs::remove_all(root);
        throw;
    }
    fs::remove_all(root);
    std::cout << "Battery regression tests passed\n";
}
