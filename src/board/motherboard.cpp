#include "board/motherboard.h"
#include "board/brd_parser.h"
#include <spdlog/spdlog.h>
#include <filesystem>

namespace bench {

// Default BRD file path (relative to working directory).
static constexpr const char* BRD_PATH = "assets/pcb/64_256KB_SYSTEM_BOARD_rev1_2a.brd";

Motherboard::Motherboard() {
    create_ram_sockets();

    // Tie MN/~MX to GND -- 8088 always in maximum mode on the 5150.
    cpu_mn_mx.drive(Level::Low);

    // Wire everything from the BRD netlist.
    wire_from_brd();

    // Set default DIP switch configuration: MDA display, 1 floppy, 64K RAM.
    sw1.set(1, true);   // Floppy drives installed
    sw1.set(2, false);  // No 8087
    sw1.set(3, true);   // 64K RAM (bits 3,4 = 11)
    sw1.set(4, true);
    sw1.set(5, true);   // MDA display (bits 5,6 = 11)
    sw1.set(6, true);
    sw1.set(7, false);  // 1 floppy drive (bits 7,8 = 00)
    sw1.set(8, false);

    // Pull-up resistors on key signal lines.
    // Data bus pull-ups (active when no driver)
    for (int i = 0; i < 8; ++i)
        pullups.push_back(PullResistor{"RP_SD" + std::to_string(i), &sd[i], Level::High});
    // IRQ pull-downs (active low when no device asserts)
    Signal* irqs[] = {&irq0, &irq1, &irq2, &irq3, &irq4, &irq5, &irq6, &irq7};
    for (int i = 0; i < 8; ++i)
        pullups.push_back(PullResistor{"RP_IRQ" + std::to_string(i), irqs[i], Level::Low});
    // DMA request pull-downs
    Signal* dreqs[] = {&dreq0, &dreq1, &dreq2, &dreq3};
    for (int i = 0; i < 4; ++i)
        pullups.push_back(PullResistor{"RP_DREQ" + std::to_string(i), dreqs[i], Level::Low});
    // I/O channel ready pull-up
    pullups.push_back(PullResistor{"RP_IOCHRDY", &rdy_wait, Level::High});
    // PIT gate0/gate1 tied high
    pullups.push_back(PullResistor{"RP_GATE0", &pit_gate0, Level::High});
    pullups.push_back(PullResistor{"RP_GATE1", &pit_gate1, Level::High});

    spdlog::info("5150 motherboard wired from BRD: {} address lines, {} data lines, 5 ISA slots",
                 sa.width(), sd.width());
}

void Motherboard::power_on() {
    psu.switch_on();
    sw1.apply();
    sw2.apply();
    for (auto& p : pullups) p.apply();
    for (auto& j : jumpers) j.apply();
    spdlog::info("5150 powered on");
}

void Motherboard::power_off() {
    psu.switch_off();
    spdlog::info("5150 powered off");
}

// =========================================================================
// RAM socket creation (4 banks x 9 chips = 36 sockets)
// =========================================================================
void Motherboard::create_ram_sockets() {
    auto make_bank = [](std::vector<Socket>& bank, int start_u, int count) {
        bank.reserve(count);
        for (int i = 0; i < count; ++i) {
            int u_num = start_u + i;
            bank.emplace_back("U" + std::to_string(u_num), "RAM_64K_X_1", 16);
        }
    };
    make_bank(ram_bank0, 37, 9);  // U37-U45
    make_bank(ram_bank1, 53, 9);  // U53-U61
    make_bank(ram_bank2, 69, 9);  // U69-U77
    make_bank(ram_bank3, 85, 9);  // U85-U93
}

// =========================================================================
// Build the net name -> Signal* mapping.
// Maps every known named signal on the motherboard to its BRD net name.
// =========================================================================
void Motherboard::build_net_map() {
    // --- Power rails ---
    net_map_["+5V"]  = &psu.vcc;
    net_map_["+12V"] = &psu.vcc_12;
    net_map_["-5V"]  = &psu.vcc_n5;
    net_map_["-12V"] = &psu.vcc_n12;
    net_map_["GND"]  = &psu.gnd;
    net_map_["PWR_GOOD"] = &psu.power_good;

    // --- Clock / oscillator ---
    net_map_["OSC"]   = &osc;
    net_map_["CLK"]   = &clk;
    net_map_["PCLK"]  = &pclk;
    net_map_["DCLK"]  = &dclk;
    net_map_["CLK88"] = &clk88;

    // --- CPU local bus (SHEET1 = CPU local signals) ---
    // Multiplexed address/data AD0-AD7
    for (int i = 0; i < 8; ++i)
        net_map_["/SHEET1/AD" + std::to_string(i)] = &cpu_ad[i];
    // Address bus A8-A19 (named A8_BUS..A19_BUS in schematic)
    for (int i = 8; i < 20; ++i)
        net_map_["/SHEET1/A" + std::to_string(i) + "_BUS"] = &cpu_a[i - 8];

    net_map_["/SHEET1/QS0"]     = &cpu_qs0;
    net_map_["/SHEET1/QS1"]     = &cpu_qs1;
    net_map_["/SHEET1/READY"]   = &ready;
    net_map_["/SHEET1/~RQ~/~GT"] = &cpu_rq_gt0;

    net_map_["~S0"]   = &cpu_s0;
    net_map_["~S1"]   = &cpu_s1;
    net_map_["~S2"]   = &cpu_s2;
    net_map_["~LOCK"] = &cpu_lock;
    net_map_["RESET"] = &cpu_reset;
    net_map_["NMI"]   = &nmi;

    // --- 8288 bus controller outputs (active on system bus) ---
    net_map_["AEN_BRD"]  = &ale;       // ALE on the ISA bus (directly from AEN_BRD net)
    // Note: 8288 internal outputs have anonymous net names (N-000xxx).
    // These get auto-created as dynamic signals and still wire correctly.

    // --- System bus (after latches/transceivers) ---
    for (int i = 0; i < 20; ++i)
        net_map_["A" + std::to_string(i)] = &sa[i];
    for (int i = 0; i < 8; ++i)
        net_map_["D" + std::to_string(i)] = &sd[i];

    // --- ISA / system control signals ---
    net_map_["~MEMR"]     = &memr;
    net_map_["~MEMW"]     = &memw;
    net_map_["~IOR"]      = &ior;
    net_map_["~IOW"]      = &iow;
    net_map_["~XMEMR"]    = &memr;  // buffered version, same signal in our model
    net_map_["~XMEMW"]    = &memw;
    net_map_["~XIOR"]     = &ior;
    net_map_["~XIOW"]     = &iow;
    net_map_["RESET_DRV"]  = &reset_drv;
    net_map_["~RESET_DRV"] = &reset_drv;
    net_map_["AEN"]        = &aen;
    net_map_["~AEN"]       = &aen;  // inverted version, same signal in model
    net_map_["HOLDA"]      = &hlda;
    net_map_["T/C"]        = &tc;

    // --- 8259A PIC ---
    net_map_["IRQ0"] = &irq0;
    net_map_["IRQ1"] = &irq1;
    net_map_["IRQ2"] = &irq2;
    net_map_["IRQ3"] = &irq3;
    net_map_["IRQ4"] = &irq4;
    net_map_["IRQ5"] = &irq5;
    net_map_["IRQ6"] = &irq6;
    net_map_["IRQ7"] = &irq7;
    net_map_["I/O_CH_RDY"]  = &rdy_wait;
    net_map_["~I/O_CH_CK"]  = &parity_chk;
    net_map_["I/O_CH_CK"]   = &parity_chk;

    // --- 8237A DMA ---
    net_map_["DACK0"]  = &dack0;
    net_map_["~DACK0"] = &dack0;
    net_map_["~DACK1"] = &dack1;
    net_map_["~DACK2"] = &dack2;
    net_map_["~DACK3"] = &dack3;
    net_map_["DRQ0"]   = &dreq0;
    net_map_["DRQ1"]   = &dreq1;
    net_map_["DRQ2"]   = &dreq2;
    net_map_["DRQ3"]   = &dreq3;
    net_map_["~DACK_0_BRD"] = &dack0;
    net_map_["~HRQ_DMA"]    = &hrq;
    net_map_["~DMA_CS"]     = &dma_cs;
    net_map_["~DMA_WAIT"]   = &rdy_wait;
    net_map_["~DMA_AEN"]    = &aen;
    net_map_["RDY_TO_DMA"]  = &dreq0;  // PIT ch1 out -> DMA ch0 request

    // --- 8255A PPI ---
    net_map_["~PPI_CS"] = &ppi_cs;

    // --- 8253 PIT ---
    // PIT has pin-specific nets: U34.21, U34.22, U34.23
    net_map_["U34.21"] = &pit_out0;
    net_map_["U34.22"] = &pit_gate2;
    net_map_["U34.23"] = &pit_clk2;

    // --- Memory control ---
    net_map_["/SHEET3/RAS"]          = &ras;
    net_map_["/SHEET3/~CAS"]        = &cas_0; // base CAS signal
    net_map_["~CAS0"]               = &cas_0;
    net_map_["~CAS1"]               = &cas_1;
    net_map_["~CAS2"]               = &cas_2;
    net_map_["~CAS3"]               = &cas_3;
    net_map_["~RAS0"]               = &ras;    // per-bank RAS in our model
    net_map_["~RAS1"]               = &ras;
    net_map_["~RAS2"]               = &ras;
    net_map_["~RAS3"]               = &ras;
    net_map_["~WE"]                 = &we;
    net_map_["ADDR_SEL"]            = &ras_cas_sel;
    net_map_["~RAM_ADDR_SEL"]       = &ras_cas_sel;
    net_map_["~ROM_ADDR_SEL"]       = &rom_cs;
    net_map_["PCK"]                 = &parity_chk;
    net_map_["~PCK"]                = &parity_chk;
    net_map_["~ENB_RAM_PCK"]        = &parity_chk;
    net_map_["~WRT_NMI_REG"]        = &nmi_mask;
    net_map_["~WRT_DMA_PG_REG"]     = &dma_cs;  // DMA page register write

    // Multiplexed DRAM address MA0-MA7
    for (int i = 0; i < 8; ++i)
        net_map_["MA" + std::to_string(i)] = &ma[i];

    // DRAM data MD0-MD7 (active DRAM data bus, same as system data in our model)
    for (int i = 0; i < 8; ++i)
        net_map_["MD" + std::to_string(i)] = &sd[i];
    net_map_["MDP"] = &parity_chk;  // DRAM parity data

    // CPU address bus extras for extended decode
    for (int i = 0; i < 13; ++i)
        net_map_["XA" + std::to_string(i)] = &sa[i];
    for (int i = 0; i < 8; ++i)
        net_map_["XD" + std::to_string(i)] = &sd[i];
    net_map_["/SHEET5/XA10"] = &sa[10];
    net_map_["/SHEET5/XA11"] = &sa[11];
    net_map_["/SHEET5/XA12"] = &sa[12];

    // --- Address decode chip selects ---
    net_map_["~CS2"]  = &pic_cs;    // 8259 CS
    net_map_["~CS3"]  = &pit_cs;    // 8253 CS
    net_map_["~CS4"]  = &ppi_cs;    // 8255 CS
    net_map_["~CS5"]  = &dma_cs;    // 8237 CS
    net_map_["~CS6"]  = &io_cs;     // I/O decode
    net_map_["~CS7"]  = &ram_cs;    // RAM CS
    net_map_["~INTR_CS"] = &pic_cs;

    // --- Speaker / cassette ---
    net_map_["SPKR_DATA"]        = &spkr_data;
    net_map_["TIM_2_GATE_SPK"]   = &spkr_gate;
    net_map_["T/C_2_OUT"]        = &pit_out2;
    net_map_["MOTOR_OFF"]        = &cas_motor;
    net_map_["CASS_DATA_IN"]     = &cas_data_in;
    net_map_["~ENABLE_I/O_CK"]  = &parity_chk;

    // --- Misc ---
    net_map_["N.P._INSTL_SW"]  = &cpu_test;  // No 8087 installed indicator
    net_map_["N.P._NPI"]       = &cpu_test;
    net_map_["~RDY~/WAIT"]     = &rdy_wait;
    net_map_["/SHEET10/RESERVED"] = &psu.gnd;  // Reserved ISA pin, tied to GND
    net_map_["/SHEET3/~REFRSH_GATE"] = &pit_gate1;
    net_map_["U5.3"] = &rdy_nand;

    // --- RN4 signals (DRAM address buffered through 30-ohm network) ---
    for (int i = 0; i < 8; ++i)
        net_map_["/SHEET6/A" + std::to_string(i) + "_RN4"] = &ma[i];
}

// =========================================================================
// Build the component registry: maps ref -> object pointer for each type.
// =========================================================================
void Motherboard::build_component_registry() {
    // --- IC Sockets ---
    auto reg_socket = [&](Socket& s) { sockets_[s.ref()] = &s; };
    reg_socket(u1);  reg_socket(u2);  reg_socket(u3);  reg_socket(xu4);
    reg_socket(u5);  reg_socket(u6);  reg_socket(u7);  reg_socket(u8);
    reg_socket(u9);  reg_socket(u10); reg_socket(u11); reg_socket(u12);
    reg_socket(u13); reg_socket(u14); reg_socket(u15); reg_socket(u16);
    reg_socket(u17); reg_socket(u18); reg_socket(u19); reg_socket(u23);
    reg_socket(u24); reg_socket(u26); reg_socket(u27);
    reg_socket(u28); reg_socket(u29); reg_socket(u30); reg_socket(u31);
    reg_socket(u32); reg_socket(u33); reg_socket(u34); reg_socket(u35);
    reg_socket(u36);
    reg_socket(u46); reg_socket(u47); reg_socket(u48); reg_socket(u49);
    reg_socket(u50); reg_socket(u51); reg_socket(u52);
    reg_socket(u62); reg_socket(u63); reg_socket(u64); reg_socket(u65);
    reg_socket(u66); reg_socket(u67);
    reg_socket(u79); reg_socket(u80); reg_socket(u81); reg_socket(u82);
    reg_socket(u83); reg_socket(u84);
    reg_socket(u94); reg_socket(u95); reg_socket(u96); reg_socket(u97);
    reg_socket(u98); reg_socket(u99); reg_socket(u100); reg_socket(u101);
    reg_socket(td1); reg_socket(td2);

    // RAM bank sockets
    for (auto& s : ram_bank0) reg_socket(s);
    for (auto& s : ram_bank1) reg_socket(s);
    for (auto& s : ram_bank2) reg_socket(s);
    for (auto& s : ram_bank3) reg_socket(s);

    // --- ISA Slots ---
    isa_slots_["J1"] = &j1;
    isa_slots_["J2"] = &j2;
    isa_slots_["J3"] = &j3;
    isa_slots_["J4"] = &j4;
    isa_slots_["J5"] = &j5;

    // --- DIP Switches ---
    dip_switches_["SW1"] = &sw1;
    dip_switches_["SW2"] = &sw2;

    // --- Connectors ---
    connectors_["J6"] = &j6;
    connectors_["J7"] = &j7;
    connectors_["J8"] = &j8;
    connectors_["P1"] = &p1;
    connectors_["P2"] = &p2;
    connectors_["P3"] = &p3;
    connectors_["P4"] = &p4;

    // --- Discrete Resistors ---
    auto reg_r = [&](Resistor& r) { resistors_[r.ref] = &r; };
    reg_r(r1);  reg_r(r2);  reg_r(r3);  reg_r(r4);  reg_r(r5);
    reg_r(r6);  reg_r(r7);  reg_r(r8);  reg_r(r9);  reg_r(r10);
    reg_r(r11); reg_r(r12); reg_r(r13); reg_r(r14); reg_r(r15);
    reg_r(r16); reg_r(r17); reg_r(r18); reg_r(r19); reg_r(r20);
    reg_r(r21); reg_r(r22); reg_r(r23); reg_r(r25);

    // --- Resistor Networks ---
    resistor_networks_["RN1"] = &rn1;
    resistor_networks_["RN2"] = &rn2;
    resistor_networks_["RN3"] = &rn3;
    resistor_networks_["RN4"] = &rn4;

    // --- Capacitors (named ones) ---
    auto reg_c = [&](Capacitor& c) { capacitors_[c.ref] = &c; };
    reg_c(c1);  reg_c(c3);  reg_c(c5);  reg_c(c8);  reg_c(c9);
    reg_c(c22); reg_c(c23); reg_c(c24); reg_c(c25); reg_c(c26);
    reg_c(c27); reg_c(c28); reg_c(c29); reg_c(c30); reg_c(c31);
    reg_c(c32); reg_c(c33); reg_c(c34); reg_c(c35); reg_c(c36);
    reg_c(c37); reg_c(c38); reg_c(c39); reg_c(c40); reg_c(c41);
    reg_c(c42); reg_c(c43); reg_c(c44); reg_c(c45); reg_c(c46);
    reg_c(c47); reg_c(c48);

    // --- Crystal ---
    crystals_["Y1"] = &y1;

    // --- Diode ---
    diodes_["D1"] = &d1;

    // --- Relay ---
    relays_["K1"] = &k1;

    // --- Trimmer ---
    trimmers_["VC1"] = &vc1;
}

// =========================================================================
// Look up or create a Signal for a given net name.
// =========================================================================
static Signal* resolve_net(
    const std::string& net_name,
    std::unordered_map<std::string, Signal*>& net_map,
    std::vector<std::unique_ptr<Signal>>& dynamic_signals)
{
    if (net_name.empty()) return nullptr;

    auto it = net_map.find(net_name);
    if (it != net_map.end()) return it->second;

    // Create a new dynamic signal for this net.
    auto sig = std::make_unique<Signal>(net_name);
    Signal* ptr = sig.get();
    dynamic_signals.push_back(std::move(sig));
    net_map[net_name] = ptr;
    return ptr;
}

// =========================================================================
// Wire all components from the BRD netlist.
// =========================================================================
void Motherboard::wire_from_brd() {
    // 1. Build the net name -> Signal* mapping.
    build_net_map();

    // 2. Build the component registry.
    build_component_registry();

    // 3. Parse the BRD file.
    std::string brd_path = BRD_PATH;

    // Try relative path first, then try from executable directory.
    if (!std::filesystem::exists(brd_path)) {
        spdlog::error("BRD file not found: {}", brd_path);
        throw std::runtime_error("BRD file not found: " + brd_path);
    }

    BrdData brd = parse_brd(brd_path);

    int wired_pins = 0;
    int skipped_components = 0;

    // 4. Wire each component's pins to their nets.
    for (const auto& comp : brd.components) {
        const std::string& ref = comp.ref;

        // Skip mounting holes and unnamed components.
        if (ref == "?" || ref.empty() || ref[0] == 'H') continue;

        // --- IC Sockets ---
        if (auto it = sockets_.find(ref); it != sockets_.end()) {
            Socket* sock = it->second;
            for (const auto& pad : comp.pads) {
                if (pad.net_name.empty() || pad.net_id == 0) continue;
                Signal* sig = resolve_net(pad.net_name, net_map_, dynamic_signals_);
                if (!sig) continue;
                int pin_num = 0;
                try { pin_num = std::stoi(pad.pin); } catch (...) { continue; }
                if (pin_num >= 1 && pin_num <= sock->pin_count()) {
                    sock->wire(pin_num, *sig);
                    ++wired_pins;
                }
            }
            continue;
        }

        // --- ISA Slots ---
        if (auto it = isa_slots_.find(ref); it != isa_slots_.end()) {
            IsaSlot* slot = it->second;
            for (const auto& pad : comp.pads) {
                if (pad.net_name.empty() || pad.net_id == 0) continue;
                Signal* sig = resolve_net(pad.net_name, net_map_, dynamic_signals_);
                if (!sig) continue;
                int pin_num = 0;
                try { pin_num = std::stoi(pad.pin); } catch (...) { continue; }
                slot->wire_pin(pin_num, sig);
                ++wired_pins;
            }
            continue;
        }

        // --- DIP Switches ---
        if (auto it = dip_switches_.find(ref); it != dip_switches_.end()) {
            DipSwitch* sw = it->second;
            for (const auto& pad : comp.pads) {
                if (pad.net_name.empty() || pad.net_id == 0) continue;
                Signal* sig = resolve_net(pad.net_name, net_map_, dynamic_signals_);
                if (!sig) continue;
                int pin_num = 0;
                try { pin_num = std::stoi(pad.pin); } catch (...) { continue; }
                // 16-pin DIP switch: pins 1-8 = common (GND), pins 9-16 = signal side.
                // Position P corresponds to pin (17-P).
                int positions = sw->positions();
                if (pin_num > positions && pin_num <= positions * 2) {
                    int position = positions * 2 + 1 - pin_num;  // pin 16->pos 1, pin 9->pos 8
                    sw->wire(position, *sig);
                    ++wired_pins;
                }
                // GND-side pins (1-8) don't need switch wiring -- they're just GND.
            }
            continue;
        }

        // --- Connectors ---
        if (auto it = connectors_.find(ref); it != connectors_.end()) {
            Connector* conn = it->second;
            for (const auto& pad : comp.pads) {
                if (pad.net_name.empty() || pad.net_id == 0) continue;
                Signal* sig = resolve_net(pad.net_name, net_map_, dynamic_signals_);
                if (!sig) continue;
                int pin_num = 0;
                try { pin_num = std::stoi(pad.pin); } catch (...) { continue; }
                // Ensure pins vector is large enough.
                if (pin_num > 0) {
                    if (static_cast<int>(conn->pins.size()) < pin_num)
                        conn->pins.resize(pin_num, nullptr);
                    conn->pins[pin_num - 1] = sig;
                    ++wired_pins;
                }
            }
            continue;
        }

        // --- Discrete Resistors ---
        if (auto it = resistors_.find(ref); it != resistors_.end()) {
            Resistor* r = it->second;
            for (const auto& pad : comp.pads) {
                if (pad.net_name.empty() || pad.net_id == 0) continue;
                Signal* sig = resolve_net(pad.net_name, net_map_, dynamic_signals_);
                if (!sig) continue;
                int pin_num = 0;
                try { pin_num = std::stoi(pad.pin); } catch (...) { continue; }
                if (pin_num == 1) r->signal_a = sig;
                else if (pin_num == 2) r->signal_b = sig;
            }
            continue;
        }

        // --- Resistor Networks ---
        if (auto it = resistor_networks_.find(ref); it != resistor_networks_.end()) {
            ResistorNetwork* rn = it->second;
            for (const auto& pad : comp.pads) {
                if (pad.net_name.empty() || pad.net_id == 0) continue;
                Signal* sig = resolve_net(pad.net_name, net_map_, dynamic_signals_);
                if (!sig) continue;
                int pin_num = 0;
                try { pin_num = std::stoi(pad.pin); } catch (...) { continue; }
                // Store all pin signals in the pins vector.
                if (pin_num > 0) {
                    if (static_cast<int>(rn->pins.size()) < pin_num)
                        rn->pins.resize(pin_num, nullptr);
                    rn->pins[pin_num - 1] = sig;
                    ++wired_pins;
                }
            }
            continue;
        }

        // --- Capacitors ---
        if (auto it = capacitors_.find(ref); it != capacitors_.end()) {
            Capacitor* c = it->second;
            for (const auto& pad : comp.pads) {
                if (pad.net_name.empty() || pad.net_id == 0) continue;
                Signal* sig = resolve_net(pad.net_name, net_map_, dynamic_signals_);
                if (!sig) continue;
                int pin_num = 0;
                try { pin_num = std::stoi(pad.pin); } catch (...) { continue; }
                if (pin_num == 1)      c->signal_a = sig;
                else if (pin_num == 2) c->signal_b = sig;
                // Some caps have 3 pins (pin 3 = extra ground pad), treat as signal_a
                else if (pin_num == 3 && !c->signal_a) c->signal_a = sig;
            }
            continue;
        }

        // --- Crystal ---
        if (auto it = crystals_.find(ref); it != crystals_.end()) {
            Crystal* y = it->second;
            for (const auto& pad : comp.pads) {
                if (pad.net_name.empty() || pad.net_id == 0) continue;
                Signal* sig = resolve_net(pad.net_name, net_map_, dynamic_signals_);
                if (!sig) continue;
                int pin_num = 0;
                try { pin_num = std::stoi(pad.pin); } catch (...) { continue; }
                if (pin_num == 1)      y->osc_in = sig;
                else if (pin_num == 2) y->osc_out = sig;
                // Pins 3,4 = GND (case)
            }
            continue;
        }

        // --- Diode ---
        if (auto it = diodes_.find(ref); it != diodes_.end()) {
            Diode* d = it->second;
            for (const auto& pad : comp.pads) {
                if (pad.net_name.empty() || pad.net_id == 0) continue;
                Signal* sig = resolve_net(pad.net_name, net_map_, dynamic_signals_);
                if (!sig) continue;
                int pin_num = 0;
                try { pin_num = std::stoi(pad.pin); } catch (...) { continue; }
                if (pin_num == 1)      d->anode = sig;
                else if (pin_num == 2) d->cathode = sig;
            }
            continue;
        }

        // --- Relay ---
        if (auto it = relays_.find(ref); it != relays_.end()) {
            Relay* k = it->second;
            for (const auto& pad : comp.pads) {
                if (pad.net_name.empty() || pad.net_id == 0) continue;
                Signal* sig = resolve_net(pad.net_name, net_map_, dynamic_signals_);
                if (!sig) continue;
                int pin_num = 0;
                try { pin_num = std::stoi(pad.pin); } catch (...) { continue; }
                if (pin_num == 1)      k->coil_a = sig;
                else if (pin_num == 16) k->coil_b = sig;
                // Other relay pins are contact signals -- not modeled yet.
            }
            continue;
        }

        // --- Trimmer ---
        if (auto it = trimmers_.find(ref); it != trimmers_.end()) {
            Trimmer* vc = it->second;
            for (const auto& pad : comp.pads) {
                if (pad.net_name.empty() || pad.net_id == 0) continue;
                Signal* sig = resolve_net(pad.net_name, net_map_, dynamic_signals_);
                if (!sig) continue;
                int pin_num = 0;
                try { pin_num = std::stoi(pad.pin); } catch (...) { continue; }
                if (pin_num == 1)      vc->signal_a = sig;
                else if (pin_num == 2) vc->signal_b = sig;
            }
            continue;
        }

        // --- Unnamed bypass capacitors (ref = "C1-1" or similar duplicates) ---
        // These are bulk VCC-GND decoupling. Create them dynamically.
        if (ref.find('-') != std::string::npos && ref[0] == 'C') {
            Capacitor cap;
            cap.ref = ref;
            cap.value = comp.value;
            for (const auto& pad : comp.pads) {
                if (pad.net_name.empty() || pad.net_id == 0) continue;
                Signal* sig = resolve_net(pad.net_name, net_map_, dynamic_signals_);
                if (!sig) continue;
                int pin_num = 0;
                try { pin_num = std::stoi(pad.pin); } catch (...) { continue; }
                if (pin_num == 1)      cap.signal_a = sig;
                else if (pin_num == 2) cap.signal_b = sig;
            }
            bypass_caps.push_back(std::move(cap));
            continue;
        }

        // Component not in our registry -- log it.
        ++skipped_components;
        spdlog::debug("BRD component not in registry: {} ({})", ref, comp.value);
    }

    spdlog::info("BRD wiring complete: {} pins wired, {} dynamic signals, {} components skipped",
                 wired_pins, dynamic_signals_.size(), skipped_components);
}

} // namespace bench
