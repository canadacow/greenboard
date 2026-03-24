#pragma once
#include <string>
#include <vector>
#include <unordered_map>

namespace bench {

// A single pad (pin) on a BRD component.
struct BrdPad {
    std::string pin;        // Pin name from BRD (usually "1", "2", etc.)
    int         net_id = 0; // Net ID (0 = unconnected)
    std::string net_name;   // Net name (empty = unconnected)
};

// A component parsed from the BRD file.
struct BrdComponent {
    std::string ref;        // Reference designator (e.g. "U3", "R1")
    std::string value;      // Part value (e.g. "8088", "18K")
    std::string footprint;  // Footprint name
    std::vector<BrdPad> pads;
};

// Parsed BRD data: nets and components.
struct BrdData {
    std::unordered_map<int, std::string> nets;  // net_id -> net_name
    std::vector<BrdComponent> components;
};

// Parse a KiCad legacy Pcbnew BRD file.
//
// Extracts:
//   - $EQUIPOT sections: net ID -> net name
//   - $MODULE sections: component ref, value, footprint, and per-pad net assignments
//
// The legacy format is line-oriented text:
//   $EQUIPOT / Na <id> "<name>" / $EndEQUIPOT
//   $MODULE <footprint> / T0 ... "<ref>" / T1 ... "<value>" /
//     $PAD / Sh "<pin>" ... / Ne <id> "<net>" / $EndPAD / $EndMODULE
BrdData parse_brd(const std::string& filepath);

} // namespace bench
