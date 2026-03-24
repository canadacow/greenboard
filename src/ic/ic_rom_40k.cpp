#include "ic/ic_rom_40k.h"
#include <spdlog/spdlog.h>
#include <fstream>

namespace bench {

IC_ROM_40K::IC_ROM_40K() : CallbackComponent("ROM_40K") { set_description("ROM"); }

void IC_ROM_40K::load(int bank, const std::string& file_path) {
    if (bank < 0 || bank >= 5) {
        spdlog::error("[ROM] invalid bank {}", bank);
        return;
    }
    if (file_path.empty()) return;
    std::ifstream f(file_path, std::ios::binary);
    if (!f) {
        spdlog::error("[ROM] failed to open ROM file for bank {}: {}", bank, file_path);
        return;
    }
    f.read(reinterpret_cast<char*>(rom_.data() + bank * 8192), 8192);
    spdlog::info("[ROM] bank {} loaded {} bytes from {}", bank, f.gcount(), file_path);
}

void IC_ROM_40K::install(Socket& u29, Socket& u30, Socket& u31, Socket& u32, Socket& u33) {
    Socket* sockets[5] = {&u29, &u30, &u31, &u32, &u33};

    auto pin = [](Socket& s, int p) -> Pin {
        Signal* sig = s.pin_signal(p);
        return sig ? sig->pin() : Pin{};
    };
    auto connect_pin = [this](Socket& s, int p) -> Pin {
        Signal* sig = s.pin_signal(p);
        if (sig) sig->connect(this);
        return sig ? sig->pin() : Pin{};
    };

    // Shared address/data from first socket (all on same nets)
    Socket& s0 = *sockets[0];
    pin_a_[0]  = pin(s0, 8);  pin_a_[1]  = pin(s0, 7);  pin_a_[2]  = pin(s0, 6);
    pin_a_[3]  = pin(s0, 5);  pin_a_[4]  = pin(s0, 4);  pin_a_[5]  = pin(s0, 3);
    pin_a_[6]  = pin(s0, 2);  pin_a_[7]  = pin(s0, 1);  pin_a_[8]  = pin(s0, 23);
    pin_a_[9]  = pin(s0, 22); pin_a_[10] = pin(s0, 19); pin_a_[11] = pin(s0, 18);
    pin_a_[12] = pin(s0, 21);

    pin_d_[0] = pin(s0, 9);  pin_d_[1] = pin(s0, 10); pin_d_[2] = pin(s0, 11);
    pin_d_[3] = pin(s0, 13); pin_d_[4] = pin(s0, 14); pin_d_[5] = pin(s0, 15);
    pin_d_[6] = pin(s0, 16); pin_d_[7] = pin(s0, 17);

    pin_vcc_ = connect_pin(s0, 24);

    // Per-bank ~CS
    for (int b = 0; b < 5; ++b)
        banks_[b].cs = connect_pin(*sockets[b], 20);

    // Pin directions
    for (int i = 0; i < 13; ++i) declare_input(pin_a_[i]);
    for (int b = 0; b < 5; ++b) declare_async_input(banks_[b].cs);
    for (int i = 0; i < 8; ++i) { declare_input(pin_d_[i]); declare_output(pin_d_[i]); }

    // Data outputs tri-stated when no ~CS is Low
    declare_bidir_block({pin_d_[0], pin_d_[1], pin_d_[2], pin_d_[3],
                         pin_d_[4], pin_d_[5], pin_d_[6], pin_d_[7]},
                        BidirDir::HiZ | BidirDir::Output,
                        [this]() {
                            for (int b = 0; b < 5; ++b)
                                if (banks_[b].cs.level() == Level::Low)
                                    return BidirDir::Output;
                            return BidirDir::HiZ;
                        });
}

void IC_ROM_40K::on_power_on() {
    active_bank_ = -1;
    spdlog::debug("[ROM] power on");
}

void IC_ROM_40K::on_power_off() {
    if (active_bank_ >= 0) {
        for (int i = 0; i < 8; ++i)
            pin_d_[i].release();
        active_bank_ = -1;
    }
    spdlog::debug("[ROM] power off");
}

void IC_ROM_40K::on_cycle(Fiber /*caller*/) {
    // Find active bank (at most one ~CS Low at a time)
    int active = -1;
    for (int b = 0; b < 5; ++b) {
        if (banks_[b].cs.level() == Level::Low) {
            active = b;
            break;
        }
    }

    if (active >= 0) {
        // Drive data from selected bank
        uint16_t addr = read_address();
        uint8_t data = rom_[active * 8192 + (addr & 0x1FFF)];
        for (int i = 0; i < 8; ++i)
            pin_d_[i].drive((data >> i) & 1 ? Level::High : Level::Low);
        active_bank_ = active;
    } else if (active_bank_ >= 0) {
        // No bank selected -- release data bus
        for (int i = 0; i < 8; ++i)
            pin_d_[i].release();
        active_bank_ = -1;
    }
}

uint16_t IC_ROM_40K::read_address() const {
    uint16_t addr = 0;
    for (int i = 0; i < 13; ++i) {
        if (pin_a_[i].level() == Level::High)
            addr |= (1 << i);
    }
    return addr;
}

} // namespace bench
