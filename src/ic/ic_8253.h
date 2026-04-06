#pragma once
#include "core/callback_component.h"
#include "board/socket.h"

namespace bench {

// Intel 8253-5 Programmable Interval Timer.
//
// 24-pin DIP. Three independent 16-bit down-counters, each with CLK,
// GATE, and OUT pins. Programmed via data bus using ~CS, ~RD, ~WR.
//
// Each call to on_cycle() is exactly one full PIT clock cycle.
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

    void save(cereal::BinaryOutputArchive& ar) override { serialize(ar); }
    void load(cereal::BinaryInputArchive& ar) override { serialize(ar); }
    template <class Archive> void serialize(Archive& ar) {
        for (int i = 0; i < 3; ++i)
            ar(channels_[i].mode, channels_[i].bcd, channels_[i].rw_mode,
               channels_[i].programmed, channels_[i].count, channels_[i].reload,
               channels_[i].latch, channels_[i].latched,
               channels_[i].load_lsb_pending, channels_[i].load_lsb_value,
               channels_[i].read_msb_next, channels_[i].out, channels_[i].gate,
               channels_[i].counting, channels_[i].loaded, channels_[i].null_count);
        ar(data_bus_driven_, write_pending_, read_pending_, wr_prev_, rd_prev_,
           pit_timer_);
    }

    // Debug accessors for channel state
    struct ChannelInfo {
        uint8_t mode; uint8_t rw_mode; uint32_t count; uint32_t reload;
        bool out; bool gate; bool counting; bool loaded; bool null_count;
    };
    ChannelInfo channel_info(int ch) const {
        auto& c = channels_[ch];
        return {c.mode, c.rw_mode, c.count, c.reload,
                c.out, c.gate, c.counting, c.loaded, c.null_count};
    }

protected:
    void on_power_on() override;
    void on_cycle(Fiber caller) override;

private:
    struct Channel {
        uint8_t mode = 0;
        bool bcd = false;
        uint8_t rw_mode = 0;       // 1=LSB, 2=MSB, 3=LSB+MSB
        bool programmed = false;

        uint32_t count = 0;        // Current counting element
        uint32_t reload = 0;       // Reload value (0 from software = 65536)
        uint16_t latch = 0;        // Latched count for reading
        bool latched = false;

        bool load_lsb_pending = false;
        uint8_t load_lsb_value = 0;
        bool read_msb_next = false;

        bool out = false;
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
    uint32_t decrement(uint32_t val, bool bcd);

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
    bool wr_prev_ = false;  // ~WR was low last cycle
    bool rd_prev_ = false;  // ~RD was low last cycle

    uint64_t pit_timer_ = 0;
};

} // namespace bench
