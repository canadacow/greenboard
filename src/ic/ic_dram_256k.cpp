#include "ic/ic_dram_256k.h"
#include <spdlog/spdlog.h>

namespace bench {

IC_DRAM_256K::IC_DRAM_256K() : CallbackComponent("DRAM_256K") { set_description("DRAM"); }

void IC_DRAM_256K::install(std::vector<Socket>& bank0, std::vector<Socket>& bank1,
                           std::vector<Socket>& bank2, std::vector<Socket>& bank3) {
    std::vector<Socket>* bank_sockets[4] = {&bank0, &bank1, &bank2, &bank3};

    auto pin = [](Socket& s, int p) -> Pin {
        Signal* sig = s.pin_signal(p);
        return sig ? sig->pin() : Pin{};
    };
    auto connect_pin = [this](Socket& s, int p) -> Pin {
        Signal* sig = s.pin_signal(p);
        if (sig) sig->connect(this);
        return sig ? sig->pin() : Pin{};
    };

    // Shared signals -- grab from first chip of bank 0
    Socket& chip0 = bank0[0];

    // 4164 address pins -> address bits
    pin_a_[0] = pin(chip0, 5);    // A0
    pin_a_[1] = pin(chip0, 7);    // A1
    pin_a_[2] = pin(chip0, 6);    // A2
    pin_a_[3] = pin(chip0, 12);   // A3
    pin_a_[4] = pin(chip0, 11);   // A4
    pin_a_[5] = pin(chip0, 10);   // A5
    pin_a_[6] = pin(chip0, 13);   // A6
    pin_a_[7] = pin(chip0, 9);    // A7

    pin_we_  = connect_pin(chip0, 3);   // ~WE (set at T1, read at ~CAS time)
    pin_vcc_ = connect_pin(chip0, 8);   // VCC

    for (int b = 0; b < 4; ++b) {
        auto& sockets = *bank_sockets[b];
        auto& bank = banks_[b];

        // ~RAS from pin 4, ~CAS from pin 15 (same net for all chips in bank)
        bank.ras = connect_pin(sockets[0], 4);
        bank.cas = connect_pin(sockets[0], 15);

        // Data pins: socket[0]=parity chip, socket[1..8]=MD0..MD7
        for (int i = 0; i < 8; ++i) {
            bank.din[i]  = pin(sockets[i + 1], 2);    // MD[i] DIN
            bank.dout[i] = pin(sockets[i + 1], 14);   // MD[i] DOUT
        }
        bank.din[8]  = pin(sockets[0], 2);             // Parity DIN
        bank.dout[8] = pin(sockets[0], 14);            // Parity DOUT
    }

    // Pin directions for wiring visualization.
    for (int i = 0; i < 8; ++i) declare_input(pin_a_[i]);   // MA0-MA7
    declare_async_input(pin_we_);                              // ~WE (driven at T1, sampled at ~CAS)
    for (int b = 0; b < 4; ++b) {
        declare_input(banks_[b].ras);                         // ~RAS
        declare_input(banks_[b].cas);                         // ~CAS
        for (int i = 0; i < 9; ++i) { declare_input(banks_[b].din[i]); declare_output(banks_[b].din[i]); }
    }

    // MD is bidirectional: Output during read (~WE High), Input during write (~WE Low).
    // Collect all DOUT pins across all banks for the bidir block.
    std::vector<Pin> md_pins;
    for (int b = 0; b < 4; ++b)
        for (int i = 0; i < 9; ++i)
            if (banks_[b].din[i].idx != 0)
                md_pins.push_back(banks_[b].din[i]);
    declare_bidir_block(
        std::initializer_list<Pin>(md_pins.data(), md_pins.data() + md_pins.size()),
        BidirDir::Input | BidirDir::Output,
        [this]() {
            auto wv = pin_we_.level();
            spdlog::trace("[DRAM] bidir lambda: ~WE={}", int(wv));
            return wv == Level::Low ? BidirDir::Input : BidirDir::Output;
        });
}

void IC_DRAM_256K::on_power_on() {
    // DRAM contents are undefined at power-on (leave as zero-initialized)
    spdlog::debug("[DRAM] power on");
}

void IC_DRAM_256K::on_power_off() {
    // Release any driven data pins
    for (int b = 0; b < 4; ++b) {
        auto& bank = banks_[b];
        if (bank.driving) {
            for (int i = 0; i < 9; ++i)
                bank.dout[i].release();
            bank.driving = false;
        }
        bank.row_latched = false;
        bank.ras_prev = Level::HiZ;
        bank.cas_prev = Level::HiZ;
    }
    active_bank_ = -1;
    spdlog::debug("[DRAM] power off");
}

void IC_DRAM_256K::on_signal_change(Fiber /*caller*/) {

    // Find the active bank: the one with ~RAS Low (only one at a time).
    // On ~RAS rising edge, the previously active bank needs processing too,
    // so also check the bank we were tracking.
    int b = active_bank_;

    if (b < 0) {
        // No active bank -- scan for a ~RAS falling edge
        for (int i = 0; i < 4; ++i) {
            if (banks_[i].ras.level() == Level::Low && banks_[i].ras_prev != Level::Low) {
                b = i;
                spdlog::trace("[DRAM] ~RAS{} falling edge", i);
                break;
            }
        }
        if (b < 0) return;
    }

    auto& bank = banks_[b];
    Level ras_cur = bank.ras.level();
    Level cas_cur = bank.cas.level();

    // ~RAS falling edge: latch row address
    if (ras_cur == Level::Low && bank.ras_prev != Level::Low) {
        bank.row_addr = read_address();
        bank.row_latched = true;
        active_bank_ = b;
        spdlog::debug("[DRAM] ~RAS{} fall: row=0x{:02X} ~WE={}", b, bank.row_addr, int(pin_we_.level()));
    }

    // ~RAS rising edge: end of cycle, release outputs
    if (ras_cur == Level::High && bank.ras_prev != Level::High) {
        bank.row_latched = false;
        if (bank.driving) {
            for (int i = 0; i < 9; ++i)
                bank.dout[i].release();
            bank.driving = false;
        }
        active_bank_ = -1;
    }

    // ~CAS falling edge: latch column address, perform read or write
    if (cas_cur == Level::Low && bank.cas_prev != Level::Low) {
        if (!bank.row_latched) {
            spdlog::warn("[DRAM] ~CAS{} fall WITHOUT row latched! col=0x{:02X} ~WE={}",
                         b, read_address(), int(pin_we_.level()));
        }
    }
    if (cas_cur == Level::Low && bank.cas_prev != Level::Low && bank.row_latched) {
        uint8_t col_addr = read_address();
        uint32_t addr = (static_cast<uint32_t>(b) << 16)
                      | (static_cast<uint32_t>(bank.row_addr) << 8)
                      | col_addr;
        // Reconstruct linear address: 74S158 inverts, so row=~A0-A7, col=~A8-A15
        uint32_t linear = (static_cast<uint32_t>(uint8_t(~col_addr)) << 8) | uint8_t(~bank.row_addr);
        if (pin_we_.level() == Level::Low) {
            // Write: sample DIN pins, store to RAM
            uint8_t data = 0;
            for (int i = 0; i < 8; ++i) {
                if (bank.din[i].level() == Level::High)
                    data |= (1 << i);
            }
            ram_[addr] = data;
            parity_[addr] = bank.din[8].level() == Level::High ? 1 : 0;
            spdlog::trace("[DRAM] WRITE bank{} row=0x{:02X} col=0x{:02X} linear=0x{:05X} (idx=0x{:05X}) data=0x{:02X}",
                          b, bank.row_addr, col_addr, linear, addr, data);
        } else {
            // Read: drive DOUT pins from RAM
            uint8_t data = ram_[addr];
            spdlog::trace("[DRAM] READ bank{} row=0x{:02X} col=0x{:02X} linear=0x{:05X} (idx=0x{:05X}) data=0x{:02X}",
                          b, bank.row_addr, col_addr, linear, addr, data);
            for (int i = 0; i < 8; ++i) {
                bank.dout[i].drive((data >> i) & 1 ? Level::High : Level::Low);
            }
            bank.dout[8].drive(parity_[addr] ? Level::High : Level::Low);
            bank.driving = true;
        }
    }

    // ~CAS rising edge: release data outputs
    if (cas_cur == Level::High && bank.cas_prev != Level::High) {
        if (bank.driving) {
            for (int i = 0; i < 9; ++i)
                bank.dout[i].release();
            bank.driving = false;
        }
    }

    bank.ras_prev = ras_cur;
    bank.cas_prev = cas_cur;
}

uint8_t IC_DRAM_256K::read_address() const {
    uint8_t addr = 0;
    for (int i = 0; i < 8; ++i) {
        if (pin_a_[i].level() == Level::High)
            addr |= (1 << i);
    }
    return addr;
}

} // namespace bench
