#pragma once
#include "core/signal.h"
#include <cstdint>

namespace bench {

// IBM 5150 SW1 configuration block.
//
// Read via PPI Port A. The BIOS reads these bits directly.
// Call drive() after configuration to push values onto signals.
struct SW1Config {
    bool floppy_present   = true;   // SW1-1: diskette drives installed
    bool math_coprocessor = false;  // SW1-2: 8087 installed
    int  planar_ram_kb    = 64;     // SW1-3,4: 16, 32, 48, or 64
    enum Video { NONE=0, CGA_40=1, CGA_80=2, MDA=3 };
    Video video_mode      = MDA;    // SW1-5,6: display adapter type
    int  floppy_count     = 1;      // SW1-7,8: 1-4 floppy drives

    // Build the byte that PPI Port A reads.
    uint8_t value() const {
        uint8_t v = 0;
        if (floppy_present)   v |= 0x01;
        if (math_coprocessor) v |= 0x02;
        // RAM: 16K=00, 32K=01, 48K=10, 64K=11 in bits 2-3
        int ram_code = (planar_ram_kb / 16) - 1;
        if (ram_code < 0) ram_code = 0;
        if (ram_code > 3) ram_code = 3;
        v |= (ram_code & 3) << 2;
        v |= (video_mode & 3) << 4;
        // Floppy count: 1=00, 2=01, 3=10, 4=11 in bits 6-7
        int fc = floppy_count - 1;
        if (fc < 0) fc = 0;
        if (fc > 3) fc = 3;
        v |= (fc & 3) << 6;
        return v;
    }

    void drive(Signal* out[8]) const {
        uint8_t v = value();
        for (int i = 0; i < 8; ++i)
            out[i]->drive((v >> i) & 1 ? Level::High : Level::Low);
    }
};

// IBM 5150 SW2 configuration block.
//
// Read via PPI Port C lower nibble. Reports expansion RAM size.
struct SW2Config {
    int expansion_ram_banks = 0;    // 0-15: number of 32K RAM banks on ISA cards

    uint8_t value() const {
        return static_cast<uint8_t>(expansion_ram_banks & 0x0F);
    }

    void drive(Signal* out[4]) const {
        uint8_t v = value();
        for (int i = 0; i < 4; ++i)
            out[i]->drive((v >> i) & 1 ? Level::High : Level::Low);
    }
};

} // namespace bench
