#include <spdlog/spdlog.h>
#include "board/motherboard.h"

int main() {
    spdlog::set_level(spdlog::level::debug);
    spdlog::info("bench -- IBM PC 5150 interconnect emulator");

    bench::Motherboard mb;

    // Report board status
    spdlog::info("Sockets:");
    spdlog::info("  CPU:     {} [{}] -- {}", mb.u3.ref(), mb.u3.label(), mb.u3.occupied() ? "occupied" : "empty");
    spdlog::info("  Clock:   {} [{}] -- {}", mb.u11.ref(), mb.u11.label(), mb.u11.occupied() ? "occupied" : "empty");
    spdlog::info("  BusCtrl: {} [{}] -- {}", mb.u6.ref(), mb.u6.label(), mb.u6.occupied() ? "occupied" : "empty");
    spdlog::info("  PIC:     {} [{}] -- {}", mb.u2.ref(), mb.u2.label(), mb.u2.occupied() ? "occupied" : "empty");
    spdlog::info("  PIT:     {} [{}] -- {}", mb.u34.ref(), mb.u34.label(), mb.u34.occupied() ? "occupied" : "empty");
    spdlog::info("  DMA:     {} [{}] -- {}", mb.u35.ref(), mb.u35.label(), mb.u35.occupied() ? "occupied" : "empty");
    spdlog::info("  PPI:     {} [{}] -- {}", mb.u36.ref(), mb.u36.label(), mb.u36.occupied() ? "occupied" : "empty");
    spdlog::info("  8087:    {} [{}] -- {}", mb.xu4.ref(), mb.xu4.label(), mb.xu4.occupied() ? "occupied" : "empty");

    spdlog::info("ISA Slots: J1-J5 -- all {}", mb.j1.occupied() ? "occupied" : "empty");
    spdlog::info("RAM: {} banks x 9 chips = {} sockets",
                 4, mb.ram_bank0.size() + mb.ram_bank1.size() +
                    mb.ram_bank2.size() + mb.ram_bank3.size());

    // Power on -- asserts rails, applies DIP switches and pull-ups
    mb.power_on();
    spdlog::info("PSU POWER_GOOD: {}", mb.psu.power_good.level() == bench::Level::High ? "YES" : "NO");

    // All sockets empty -- nothing will respond. But the wiring is live.
    spdlog::info("System bus: {} address lines, {} data lines",
                 mb.sa.width(), mb.sd.width());
    spdlog::info("Address bus reads: 0x{:05X}", mb.sa.read());
    spdlog::info("Data bus reads: 0x{:02X}", mb.sd.read());

    mb.power_off();
    return 0;
}
