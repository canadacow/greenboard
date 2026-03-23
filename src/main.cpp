// bench -- IBM PC 5150 motherboard simulator
//
// Wires the full motherboard (board_wiring.h), plugs in ISA cards
// (FDC, MDA, keyboard), powers on, and lets the BIOS POST run.
// The CPU starts executing at F000:FFF0 (the real reset vector).

#include "board/board_wiring.h"
#include "isa/isa_testcard.h"
#include "isa/isa_fdc.h"
#include "isa/isa_mda.h"
#include "display/mda_display.h"
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

    // --- ISA cards ---

    // J1: Test card (I/O ports 0x80-0xFF, DMA channels 1+3)
    ISA_TestCard testcard;
    testcard.set_dma_channels(0x0A);
    testcard.set_irq_lines(0xBC);
    testcard.install(board.isa_slots[0]);

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
    fdc.install(board.isa_slots[1]);

    // J3: MDA card (4KB framebuffer at 0xB0000, I/O 0x3B0-0x3BB)
    ISA_MDA mda;
    mda.install(board.isa_slots[2]);

    // --- Scheduler ---
    Scheduler scheduler;
    Signal::set_scheduler(&scheduler);
    board.register_all(scheduler);
    scheduler.register_callback(&testcard);
    scheduler.register_callback(&fdc);
    scheduler.register_callback(&mda);

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

    // --- MDA display (render thread, reads framebuffer directly) ---
    MdaDisplay mda_display;
    mda_display.start(mda.framebuffer(), &board.clk_gen->clk_cycles_ref(), &scheduler);

    // --- Power on ---
    spdlog::info("=== Power on ===");
    spdlog::info("SW1: 0x{:02X}  SW2: 0x{:02X}",
                 board.sw1_ic.value(), board.sw2_ic.value());

    board.clk_gen->power_on();
    board.clk_gen->psu_power_on();

    // Run until the CPU halts or the display window is closed.
    while (!board.cpu->halted() && mda_display.running())
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // --- Power off ---
    spdlog::info("=== Power off ===");
    board.clk_gen->psu_power_off();
    board.clk_gen->power_off();
    mda_display.stop();

    spdlog::info("CLK cycles: {}", board.clk_gen->clk_cycles());
    spdlog::info("Done.");
    return 0;
}
