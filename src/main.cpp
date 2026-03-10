#include <spdlog/spdlog.h>
#include "board/motherboard.h"
#include "tools/multimeter.h"
#include "ic/ic_8284a.h"
#include "ic/ic_8288.h"
#include <thread>
#include <chrono>

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

    // --- Multimeter: continuity audit ---
    spdlog::info("--- Multimeter: continuity audit ---");
    bench::Multimeter dmm(mb);
    int failures = dmm.audit("assets/pcb/64_256KB_SYSTEM_BOARD_rev1_2a.brd");
    spdlog::info("Audit result: {}", failures == 0 ? "ALL NETS OK" : "FAILURES DETECTED");

    // --- Insert 8284A clock generator into socket U11 ---
    spdlog::info("--- Inserting 8284A clock generator ---");
    auto clk_gen = std::make_unique<bench::IC_8284A>();
    clk_gen->install(mb.u11);
    // Start the IC's thread -- it will wait for VCC.
    clk_gen->power_on();
    mb.u11.insert(std::move(clk_gen));
    spdlog::info("  Clock:   {} [{}] -- {}", mb.u11.ref(), mb.u11.label(), mb.u11.occupied() ? "occupied" : "empty");

    // --- Insert 8288 bus controller into socket U6 ---
    spdlog::info("--- Inserting 8288 bus controller ---");
    auto bus_ctrl = std::make_unique<bench::IC_8288>();
    bus_ctrl->install(mb.u6);
    bus_ctrl->power_on();
    mb.u6.insert(std::move(bus_ctrl));
    spdlog::info("  BusCtrl: {} [{}] -- {}", mb.u6.ref(), mb.u6.label(), mb.u6.occupied() ? "occupied" : "empty");

    // --- Power on: VCC goes High, 8284A starts oscillating ---
    spdlog::info("--- Power on ---");
    mb.power_on();

    // Let the oscillator spin for a moment.
    spdlog::info("Letting oscillator run for 100ms...");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Probe clock signals with the multimeter.
    spdlog::info("--- Clock signal check ---");
    auto clk_sig = dmm.probe("U11", 8);
    auto pclk_sig = dmm.probe("U11", 2);
    auto osc_sig = dmm.probe("U11", 12);
    auto reset_sig = dmm.probe("U11", 10);
    auto ready_sig = dmm.probe("U11", 5);

    if (clk_sig)   spdlog::info("  CLK   (U11.8):  {} -- {}", clk_sig->name(), clk_sig->level() == bench::Level::HiZ ? "HiZ" : (clk_sig->level() == bench::Level::High ? "High" : "Low"));
    if (pclk_sig)  spdlog::info("  PCLK  (U11.2):  {} -- {}", pclk_sig->name(), pclk_sig->level() == bench::Level::HiZ ? "HiZ" : (pclk_sig->level() == bench::Level::High ? "High" : "Low"));
    if (osc_sig)   spdlog::info("  OSC   (U11.12): {} -- {}", osc_sig->name(), osc_sig->level() == bench::Level::HiZ ? "HiZ" : (osc_sig->level() == bench::Level::High ? "High" : "Low"));
    if (reset_sig) spdlog::info("  RESET (U11.10): {} -- {}", reset_sig->name(), reset_sig->level() == bench::Level::HiZ ? "HiZ" : (reset_sig->level() == bench::Level::High ? "High" : "Low"));
    if (ready_sig) spdlog::info("  READY (U11.5):  {} -- {}", ready_sig->name(), ready_sig->level() == bench::Level::HiZ ? "HiZ" : (ready_sig->level() == bench::Level::High ? "High" : "Low"));

    // --- 8288 bus controller signal check ---
    spdlog::info("--- 8288 bus controller signals ---");
    auto ale_sig  = dmm.probe("U6", 5);
    auto den_sig  = dmm.probe("U6", 4);
    auto dtr_sig  = dmm.probe("U6", 16);
    auto memr_sig = dmm.probe("U6", 7);
    auto memw_sig = dmm.probe("U6", 8);
    auto ior_sig  = dmm.probe("U6", 13);
    auto iow_sig  = dmm.probe("U6", 12);
    auto inta_sig = dmm.probe("U6", 14);

    auto level_str = [](bench::Signal* s) -> const char* {
        if (!s) return "N/C";
        switch (s->level()) {
            case bench::Level::High: return "High";
            case bench::Level::Low:  return "Low";
            default: return "HiZ";
        }
    };

    if (ale_sig)  spdlog::info("  ALE   (U6.5):  {} -- {}", ale_sig->name(), level_str(ale_sig));
    if (den_sig)  spdlog::info("  ~DEN  (U6.4):  {} -- {}", den_sig->name(), level_str(den_sig));
    if (dtr_sig)  spdlog::info("  DT/~R (U6.16): {} -- {}", dtr_sig->name(), level_str(dtr_sig));
    if (memr_sig) spdlog::info("  ~MEMR (U6.7):  {} -- {}", memr_sig->name(), level_str(memr_sig));
    if (memw_sig) spdlog::info("  ~MEMW (U6.8):  {} -- {}", memw_sig->name(), level_str(memw_sig));
    if (ior_sig)  spdlog::info("  ~IOR  (U6.13): {} -- {}", ior_sig->name(), level_str(ior_sig));
    if (iow_sig)  spdlog::info("  ~IOW  (U6.12): {} -- {}", iow_sig->name(), level_str(iow_sig));
    if (inta_sig) spdlog::info("  ~INTA (U6.14): {} -- {}", inta_sig->name(), level_str(inta_sig));

    spdlog::info("PSU POWER_GOOD: {}", mb.psu.power_good.level() == bench::Level::High ? "YES" : "NO");

    // Power off. IC threads join via jthread destructor when
    // Sockets destruct (before Signals, per declaration order).
    mb.power_off();

    spdlog::info("Done.");
    return 0;
}
