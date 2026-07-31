#pragma once
// INS8250 -- National Semiconductor asynchronous UART (the original
// 8250, as on the IBM Asynchronous Communications Adapter and the AST
// SixPakPlus). Behavioral model behind the ISA card abstraction: no
// pins, no DAG -- the owning card feeds it I/O reads/writes and crystal
// time, and polls its interrupt line.
//
// Original-8250 fidelity notes:
//   - No scratch register (that arrived with the 8250A/16450): reads
//     of register 7 float to 0xFF.
//   - No FIFO (16550): IIR bits 6-7 always read 0.
//   - The THRE interrupt quirk: enabling IER bit 1 while the holding
//     register is already empty immediately latches a THRE interrupt.
//     Period drivers depend on this to kick-start transmission.
//   - MCR OUT2 gates the IRQ driver to the bus (IBM adapter wiring).
//
// Timing is paced in 1.8432 MHz crystal cycles fed by the owner:
// baud = 1843200 / (16 * divisor), one character frame = start + data
// + parity + stop bits at 16 crystal cycles per bit time.

#include <cereal/cereal.hpp>
#include <cstdint>

namespace bench {

// The far end of the cable. Implementations: null (nothing attached),
// serial mouse, host bridge, ... Not serialized -- the owning card
// rebinds devices after a save-state load.
class SerialDevice {
public:
    virtual ~SerialDevice() = default;
    // A byte finished shifting out the TX line.
    virtual void on_tx(uint8_t /*byte*/) {}
    // Offer a byte to the receiver. Called once per character time.
    virtual bool poll_rx(uint8_t& /*out*/) { return false; }
    // Modem control outputs changed.
    virtual void on_modem(bool /*dtr*/, bool /*rts*/) {}
    // Live modem inputs: bit0=CTS, bit1=DSR, bit2=RI, bit3=DCD.
    virtual uint8_t modem_in() { return 0; }
};

class INS8250 {
public:
    void set_device(SerialDevice* dev) { dev_ = dev; }

    void reset() {
        dll_ = 1; dlm_ = 0;
        ier_ = 0; lcr_ = 0; mcr_ = 0;
        lsr_ = LSR_THRE | LSR_TEMT;
        msr_ = 0;
        rbr_ = 0; thr_ = 0; tsr_ = 0;
        thr_full_ = false; tsr_busy_ = false;
        pend_thre_ = false;
        tx_left_ = 0; rx_left_ = 0;
        prev_modem_ = 0;
    }

    // reg = A0-A2 (port & 7).
    uint8_t read(uint8_t reg) {
        switch (reg) {
            case 0:
                if (lcr_ & 0x80) return dll_;
                lsr_ &= ~LSR_DR;
                return rbr_;
            case 1:
                return (lcr_ & 0x80) ? dlm_ : ier_;
            case 2: {
                // IIR: bit0 = 1 when NO interrupt pending; bits 1-2 id
                // the highest-priority source. Reading IIR clears a
                // reported THRE interrupt.
                uint8_t id;
                if (lsr_ & (LSR_OE | LSR_PE | LSR_FE | LSR_BI) && (ier_ & 0x04)) {
                    id = 0x06;                     // receiver line status
                } else if ((lsr_ & LSR_DR) && (ier_ & 0x01)) {
                    id = 0x04;                     // received data available
                } else if (pend_thre_ && (ier_ & 0x02)) {
                    id = 0x02;                     // THRE
                    pend_thre_ = false;
                } else if ((msr_ & 0x0F) && (ier_ & 0x08)) {
                    id = 0x00;                     // modem status
                } else {
                    return 0x01;                   // none pending
                }
                return id;
            }
            case 3: return lcr_;
            case 4: return mcr_;
            case 5: {
                uint8_t v = lsr_;
                lsr_ &= ~(LSR_OE | LSR_PE | LSR_FE | LSR_BI);
                return v;
            }
            case 6: {
                uint8_t v = msr_;
                msr_ &= 0xF0;                      // reading clears deltas
                return v;
            }
            default:
                return 0xFF;                       // no scratch on the 8250
        }
    }

    void write(uint8_t reg, uint8_t val) {
        switch (reg) {
            case 0:
                if (lcr_ & 0x80) { dll_ = val; return; }
                thr_ = val;
                thr_full_ = true;
                pend_thre_ = false;
                lsr_ &= ~(LSR_THRE | LSR_TEMT);
                load_shifter();
                return;
            case 1:
                if (lcr_ & 0x80) { dlm_ = val; return; }
                {
                    bool was_enabled = (ier_ & 0x02) != 0;
                    ier_ = val & 0x0F;
                    // THRE quirk: enabling the interrupt while the
                    // holding register is empty latches it immediately.
                    if (!was_enabled && (ier_ & 0x02) && (lsr_ & LSR_THRE))
                        pend_thre_ = true;
                }
                return;
            case 2: return;                        // IIR is read-only
            case 3: lcr_ = val; return;
            case 4:
                mcr_ = val & 0x1F;
                if (dev_ && !(mcr_ & MCR_LOOP))
                    dev_->on_modem((mcr_ & 0x01) != 0, (mcr_ & 0x02) != 0);
                return;
            case 5: return;                        // LSR not writable
            case 6: return;                        // MSR not writable
            default: return;
        }
    }

    // Advance time by `cycles` of the 1.8432 MHz crystal.
    void tick(uint32_t cycles) {
        // Transmit shifter.
        if (tsr_busy_) {
            if (tx_left_ > cycles) {
                tx_left_ -= cycles;
            } else {
                tsr_busy_ = false;
                uint8_t sent = tsr_;
                if (mcr_ & MCR_LOOP)
                    receive(sent);
                else if (dev_)
                    dev_->on_tx(sent);
                load_shifter();
                if (!tsr_busy_)
                    lsr_ |= LSR_TEMT;
            }
        }

        // Receive pacing: offer the wire one character slot per frame.
        if (!(mcr_ & MCR_LOOP)) {
            if (rx_left_ > cycles) {
                rx_left_ -= cycles;
            } else {
                rx_left_ = frame_cycles();
                uint8_t b;
                if (dev_ && dev_->poll_rx(b))
                    receive(b);
            }
        }

        // Modem status inputs -> MSR live bits + change deltas.
        uint8_t in;
        if (mcr_ & MCR_LOOP) {
            // Loopback maps DTR->DSR, RTS->CTS, OUT1->RI, OUT2->DCD.
            in = (uint8_t)(((mcr_ & 0x01) ? 0x02 : 0) |
                           ((mcr_ & 0x02) ? 0x01 : 0) |
                           ((mcr_ & 0x04) ? 0x04 : 0) |
                           ((mcr_ & 0x08) ? 0x08 : 0));
        } else {
            in = dev_ ? (uint8_t)(dev_->modem_in() & 0x0F) : 0;
        }
        if (in != prev_modem_) {
            uint8_t changed = in ^ prev_modem_;
            if (changed & 0x01) msr_ |= 0x01;              // delta CTS
            if (changed & 0x02) msr_ |= 0x02;              // delta DSR
            if ((prev_modem_ & 0x04) && !(in & 0x04))
                msr_ |= 0x04;                              // RI trailing edge
            if (changed & 0x08) msr_ |= 0x08;              // delta DCD
            prev_modem_ = in;
        }
        msr_ = (uint8_t)((msr_ & 0x0F) |
                         ((in & 0x01) ? 0x10 : 0) |        // CTS
                         ((in & 0x02) ? 0x20 : 0) |        // DSR
                         ((in & 0x04) ? 0x40 : 0) |        // RI
                         ((in & 0x08) ? 0x80 : 0));        // DCD
    }

    // IRQ line to the bus, gated by MCR OUT2 (IBM adapter wiring).
    bool irq_pending() const {
        if (!(mcr_ & 0x08))
            return false;
        if ((lsr_ & (LSR_OE | LSR_PE | LSR_FE | LSR_BI)) && (ier_ & 0x04)) return true;
        if ((lsr_ & LSR_DR) && (ier_ & 0x01)) return true;
        if (pend_thre_ && (ier_ & 0x02)) return true;
        if ((msr_ & 0x0F) && (ier_ & 0x08)) return true;
        return false;
    }

    template <class Archive> void serialize(Archive& ar) {
        ar(dll_, dlm_, ier_, lcr_, mcr_, lsr_, msr_, rbr_, thr_, tsr_,
           thr_full_, tsr_busy_, pend_thre_,
           tx_left_, rx_left_, prev_modem_);
    }

private:
    static constexpr uint8_t LSR_DR   = 0x01;
    static constexpr uint8_t LSR_OE   = 0x02;
    static constexpr uint8_t LSR_PE   = 0x04;
    static constexpr uint8_t LSR_FE   = 0x08;
    static constexpr uint8_t LSR_BI   = 0x10;
    static constexpr uint8_t LSR_THRE = 0x20;
    static constexpr uint8_t LSR_TEMT = 0x40;
    static constexpr uint8_t MCR_LOOP = 0x10;

    uint32_t divisor() const {
        uint32_t d = ((uint32_t)dlm_ << 8) | dll_;
        return d ? d : 1;
    }
    // Crystal cycles per character frame: 16 per bit time; start bit +
    // 5-8 data + optional parity + 1-2 stop (1.5 rounds to 2).
    uint32_t frame_cycles() const {
        uint32_t bits = 1 + (5 + (lcr_ & 3)) + ((lcr_ & 0x08) ? 1 : 0) +
                        ((lcr_ & 0x04) ? 2 : 1);
        return bits * divisor() * 16;
    }

    void load_shifter() {
        if (!thr_full_ || tsr_busy_)
            return;
        tsr_ = thr_;
        tsr_busy_ = true;
        tx_left_ = frame_cycles();
        thr_full_ = false;
        lsr_ |= LSR_THRE;
        pend_thre_ = true;
    }

    void receive(uint8_t b) {
        if (lsr_ & LSR_DR)
            lsr_ |= LSR_OE;    // overrun: new character overwrites RBR
        rbr_ = b;
        lsr_ |= LSR_DR;
    }

    SerialDevice* dev_ = nullptr;

    uint8_t dll_ = 1, dlm_ = 0;
    uint8_t ier_ = 0, lcr_ = 0, mcr_ = 0;
    uint8_t lsr_ = LSR_THRE | LSR_TEMT;
    uint8_t msr_ = 0;
    uint8_t rbr_ = 0, thr_ = 0, tsr_ = 0;
    bool thr_full_ = false;
    bool tsr_busy_ = false;
    bool pend_thre_ = false;
    uint32_t tx_left_ = 0;
    uint32_t rx_left_ = 0;
    uint8_t prev_modem_ = 0;
};

} // namespace bench
