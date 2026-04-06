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
#include "audio/pc_speaker.h"
#include "audio/speaker_driver.h"
#include "core/signal.h"
#include "core/scheduler.h"
#include "core/save_state.h"
#include <cereal/archives/binary.hpp>
#include <spdlog/spdlog.h>
#include <fstream>
#include <vector>
#include <thread>
#include <chrono>
#include <memory>

using namespace bench;

// Display card selection: uncomment ONE
//#define DISPLAY_MDA   // MDA 80x25 monochrome
#define DISPLAY_CGA   // CGA color

// ========================================================================
// System -- owns the entire simulation, can be rebuilt from a save file.
// ========================================================================

struct SystemConfig {
    std::string bios_path = "assets/GLABIOS_0.4.1_8P.ROM";
    std::string basic_u29;
    std::string basic_u30;
    std::string basic_u31;
    std::string basic_u32;
    std::string dos_disk = "assets/50boot.img";
    std::string hostfs_root = "D:/dos";
    bool use_cga = true;
    uint32_t expansion_kb = 384;
};

struct System {
    std::unique_ptr<Board> board;
    std::unique_ptr<Scheduler> scheduler;
    std::unique_ptr<ISA_Bus> isa_bus;
    std::unique_ptr<ISA_TestCard> testcard;
    std::unique_ptr<ISA_FloppyController> fdc;
    std::unique_ptr<ISA_CGA> cga;
    std::unique_ptr<ISA_MDA> mda;
    std::unique_ptr<ISA_RAM> ram_exp;
    std::unique_ptr<TestKeyboard> keyboard;
    std::unique_ptr<PCSpeaker> pc_speaker;
    std::unique_ptr<SpeakerDriver> speaker_driver;
    MemoryView memview;
    BusProbe bus_probe;
    SystemConfig config;

    void power_off() {
        if (!board || !board->clk_gen) return;
        scheduler->resume();
        board->clk_gen->psu_power_off();
        board->clk_gen->power_off();
        if (pc_speaker) pc_speaker->shutdown();
    }
};

static std::vector<uint8_t> load_floppy_image(const std::string& path) {
    std::vector<uint8_t> img;
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (f) {
        auto sz = f.tellg();
        img.resize(static_cast<size_t>(sz));
        f.seekg(0);
        f.read(reinterpret_cast<char*>(img.data()), sz);
        spdlog::info("[FDC] loaded {} bytes from {}", img.size(), path);
    } else {
        spdlog::warn("[FDC] disk image not found: {}", path);
    }
    return img;
}

static System build_system(const SystemConfig& cfg,
                           cereal::BinaryInputArchive* ar = nullptr) {
    System sys;
    sys.config = cfg;

    // --- Board ---
    sys.board = std::make_unique<Board>();
    sys.board->wire(cfg.bios_path, cfg.basic_u29, cfg.basic_u30,
                    cfg.basic_u31, cfg.basic_u32, ar);

    // --- ISA bus + cards ---
    sys.isa_bus = std::make_unique<ISA_Bus>();
    sys.isa_bus->install(sys.board->isa_slots[0]);

    // J1: Test card
    sys.testcard = std::make_unique<ISA_TestCard>();
    sys.isa_bus->insert_card(0, sys.testcard.get(), 0x0A, 0xBC);

    // J2: Floppy disk controller
    auto floppy_img = load_floppy_image(cfg.dos_disk);
    sys.fdc = std::make_unique<ISA_FloppyController>(std::move(floppy_img), 9, 2);
    sys.isa_bus->insert_card(1, sys.fdc.get(), 0x04, 0x40);
    sys.board->add_floppy_drives(2);

    // J3: Display card
#ifdef DISPLAY_CGA
    if (cfg.use_cga) {
        sys.cga = std::make_unique<ISA_CGA>();
        sys.cga->set_clk_counter(&sys.board->clk_gen->clk_cycles_ref());
        sys.isa_bus->insert_card(2, sys.cga.get());
        sys.board->set_video(Board::CGA_80);
    } else
#endif
    {
        sys.mda = std::make_unique<ISA_MDA>();
        sys.isa_bus->insert_card(2, sys.mda.get());
        sys.board->set_video(Board::MDA);
    }

    // J4: RAM expansion
    if (cfg.expansion_kb > 0) {
        sys.ram_exp = std::make_unique<ISA_RAM>(0x40000, cfg.expansion_kb * 1024);
        sys.isa_bus->insert_card(3, sys.ram_exp.get());
        sys.board->add_expansion_kb(cfg.expansion_kb);
    }

    sys.board->compute_switches();

    // --- Scheduler ---
    sys.scheduler = std::make_unique<Scheduler>();
    Signal::set_scheduler(sys.scheduler.get());
    sys.board->register_all(*sys.scheduler);
    sys.scheduler->register_callback(sys.isa_bus.get());

    // --- Keyboard ---
    sys.testcard->set_kbd_ready_signal(&sys.board->kbd_ready);
    sys.testcard->set_kbd_ack_signal(&sys.board->kbd_ack);
    sys.testcard->set_hostfs_root(cfg.hostfs_root);
    sys.keyboard = std::make_unique<TestKeyboard>();
    sys.testcard->set_keyboard(sys.keyboard.get());
    {
        Signal* pa_ptrs[8];
        for (int i = 0; i < 8; ++i) pa_ptrs[i] = &sys.board->ppi_pa[i];
        sys.keyboard->connect(pa_ptrs, sys.board->irq1, sys.board->ppi_pb[6],
                              sys.board->ppi_pb[7], sys.board->kbd_ready,
                              sys.board->kbd_ack);
    }
    sys.scheduler->register_callback(sys.keyboard.get());

    // --- PC Speaker ---
    sys.pc_speaker = std::make_unique<PCSpeaker>();
    sys.pc_speaker->init();
    sys.speaker_driver = std::make_unique<SpeakerDriver>();
    sys.speaker_driver->connect(sys.board->spkr_mix, sys.pc_speaker.get());
    sys.scheduler->register_callback(sys.speaker_driver.get());

    sys.scheduler->resolve();

    return sys;
}

// Must be called AFTER sys is in its final memory location (no more moves).
static void bind_debug(System& sys) {
    sys.memview = MemoryView{};
    sys.memview.map(0x00000, 0x40000, [&sys](uint32_t addr) -> uint8_t {
        uint8_t row = static_cast<uint8_t>(~(addr & 0xFF));
        uint8_t col = static_cast<uint8_t>(~((addr >> 8) & 0xFF));
        uint32_t bank = (addr >> 16) & 3;
        uint32_t xlat = (bank << 16) | (static_cast<uint32_t>(row) << 8) | col;
        return sys.board->dram.data()[xlat];
    });
    if (sys.ram_exp) {
        sys.memview.map(sys.ram_exp->base(), sys.ram_exp->size(),
            [&sys](uint32_t addr) -> uint8_t {
                return sys.ram_exp->data()[addr - sys.ram_exp->base()];
            });
    }
    if (sys.cga) {
        sys.memview.map(ISA_CGA::FB_BASE, ISA_CGA::FB_SIZE,
            [&sys](uint32_t addr) -> uint8_t {
                return sys.cga->vram()[addr - ISA_CGA::FB_BASE];
            });
    }
    if (sys.mda) {
        sys.memview.map(ISA_MDA::FB_BASE, ISA_MDA::FB_SIZE,
            [&sys](uint32_t addr) -> uint8_t {
                return sys.mda->framebuffer()[addr - ISA_MDA::FB_BASE];
            });
    }
    sys.memview.map(0xF6000, 5 * 8192, [&sys](uint32_t addr) -> uint8_t {
        uint32_t off = addr - 0xF6000;
        return sys.board->rom.bank_data(off / 8192)[off % 8192];
    });

    auto pidx = [](Signal& s) { return (int)s.pin().idx; };
    auto& bp = sys.bus_probe;
    auto& b = *sys.board;
    bp.ad = b.ad_block_;  bp.d = b.d_block_;
    bp.xd = b.xd_block_; bp.la = b.la_block_;
    bp.md = b.md_block_;
    bp.ale = pidx(b.ale);   bp.den = pidx(b.den);   bp.dtr = pidx(b.dtr);
    bp.memr = pidx(b.memr); bp.memw = pidx(b.memw);
    bp.ior = pidx(b.ior_sig); bp.iow = pidx(b.iow_sig);
    bp.ready = pidx(b.ready); bp.clk = pidx(b.clk); bp.reset = pidx(b.reset);
    bp.hrq = pidx(b.hrq);   bp.holda = pidx(b.holda);
    bp.aen_brd = pidx(b.aen_brd); bp.aen_bar = pidx(b.aen_bar);
    bp.s0 = pidx(b.s0); bp.s1 = pidx(b.s1); bp.s2 = pidx(b.s2);
    bp.dack0 = pidx(b.dack0_brd); bp.dack1 = pidx(b.dack1);
    bp.dack2 = pidx(b.dack2);     bp.dack3 = pidx(b.dack3);
    bp.drq0 = pidx(b.drq0); bp.drq1 = pidx(b.drq1);
    bp.drq2 = pidx(b.drq2); bp.drq3 = pidx(b.drq3);
    bp.intr = pidx(b.intr); bp.nmi = pidx(b.nmi);

    sys.scheduler->set_cpu(sys.board->cpu);
    sys.board->cpu->debug_peek_ = [&sys](uint32_t addr) -> uint8_t {
        return sys.memview.read(addr);
    };
    sys.board->cpu->debug_bus_state_ = [&sys]() -> std::string {
        auto& b = *sys.board;
        auto d = [](IC_74S245::Driving v) {
            return v == IC_74S245::Driving::A ? 'A' : v == IC_74S245::Driving::B ? 'B' : '-';
        };
        auto rv = [](auto* arr) { uint8_t v=0; for(int i=0;i<8;i++) if(arr[i]->level()==bench::Level::High) v|=(1<<i); return v; };
        auto rs = [&b](auto& arr) { uint8_t v=0; for(int i=0;i<8;i++) if(arr[i].level()==bench::Level::High) v|=(1<<i); return v; };
        return fmt::format("~MW={} ~MR={} AEN={} CEN={} HLDA={} inh={} bh={} cyc={} bus={:05X} AD={:02X} D={:02X} XD={:02X} MD={:02X} U8={}{} U13={}{} U12={}{} U14={}{} dma={} mwp={} cmwp={} dmwp={} cmrp={} dmrp={} P={}",
            (int)b.memw.level(), (int)b.memr.level(), (int)b.isa_aen.level(),
            (int)b.aen_brd.level(), (int)b.holda.level(),
            b.bc->inhibited(), b.bc->bus_hold_count(), b.bc->cycle_type(),
            SignalPool::bus_address,
            rs(b.ad), rv(b.d_arr), rv(b.xd_arr), rv(b.md_arr),
            d(b.xcvr->driving()), d(b.xcvr->pending()),
            d(b.xcvr13_ic->driving()), d(b.xcvr13_ic->pending()),
            d(b.mem_xcvr->driving()), d(b.mem_xcvr->pending()),
            d(b.xcvr14_ic->driving()), d(b.xcvr14_ic->pending()),
            (int)b.dma_ic->state(),
            sys.isa_bus->mem_write_pending(), (int)sys.isa_bus->cpu_memw_prev(),
            (int)sys.isa_bus->dma_memw_prev(),
            (int)sys.isa_bus->cpu_memr_prev(), (int)sys.isa_bus->dma_memr_prev(),
            sys.scheduler->current_perm());
    };
}

static void start_renderer(Renderer& renderer, System& sys) {
    bench::SystemInfo sys_info;
    sys_info.expansion_kb = sys.board->expansion_kb_;
    for (int i = 0; i < 5; i++) {
        sys_info.slots[i].ref = sys.board->isa_slots[i].ref();
        if (auto* c = sys.isa_bus->card(i))
            sys_info.slots[i].card = c->card_name();
    }

    if (sys.cga) {
        renderer.start(nullptr, &sys.board->clk_gen->clk_cycles_ref(),
            sys.scheduler.get(), sys.board->cpu, &sys.memview, sys.board->dma_ic,
            nullptr, &sys.bus_probe, sys.keyboard.get(), sys.fdc.get(),
            sys.cga.get(), sys.board->clk_gen, sys_info,
            sys.board->pic, sys.board->pit_ic);
    } else if (sys.mda) {
        renderer.start(sys.mda->framebuffer(), &sys.board->clk_gen->clk_cycles_ref(),
            sys.scheduler.get(), sys.board->cpu, &sys.memview, sys.board->dma_ic,
            sys.mda.get(), &sys.bus_probe, sys.keyboard.get(), sys.fdc.get(),
            nullptr, sys.board->clk_gen, sys_info,
            sys.board->pic, sys.board->pit_ic);
    }

    renderer.bind_board_signals(sys.board->brd_net_map());
}

// ========================================================================
// main
// ========================================================================

int main() {
    spdlog::set_level(spdlog::level::info);
    spdlog::info("bench -- IBM PC 5150 motherboard simulator");

    SystemConfig cfg;
#ifdef DISPLAY_CGA
    cfg.use_cga = true;
#else
    cfg.use_cga = false;
#endif

    auto sys = build_system(cfg);
    bind_debug(sys);

    Renderer renderer;
    renderer.set_disk_a_path(cfg.dos_disk);
    start_renderer(renderer, sys);

    sys.scheduler->pause();

    // --- Power on ---
    spdlog::info("=== Power on ===");
    spdlog::info("SW1: 0x{:02X}  SW2: 0x{:02X}",
                 sys.board->sw1_ic.value(), sys.board->sw2_mux.value());
    spdlog::info("Hardware: {} floppy drives, video={}, {}KB expansion",
                 sys.board->floppy_drives_, static_cast<int>(sys.board->video_),
                 sys.board->expansion_kb_);

    sys.board->clk_gen->power_on();
    sys.board->clk_gen->psu_power_on();

    // Run until the display window is closed.
    while (renderer.running()) {
        // Check for load request from renderer
        std::string load_path = renderer.take_pending_load();
        if (!load_path.empty()) {
            spdlog::info("[SaveState] load requested: {}", load_path);

            // Stop everything
            renderer.stop();
            sys.power_off();

            // Reset signal pool (slot 0 is the null/dummy slot, keep it)
            for (int i = 1; i < SignalPool::count; ++i)
                SignalPool::levels[i] = Level::HiZ;
            SignalPool::count = 1;

            // Rebuild from archive
            std::ifstream ifs(load_path, std::ios::binary);
            cereal::BinaryInputArchive ar(ifs);
            sys = build_system(cfg, &ar);
            bind_debug(sys);

            // Restart renderer with new pointers
            renderer.~Renderer();
            new (&renderer) Renderer{};
            renderer.set_disk_a_path(cfg.dos_disk);
            start_renderer(renderer, sys);

            // Power on (resets all ICs to defaults), wait for clock thread
            // to finish power_on_all() and park, then overwrite with saved state.
            sys.scheduler->pause();
            sys.board->clk_gen->power_on();
            sys.board->clk_gen->psu_power_on();
            sys.scheduler->wait_until_parked();

            // Overwrite defaults with saved state
            bench::load_remaining(ar, *sys.scheduler, sys.board->cpu);
            sys.board->restore_signal_pool();
            continue;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // --- Power off ---
    spdlog::info("=== Power off ===");
    sys.power_off();
    renderer.stop();

    spdlog::info("CLK cycles: {}", sys.board->clk_gen->clk_cycles());
    spdlog::info("Done.");
    return 0;
}
