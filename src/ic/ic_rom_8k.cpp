#include "ic/ic_rom_8k.h"
#include <spdlog/spdlog.h>
#include <fstream>

namespace bench {

IC_ROM_8K::IC_ROM_8K(const std::string& label, const std::string& file_path)
    : InlineComponent(label)
{
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
    // Address pins: A0=pin8, A1=pin7, ..., A7=pin1, A8=pin23, A9=pin22,
    //               A10=pin19, A11=pin18, A12=pin21
    pin_a_[0]  = socket.pin_signal(8);   // A0
    pin_a_[1]  = socket.pin_signal(7);   // A1
    pin_a_[2]  = socket.pin_signal(6);   // A2
    pin_a_[3]  = socket.pin_signal(5);   // A3
    pin_a_[4]  = socket.pin_signal(4);   // A4
    pin_a_[5]  = socket.pin_signal(3);   // A5
    pin_a_[6]  = socket.pin_signal(2);   // A6
    pin_a_[7]  = socket.pin_signal(1);   // A7
    pin_a_[8]  = socket.pin_signal(23);  // A8
    pin_a_[9]  = socket.pin_signal(22);  // A9
    pin_a_[10] = socket.pin_signal(19);  // A10
    pin_a_[11] = socket.pin_signal(18);  // A11
    pin_a_[12] = socket.pin_signal(21);  // A12

    // Data pins: D0=pin9, D1=pin10, D2=pin11, D3=pin13,
    //            D4=pin14, D5=pin15, D6=pin16, D7=pin17
    pin_d_[0] = socket.pin_signal(9);
    pin_d_[1] = socket.pin_signal(10);
    pin_d_[2] = socket.pin_signal(11);
    pin_d_[3] = socket.pin_signal(13);
    pin_d_[4] = socket.pin_signal(14);
    pin_d_[5] = socket.pin_signal(15);
    pin_d_[6] = socket.pin_signal(16);
    pin_d_[7] = socket.pin_signal(17);

    // Control
    pin_cs_  = socket.pin_signal(20);  // ~CS
    pin_vcc_ = socket.pin_signal(24);  // VCC

    // Subscribe to chip select (drives output changes)
    if (pin_cs_) pin_cs_->connect(this);
    if (pin_vcc_) pin_vcc_->connect(this);
}

void IC_ROM_8K::on_power_on() {
    update_outputs();
}

void IC_ROM_8K::on_power_off() {
    if (driving_) {
        for (int i = 0; i < 8; ++i) {
            if (pin_d_[i])
                pin_d_[i]->release();
        }
        driving_ = false;
    }
}

void IC_ROM_8K::on_signal_change() {
    update_outputs();
}

uint16_t IC_ROM_8K::read_address() const {
    uint16_t addr = 0;
    for (int i = 0; i < 13; ++i) {
        if (pin_a_[i] && pin_a_[i]->level() == Level::High)
            addr |= (1 << i);
    }
    return addr;
}

void IC_ROM_8K::update_outputs() {
    bool selected = pin_cs_ && pin_cs_->level() == Level::Low;

    if (selected) {
        uint16_t addr = read_address();
        uint8_t data = rom_[addr & 0x1FFF];
        spdlog::trace("[{}] ~CS=L addr={:04X} data={:02X}", name(), addr, data);
        for (int i = 0; i < 8; ++i) {
            if (pin_d_[i])
                pin_d_[i]->drive((data >> i) & 1 ? Level::High : Level::Low);
        }
        driving_ = true;
    } else if (driving_) {
        for (int i = 0; i < 8; ++i) {
            if (pin_d_[i])
                pin_d_[i]->release();
        }
        driving_ = false;
    }
}

} // namespace bench
