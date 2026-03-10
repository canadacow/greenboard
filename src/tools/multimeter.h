#pragma once
#include "board/motherboard.h"
#include "board/brd_parser.h"
#include <string>

namespace bench {

// A virtual multimeter / continuity tester for the bare motherboard.
//
// Like a real multimeter in continuity mode: touch two pins, beep if
// they're on the same net. Can also drive a signal on one pin and
// read it on another to verify the trace is live.
//
// Works entirely through socket pin_signal() -- probes the wiring,
// never needs to be inserted into a socket.
class Multimeter {
public:
    explicit Multimeter(Motherboard& mb);

    // --- Continuity ---

    // Check if two socket pins are on the same net.
    // Returns true if both pins resolve to the same non-null Signal*.
    bool continuity(const std::string& ref_a, int pin_a,
                    const std::string& ref_b, int pin_b) const;

    // Probe a socket pin: returns the Signal* it's wired to (null = NC).
    Signal* probe(const std::string& ref, int pin) const;

    // Return the net name of the signal on a socket pin (empty = NC).
    std::string net_name(const std::string& ref, int pin) const;

    // --- Drive / Read ---

    // Drive a level onto a socket pin's signal. Like clipping a lead.
    void drive(const std::string& ref, int pin, Level lvl);

    // Read the level on a socket pin's signal.
    Level read(const std::string& ref, int pin) const;

    // Release (stop driving) a socket pin's signal.
    void release(const std::string& ref, int pin);

    // --- Full board audit ---

    // Run a continuity test across the entire board using the BRD netlist.
    // For every net, verifies that all pins sharing that net point to the
    // same Signal object. Returns the number of failures (0 = all good).
    int audit(const std::string& brd_path) const;

private:
    Motherboard& mb_;

    // Find a socket by ref designator. Returns nullptr if not found.
    Socket* find_socket(const std::string& ref) const;
};

} // namespace bench
