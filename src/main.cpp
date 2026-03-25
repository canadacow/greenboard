// bench -- IBM PC 5150 motherboard simulator
//
// Wires the full motherboard (board_wiring.h), plugs in ISA cards
// (FDC, MDA, keyboard), powers on, and lets the BIOS POST run.
// The CPU starts executing at F000:FFF0 (the real reset vector).

#include "board/board_wiring.h"
#include "isa/isa_bus.h"
#include "isa/isa_testcard.h"
#include "isa/isa_fdc.h"
#include "isa/isa_mda.h"
#include "display/mda_display.h"
#include "debug/memory_view.h"
#include "test/test_keyboard.h"
#include "core/signal.h"
#include "core/scheduler.h"
#include <spdlog/spdlog.h>
#include <fstream>
#include <vector>
#include <thread>
#include <chrono>

using namespace bench;

int main() {
    spdlog::set_level(spdlog::level::info);
    spdlog::info("bench -- IBM PC 5150 motherboard simulator");

    // --- Wire the motherboard ---
    std::string bios_path  = "assets/BIOS_IBM5150_27OCT82_1501476_U33.BIN";
    //std::string bios_path = "docs/Troubleshooting/SuperSoft Landmark Diagnostic BIOS/5150 or 5160 _ 2764 _ 8KB.BIN";
    //std::string bios_path = "docs/Anonymous BIOS/pcxtbios25/PCXTBIOS.BIN";
    std::string basic_u29  = "assets/IBM 5150 - Cassette BASIC version C1.10 - U29 - 5000019.bin";
    std::string basic_u30  = "assets/IBM 5150 - Cassette BASIC version C1.10 - U30 - 5000021.bin";
    std::string basic_u31  = "assets/IBM 5150 - Cassette BASIC version C1.10 - U31 - 5000022.bin";
    std::string basic_u32  = "assets/IBM 5150 - Cassette BASIC version C1.10 - U32 - 5000023.bin";

    Board board;
    board.wire(bios_path, basic_u29, basic_u30, basic_u31, basic_u32);

    // --- ISA bus + cards ---
    ISA_Bus isa_bus;
    isa_bus.install(board.isa_slots[0]);

    // J1: Test card (I/O ports 0x80-0xFF, DMA channels 1+3)
    ISA_TestCard testcard;
    isa_bus.insert_card(0, &testcard, 0x0A, 0xBC);

    // J2: Floppy disk controller (DMA channel 2, IRQ 6)
    std::string dos_disk = "assets/IBM DOS 3.30 360K Disks - Disk 01.img";
    std::vector<uint8_t> floppy_img;
    {
        std::ifstream f(dos_disk, std::ios::binary | std::ios::ate);
        if (f) {
            auto sz = f.tellg();
            floppy_img.resize(static_cast<size_t>(sz));
            f.seekg(0);
            f.read(reinterpret_cast<char*>(floppy_img.data()), sz);
            spdlog::info("[FDC] loaded {} bytes from {}", floppy_img.size(), dos_disk);
        } else {
            spdlog::warn("[FDC] disk image not found: {}", dos_disk);
        }
    }
    ISA_FloppyController fdc(std::move(floppy_img), 9, 2);
    isa_bus.insert_card(1, &fdc, 0x04, 0x40);

    // J3: MDA card (4KB framebuffer at 0xB0000, I/O 0x3B0-0x3BB)
    ISA_MDA mda;
    isa_bus.insert_card(2, &mda);

    // --- Scheduler ---
    Scheduler scheduler;
    Signal::set_scheduler(&scheduler);
    board.register_all(scheduler);
    scheduler.register_callback(&isa_bus);

    // --- Keyboard ---
    testcard.set_kbd_ready_signal(&board.kbd_ready);
    testcard.set_kbd_ack_signal(&board.kbd_ack);
    TestKeyboard keyboard;
    testcard.set_keyboard(&keyboard);
    {
        Signal* pa_ptrs[8];
        for (int i = 0; i < 8; ++i) pa_ptrs[i] = &board.ppi_pa[i];
        keyboard.connect(pa_ptrs, board.irq1, board.ppi_pb[6],
                         board.ppi_pb[7], board.kbd_ready, board.kbd_ack);
    }
    scheduler.register_callback(&keyboard);

    scheduler.resolve();

    // --- Speaker (Beep() on PPI port B speaker off transition) ---
    board.ppi_ic->set_speaker_source(board.pit_ic, &board.clk_gen->clk_cycles_ref());

    spdlog::set_level(spdlog::level::info);

    // --- Debugger memory view (unified 20-bit address space) ---
    MemoryView memview;

    // DRAM: 00000-3FFFF (256KB, through 74S158 address inversion)
    memview.map(0x00000, 0x40000, [&](uint32_t addr) -> uint8_t {
        uint8_t row = static_cast<uint8_t>(~(addr & 0xFF));
        uint8_t col = static_cast<uint8_t>(~((addr >> 8) & 0xFF));
        uint32_t bank = (addr >> 16) & 3;
        uint32_t xlat = (bank << 16) | (static_cast<uint32_t>(row) << 8) | col;
        return board.dram.data()[xlat];
    });

    // MDA framebuffer: B0000-B0FFF (4KB)
    memview.map(ISA_MDA::FB_BASE, ISA_MDA::FB_SIZE, [&](uint32_t addr) -> uint8_t {
        return mda.framebuffer()[addr - ISA_MDA::FB_BASE];
    });

    // ROM: F6000-FFFFF (5 banks x 8KB)
    memview.map(0xF6000, 5 * 8192, [&](uint32_t addr) -> uint8_t {
        uint32_t off = addr - 0xF6000;
        return board.rom.bank_data(off / 8192)[off % 8192];
    });

    // --- Bus probe (pool indices for bus analyzer) ---
    auto pidx = [](Signal& s) { return (int)s.pin().idx; };
    BusProbe bus_probe;
    bus_probe.ad = board.ad_block_;  bus_probe.d = board.d_block_;
    bus_probe.xd = board.xd_block_; bus_probe.la = board.la_block_;
    bus_probe.md = board.md_block_;
    bus_probe.ale = pidx(board.ale);   bus_probe.den = pidx(board.den);
    bus_probe.dtr = pidx(board.dtr);
    bus_probe.memr = pidx(board.memr); bus_probe.memw = pidx(board.memw);
    bus_probe.ior = pidx(board.ior_sig); bus_probe.iow = pidx(board.iow_sig);
    bus_probe.ready = pidx(board.ready); bus_probe.clk = pidx(board.clk);
    bus_probe.reset = pidx(board.reset);
    bus_probe.hrq = pidx(board.hrq);   bus_probe.holda = pidx(board.holda);
    bus_probe.aen_brd = pidx(board.aen_brd); bus_probe.aen_bar = pidx(board.aen_bar);
    bus_probe.s0 = pidx(board.s0); bus_probe.s1 = pidx(board.s1); bus_probe.s2 = pidx(board.s2);
    bus_probe.dack0 = pidx(board.dack0_brd); bus_probe.dack1 = pidx(board.dack1);
    bus_probe.dack2 = pidx(board.dack2);     bus_probe.dack3 = pidx(board.dack3);
    bus_probe.drq0 = pidx(board.drq0); bus_probe.drq1 = pidx(board.drq1);
    bus_probe.drq2 = pidx(board.drq2); bus_probe.drq3 = pidx(board.drq3);
    bus_probe.intr = pidx(board.intr); bus_probe.nmi = pidx(board.nmi);

    // --- MDA display (render thread, reads framebuffer directly) ---
    MdaDisplay mda_display;
    scheduler.set_cpu(board.cpu);
    mda_display.start(mda.framebuffer(), &board.clk_gen->clk_cycles_ref(),
                       &scheduler, board.cpu, &memview, board.dma_ic, &mda,
                       &bus_probe);

    // Start paused. Pre-set the debugger view to the reset vector.
    scheduler.pause();

    // --- Power on ---
    spdlog::info("=== Power on ===");
    spdlog::info("SW1: 0x{:02X}  SW2: 0x{:02X}",
                 board.sw1_ic.value(), board.sw2_ic.value());

    board.clk_gen->power_on();
    board.clk_gen->psu_power_on();

    // Run until the display window is closed.
    // CPU HLT pauses the scheduler but keeps the window alive for inspection.
    while (mda_display.running())
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // --- Power off ---
    spdlog::info("=== Power off ===");
    scheduler.resume();  // unblock pause_gate so the clock thread can exit
    board.clk_gen->psu_power_off();
    board.clk_gen->power_off();
    mda_display.stop();

    spdlog::info("CLK cycles: {}", board.clk_gen->clk_cycles());
    spdlog::info("Done.");
    return 0;
}
