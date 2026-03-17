#include "ic/ic_rom_8k.h"
#include <spdlog/spdlog.h>
#include <fstream>

namespace bench {

IC_ROM_8K::IC_ROM_8K(const std::string& label, const std::string& file_path)
    : CallbackComponent(label)
{
    set_description("8K ROM");
    if (!file_path.empty()) {
        std::ifstream f(file_path, std::ios::binary);
        if (!f) {
            spdlog::error("[{}] failed to open ROM file: {}", name(), file_path);
            return;
        }
        f.read(reinterpret_cast<char*>(rom_.data()), rom_.size());
        spdlog::info("[{}] loaded {} bytes from {}", name(), f.gcount(), file_path);
    }
}

void IC_ROM_8K::install(Socket& socket) {
    auto pin = [&](int p) -> Pin {
        Signal* s = socket.pin_signal(p);
        return s ? s->pin() : Pin{};
    };
    auto connect_pin = [&](int p) -> Pin {
        Signal* s = socket.pin_signal(p);
        if (s) s->connect(this);
        return s ? s->pin() : Pin{};
    };

    pin_a_[0]  = pin(8);  pin_a_[1]  = pin(7);  pin_a_[2]  = pin(6);
    pin_a_[3]  = pin(5);  pin_a_[4]  = pin(4);  pin_a_[5]  = pin(3);
    pin_a_[6]  = pin(2);  pin_a_[7]  = pin(1);  pin_a_[8]  = pin(23);
    pin_a_[9]  = pin(22); pin_a_[10] = pin(19); pin_a_[11] = pin(18);
    pin_a_[12] = pin(21);

    pin_d_[0] = pin(9);  pin_d_[1] = pin(10); pin_d_[2] = pin(11);
    pin_d_[3] = pin(13); pin_d_[4] = pin(14); pin_d_[5] = pin(15);
    pin_d_[6] = pin(16); pin_d_[7] = pin(17);

    pin_cs_  = connect_pin(20);
    pin_vcc_ = connect_pin(24);

    // Pin directions for wiring visualization.
    for (int i = 0; i < 13; ++i) declare_input(pin_a_[i]);
    // ~CS feeds bidir lambda (sampled at permutation time), async.
    declare_async_input(pin_cs_);
    for (int i = 0; i < 8; ++i) { declare_input(pin_d_[i]); declare_output(pin_d_[i]); }

    // Data outputs are tri-stated when ~CS is High -- no DAG dependency.
    declare_bidir_block({pin_d_[0], pin_d_[1], pin_d_[2], pin_d_[3],
                         pin_d_[4], pin_d_[5], pin_d_[6], pin_d_[7]},
                        BidirDir::HiZ | BidirDir::Output,
                        [this]() { return pin_cs_.level() == Level::Low ? BidirDir::Output : BidirDir::HiZ; });
}

void IC_ROM_8K::on_power_on() {
    update_outputs();
}

void IC_ROM_8K::on_power_off() {
    if (driving_) {
        for (int i = 0; i < 8; ++i) {
            pin_d_[i].release();
        }
        driving_ = false;
    }
}

void IC_ROM_8K::on_signal_change(Fiber /*caller*/) {
    update_outputs();
}

uint16_t IC_ROM_8K::read_address() const {
    uint16_t addr = 0;
    for (int i = 0; i < 13; ++i) {
        if (pin_a_[i].level() == Level::High)
            addr |= (1 << i);
    }
    return addr;
}

void IC_ROM_8K::update_outputs() {
    bool selected = pin_cs_.level() == Level::Low;

    if (selected) {
        uint16_t addr = read_address();
        uint8_t data = rom_[addr & 0x1FFF];
        for (int i = 0; i < 8; ++i) {
            pin_d_[i].drive((data >> i) & 1 ? Level::High : Level::Low);
        }
        spdlog::trace("[{}] ~CS Low, addr=0x{:04X} data=0x{:02X}", name(), addr, data);
        driving_ = true;
    } else if (driving_) {
        for (int i = 0; i < 8; ++i) {
            pin_d_[i].release();
        }
        driving_ = false;
    }
}

} // namespace bench
