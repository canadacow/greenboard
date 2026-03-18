#pragma once
#include "core/callback_component.h"
#include "board/socket.h"

namespace bench {

// Intel 8253-5 Programmable Interval Timer.
//
// 24-pin DIP. Three independent 16-bit down-counters, each with CLK,
// GATE, and OUT pins. Programmed via data bus using ~CS, ~RD, ~WR.
//
// Each call to on_signal_change() is exactly one full PIT clock cycle.
// The bidir lambda (sampled before evaluate) determines what the PIT does:
//   Input  (~CS+~WR low): read data from bus, handle write
//   Output (~CS+~RD low): drive data bus with counter value
//   HiZ:                  just tick counters
// After handling any bus operation, all channels tick once.
//
// Counter modes:
//   Mode 0: Interrupt on terminal count
//   Mode 1: Hardware retriggerable one-shot
//   Mode 2: Rate generator (periodic pulse)
//   Mode 3: Square wave generator
//   Mode 4: Software triggered strobe
//   Mode 5: Hardware triggered strobe
class IC_8253 : public CallbackComponent {
public:
    IC_8253();

    void install(Socket& socket);

protected:
    void on_power_on() override;
    void on_signal_change(Fiber caller) override;

private:
    struct Channel {
        uint8_t mode = 0;
        bool bcd = false;
        uint8_t rw_mode = 0;       // 1=LSB, 2=MSB, 3=LSB+MSB
        bool programmed = false;

        uint16_t count = 0;        // Current counting element
        uint16_t reload = 0;       // Reload value
        uint16_t latch = 0;        // Latched count for reading
        bool latched = false;

        bool load_lsb_pending = false;
        uint8_t load_lsb_value = 0;
        bool read_msb_next = false;

        bool out = true;
        bool gate = true;
        bool counting = false;
        bool loaded = false;
        bool null_count = true;
    };

    void handle_write();
    void handle_read();
    void tick(int ch);
    void write_control(uint8_t value);
    void write_counter(int ch, uint8_t value);
    uint8_t read_counter(int ch);
    void update_out(int ch);
    void drive_data_bus(uint8_t value);
    void release_data_bus();
    uint16_t decrement(uint16_t val, bool bcd);

    Channel channels_[3];

    Pin pin_out_[3];
    Pin pin_clk_[3];
    Pin pin_gate_[3];
    Pin pin_data_[8];
    Pin pin_a0_, pin_a1_;
    Pin pin_cs_, pin_rd_, pin_wr_;
    Pin pin_vcc_;

    bool data_bus_driven_ = false;
    bool write_pending_ = false;
    bool read_pending_ = false;
};

} // namespace bench
