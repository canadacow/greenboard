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
#include "isa/isa_cga.h"
#include "isa/isa_ram.h"
#include "display/renderer.h"
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

// Display card selection: uncomment ONE
//#define DISPLAY_MDA   // MDA 80x25 monochrome
#define DISPLAY_CGA   // CGA color

int main() {
    spdlog::set_level(spdlog::level::info);
    spdlog::info("bench -- IBM PC 5150 motherboard simulator");

    // --- Wire the motherboard ---
    //std::string bios_path  = "assets/BIOS_IBM5150_27OCT82_1501476_U33.BIN";
    //std::string bios_path = "docs/Troubleshooting/SuperSoft Landmark Diagnostic BIOS/5150 or 5160 _ 2764 _ 8KB.BIN";
    std::string bios_path = "assets/GLABIOS_0.4.1_8P.ROM";
    //std::string bios_path = "docs/Anonymous BIOS/pcxtbios25/PCXTBIOS.BIN";
    std::string basic_u29  = ""; //"assets/IBM 5150 - Cassette BASIC version C1.10 - U29 - 5000019.bin";
    std::string basic_u30  = ""; // "assets/IBM 5150 - Cassette BASIC version C1.10 - U30 - 5000021.bin";
    std::string basic_u31  = ""; // "assets/IBM 5150 - Cassette BASIC version C1.10 - U31 - 5000022.bin";
    std::string basic_u32  = ""; // "assets/IBM 5150 - Cassette BASIC version C1.10 - U32 - 5000023.bin";

    Board board;
    board.wire(bios_path, basic_u29, basic_u30, basic_u31, basic_u32);

    // --- ISA bus + cards ---
    ISA_Bus isa_bus;
    isa_bus.install(board.isa_slots[0]);

    // J1: Test card (I/O ports 0x80-0xFF, DMA channels 1+3)
    ISA_TestCard testcard;
    isa_bus.insert_card(0, &testcard, 0x0A, 0xBC);

    // J2: Floppy disk controller (DMA channel 2, IRQ 6)
    //std::string dos_disk = "assets/IBM DOS 3.30 360K Disks - Disk 01.img";
    std::string dos_disk = "assets/50boot.img";
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
    board.add_floppy_drives(2);

    std::unique_ptr<ISA_CGA> cga = nullptr;
    std::unique_ptr<ISA_MDA> mda = nullptr;

    // J3: Display card
#ifdef DISPLAY_CGA
    cga = std::make_unique<ISA_CGA>();
    cga->set_clk_counter(&board.clk_gen->clk_cycles_ref());
    isa_bus.insert_card(2, cga.get());
    board.set_video(Board::CGA_80);
#else
    mda = std::make_unique<ISA_MDA>();
    isa_bus.insert_card(2, mda.get());
    board.set_video(Board::MDA);
#endif

    // J4: RAM expansion (256KB planar + expansion = total)
    std::unique_ptr<ISA_RAM> ram_exp = nullptr;
    
#if 1
    constexpr uint32_t ExpansionRamSize = 384;

    ram_exp = std::make_unique<ISA_RAM>(0x40000, ExpansionRamSize * 1024);
    isa_bus.insert_card(3, ram_exp.get());
    board.add_expansion_kb(ExpansionRamSize);
#endif

    // All cards announced -- compute DIP switches from hardware state.
    board.compute_switches();

    // --- Scheduler ---
    Scheduler scheduler;
    Signal::set_scheduler(&scheduler);
    board.register_all(scheduler);
    scheduler.register_callback(&isa_bus);

    // --- Keyboard ---
    testcard.set_kbd_ready_signal(&board.kbd_ready);
    testcard.set_kbd_ack_signal(&board.kbd_ack);
    testcard.set_hostfs_root("D:/dos");
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

    if (ram_exp)
    {
        // Expansion RAM: 40000-9FFFF (384KB ISA RAM card)
        memview.map(ram_exp->base(), ram_exp->size(), [&](uint32_t addr) -> uint8_t {
            return ram_exp->data()[addr - ram_exp->base()];
            });
    }

    // Display framebuffer
    if (cga)
    {
        memview.map(ISA_CGA::FB_BASE, ISA_CGA::FB_SIZE, [&](uint32_t addr) -> uint8_t {
            return cga->vram()[addr - ISA_CGA::FB_BASE];
            });
    }

    if (mda)
    {
        memview.map(ISA_MDA::FB_BASE, ISA_MDA::FB_SIZE, [&](uint32_t addr) -> uint8_t {
            return mda->framebuffer()[addr - ISA_MDA::FB_BASE];
            });
    }

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

    // --- Renderer (render thread, reads framebuffer directly) ---
    Renderer renderer;
    scheduler.set_cpu(board.cpu);
    board.cpu->debug_peek_ = [&](uint32_t addr) -> uint8_t { return memview.read(addr); };
    board.cpu->debug_bus_state_ = [&]() -> std::string {
        auto d = [](IC_74S245::Driving v) { return v == IC_74S245::Driving::A ? 'A' : v == IC_74S245::Driving::B ? 'B' : '-'; };
        auto rv = [](auto* arr) { uint8_t v=0; for(int i=0;i<8;i++) if(arr[i]->level()==bench::Level::High) v|=(1<<i); return v; };
        auto rs = [&](auto& arr) { uint8_t v=0; for(int i=0;i<8;i++) if(arr[i].level()==bench::Level::High) v|=(1<<i); return v; };
        return fmt::format("~MW={} ~MR={} AEN={} CEN={} HLDA={} inh={} bh={} cyc={} bus={:05X} AD={:02X} D={:02X} XD={:02X} MD={:02X} U8={}{} U13={}{} U12={}{} U14={}{} dma={} mwp={} cmwp={} dmwp={} cmrp={} dmrp={} P={}",
            (int)board.memw.level(), (int)board.memr.level(), (int)board.isa_aen.level(),
            (int)board.aen_brd.level(), (int)board.holda.level(),
            board.bc->inhibited(), board.bc->bus_hold_count(), board.bc->cycle_type(),
            SignalPool::bus_address,
            rs(board.ad), rv(board.d_arr), rv(board.xd_arr), rv(board.md_arr),
            d(board.xcvr->driving()), d(board.xcvr->pending()),
            d(board.xcvr13_ic->driving()), d(board.xcvr13_ic->pending()),
            d(board.mem_xcvr->driving()), d(board.mem_xcvr->pending()),
            d(board.xcvr14_ic->driving()), d(board.xcvr14_ic->pending()),
            (int)board.dma_ic->state(),
            isa_bus.mem_write_pending(), (int)isa_bus.cpu_memw_prev(), (int)isa_bus.dma_memw_prev(),
            (int)isa_bus.cpu_memr_prev(), (int)isa_bus.dma_memr_prev(),
            scheduler.current_perm());
    };

    renderer.set_disk_a_path(dos_disk);

    if (cga)
    {
        renderer.start(nullptr, &board.clk_gen->clk_cycles_ref(),
            &scheduler, board.cpu, &memview, board.dma_ic, nullptr,
            &bus_probe, &keyboard, &fdc, cga.get());
    }
    else if (mda)
    {
        renderer.start(mda->framebuffer(), &board.clk_gen->clk_cycles_ref(),
            &scheduler, board.cpu, &memview, board.dma_ic, mda.get(),
            &bus_probe, &keyboard, &fdc, nullptr);
    }

    // Bind board traces to live simulation signals.
    renderer.bind_board_signals(board.brd_net_map());

    // Start paused. Pre-set the debugger view to the reset vector.
    scheduler.pause();

    // --- Power on ---
    spdlog::info("=== Power on ===");
    spdlog::info("SW1: 0x{:02X}  SW2: 0x{:02X}",
                 board.sw1_ic.value(), board.sw2_mux.value());
    spdlog::info("Hardware: {} floppy drives, video={}, {}KB expansion",
                 board.floppy_drives_, static_cast<int>(board.video_), board.expansion_kb_);

    board.clk_gen->power_on();
    board.clk_gen->psu_power_on();

    // Run until the display window is closed.
    // CPU HLT pauses the scheduler but keeps the window alive for inspection.
    while (renderer.running())
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // --- Power off ---
    spdlog::info("=== Power off ===");
    scheduler.resume();  // unblock pause_gate so the clock thread can exit
    board.clk_gen->psu_power_off();
    board.clk_gen->power_off();
    renderer.stop();

    spdlog::info("CLK cycles: {}", board.clk_gen->clk_cycles());
    spdlog::info("Done.");
    return 0;
}
