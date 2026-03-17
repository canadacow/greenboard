#include <spdlog/spdlog.h>
#include "board/motherboard.h"
#include "tools/multimeter.h"
#include "ic/ic_8284a.h"
#include "ic/ic_8288.h"
#include "ic/ic_8253.h"
#include "ic/ic_74s175.h"
#include "ic/ic_8259a.h"
#include "ic/ic_8255a.h"
#include "ic/ic_8237a.h"
#include "ic/ic_8088.h"
#include "ic/ic_rom_8k.h"
#include "ic/ic_74s373.h"
#include "ic/ic_74s244.h"
#include "ic/ic_74s245.h"
#include "ic/ic_74s08.h"
#include "ic/ic_74s10.h"
#include "ic/ic_74ls02.h"
#include "ic/ic_74ls32.h"
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

    // --- Insert ICs into sockets ---
    spdlog::info("--- Inserting ICs ---");
    mb.u3.emplace<bench::IC_8088>();
    mb.u11.emplace<bench::IC_8284A>();
    mb.u6.emplace<bench::IC_8288>();
    mb.u26.emplace<bench::IC_74S175>();
    mb.u2.emplace<bench::IC_8259A>();
    mb.u36.emplace<bench::IC_8255A>();
    mb.u35.emplace<bench::IC_8237A>();
    mb.u34.emplace<bench::IC_8253>();
    mb.u7.emplace<bench::IC_74S373>();
    mb.u9.emplace<bench::IC_74S373>();
    mb.u10.emplace<bench::IC_74S373>();
    mb.u8.emplace<bench::IC_74S245>();
    mb.u12.emplace<bench::IC_74S245>();
    mb.u13.emplace<bench::IC_74S245>(true);
    mb.u14.emplace<bench::IC_74S245>(true);
    mb.u15.emplace<bench::IC_74S244>();
    mb.u16.emplace<bench::IC_74S244>();
    mb.u17.emplace<bench::IC_74S244>();
    mb.u27.emplace<bench::IC_74LS02>();
    mb.u84.emplace<bench::IC_74S10>();
    mb.u97.emplace<bench::IC_74S08>();
    mb.u101.emplace<bench::IC_74LS32>();

    // ROMs
    mb.u29.emplace<bench::IC_ROM_8K>("BASIC_C1", "assets/IBM 5150 - Cassette BASIC version C1.10 - U29 - 5000019.bin");
    mb.u30.emplace<bench::IC_ROM_8K>("BASIC_C2", "assets/IBM 5150 - Cassette BASIC version C1.10 - U30 - 5000021.bin");
    mb.u31.emplace<bench::IC_ROM_8K>("BASIC_C3", "assets/IBM 5150 - Cassette BASIC version C1.10 - U31 - 5000022.bin");
    mb.u32.emplace<bench::IC_ROM_8K>("BASIC_C4", "assets/IBM 5150 - Cassette BASIC version C1.10 - U32 - 5000023.bin");
    mb.u33.emplace<bench::IC_ROM_8K>("BIOS",     "assets/BIOS_IBM5150_27OCT82_1501476_U33.BIN");

    // --- Power on: VCC goes High, 8284A starts oscillating ---
    spdlog::info("--- Power on ---");
    mb.power_on();

    // Let the oscillator spin for a moment.
    spdlog::info("Letting oscillator run for 2s...");
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

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

    // --- 8253 PIT signal check ---
    spdlog::info("--- 8253 PIT signals ---");
    auto pit_out0 = dmm.probe("U34", 10);
    auto pit_out1 = dmm.probe("U34", 13);
    auto pit_out2 = dmm.probe("U34", 17);
    auto pit_clk0 = dmm.probe("U34", 9);
    auto pit_gate0 = dmm.probe("U34", 11);
    auto pit_gate2 = dmm.probe("U34", 16);

    if (pit_out0)  spdlog::info("  OUT0  (U34.10): {} -- {}", pit_out0->name(), level_str(pit_out0));
    if (pit_out1)  spdlog::info("  OUT1  (U34.13): {} -- {}", pit_out1->name(), level_str(pit_out1));
    if (pit_out2)  spdlog::info("  OUT2  (U34.17): {} -- {}", pit_out2->name(), level_str(pit_out2));
    if (pit_clk0)  spdlog::info("  CLK0  (U34.9):  {} -- {}", pit_clk0->name(), level_str(pit_clk0));
    if (pit_gate0) spdlog::info("  GATE0 (U34.11): {} -- {}", pit_gate0->name(), level_str(pit_gate0));
    if (pit_gate2) spdlog::info("  GATE2 (U34.16): {} -- {}", pit_gate2->name(), level_str(pit_gate2));

    // --- 8259A PIC signal check ---
    spdlog::info("--- 8259A PIC signals ---");
    auto pic_int_sig  = dmm.probe("U2", 17);
    auto pic_inta_sig = dmm.probe("U2", 26);
    auto pic_cs_sig   = dmm.probe("U2", 1);
    auto pic_ir0_sig  = dmm.probe("U2", 18);
    auto pic_ir1_sig  = dmm.probe("U2", 19);

    if (pic_int_sig)  spdlog::info("  INT   (U2.17):  {} -- {}", pic_int_sig->name(), level_str(pic_int_sig));
    if (pic_inta_sig) spdlog::info("  ~INTA (U2.26):  {} -- {}", pic_inta_sig->name(), level_str(pic_inta_sig));
    if (pic_cs_sig)   spdlog::info("  ~CS   (U2.1):   {} -- {}", pic_cs_sig->name(), level_str(pic_cs_sig));
    if (pic_ir0_sig)  spdlog::info("  IR0   (U2.18):  {} -- {}", pic_ir0_sig->name(), level_str(pic_ir0_sig));
    if (pic_ir1_sig)  spdlog::info("  IR1   (U2.19):  {} -- {}", pic_ir1_sig->name(), level_str(pic_ir1_sig));

    // --- 8255A PPI signal check ---
    spdlog::info("--- 8255A PPI signals ---");
    auto ppi_cs_sig  = dmm.probe("U36", 6);
    auto ppi_pb0_sig = dmm.probe("U36", 18);  // speaker gate
    auto ppi_pb1_sig = dmm.probe("U36", 19);  // speaker data
    auto ppi_pb7_sig = dmm.probe("U36", 25);  // keyboard clear
    auto ppi_pc5_sig = dmm.probe("U36", 12);  // T/C2 out
    auto ppi_pc7_sig = dmm.probe("U36", 10);  // parity check

    if (ppi_cs_sig)  spdlog::info("  ~CS   (U36.6):  {} -- {}", ppi_cs_sig->name(), level_str(ppi_cs_sig));
    if (ppi_pb0_sig) spdlog::info("  PB0   (U36.18): {} -- {}", ppi_pb0_sig->name(), level_str(ppi_pb0_sig));
    if (ppi_pb1_sig) spdlog::info("  PB1   (U36.19): {} -- {}", ppi_pb1_sig->name(), level_str(ppi_pb1_sig));
    if (ppi_pb7_sig) spdlog::info("  PB7   (U36.25): {} -- {}", ppi_pb7_sig->name(), level_str(ppi_pb7_sig));
    if (ppi_pc5_sig) spdlog::info("  PC5   (U36.12): {} -- {}", ppi_pc5_sig->name(), level_str(ppi_pc5_sig));
    if (ppi_pc7_sig) spdlog::info("  PC7   (U36.10): {} -- {}", ppi_pc7_sig->name(), level_str(ppi_pc7_sig));

    // --- 8237A DMA signal check ---
    spdlog::info("--- 8237A DMA signals ---");
    auto dma_hrq_sig   = dmm.probe("U35", 10);
    auto dma_hlda_sig  = dmm.probe("U35", 7);
    auto dma_dreq0_sig = dmm.probe("U35", 19);
    auto dma_dack0_sig = dmm.probe("U35", 25);
    auto dma_cs_sig    = dmm.probe("U35", 11);

    if (dma_hrq_sig)   spdlog::info("  HRQ    (U35.10): {} -- {}", dma_hrq_sig->name(), level_str(dma_hrq_sig));
    if (dma_hlda_sig)  spdlog::info("  HLDA   (U35.7):  {} -- {}", dma_hlda_sig->name(), level_str(dma_hlda_sig));
    if (dma_dreq0_sig) spdlog::info("  DREQ0  (U35.19): {} -- {}", dma_dreq0_sig->name(), level_str(dma_dreq0_sig));
    if (dma_dack0_sig) spdlog::info("  ~DACK0 (U35.25): {} -- {}", dma_dack0_sig->name(), level_str(dma_dack0_sig));
    if (dma_cs_sig)    spdlog::info("  ~CS    (U35.11): {} -- {}", dma_cs_sig->name(), level_str(dma_cs_sig));

    spdlog::info("PSU POWER_GOOD: {}", mb.psu.power_good.level() == bench::Level::High ? "YES" : "NO");

    // Power off. VCC drop causes IC run() loops to exit.
    // Threads join via jthread destructor when Sockets destruct
    // (Signals outlive Sockets per declaration order in Motherboard).
    mb.power_off();

    spdlog::info("Done.");
    return 0;
}
