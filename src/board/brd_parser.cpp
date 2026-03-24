#include "board/brd_parser.h"
#include <fstream>
#include <sstream>
#include <regex>
#include <stdexcept>
#include <spdlog/spdlog.h>

namespace bench {

// Extract the first double-quoted string from a line.
static std::string extract_quoted(const std::string& line) {
    auto q1 = line.find('"');
    if (q1 == std::string::npos) return "";
    auto q2 = line.find('"', q1 + 1);
    if (q2 == std::string::npos) return "";
    return line.substr(q1 + 1, q2 - q1 - 1);
}

BrdData parse_brd(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open BRD file: " + filepath);
    }

    BrdData data;
    std::string line;

    while (std::getline(file, line)) {
        // Trim leading whitespace
        auto pos = line.find_first_not_of(" \t\r\n");
        if (pos == std::string::npos) continue;
        std::string trimmed = line.substr(pos);

        // --- Parse net definitions ($EQUIPOT) ---
        if (trimmed == "$EQUIPOT") {
            // Next line: Na <id> "<name>"
            if (!std::getline(file, line)) break;
            pos = line.find_first_not_of(" \t\r\n");
            if (pos != std::string::npos) {
                std::string na_line = line.substr(pos);
                if (na_line.substr(0, 3) == "Na ") {
                    // Parse: Na <id> "<name>"
                    std::istringstream ss(na_line.substr(3));
                    int net_id = 0;
                    ss >> net_id;
                    std::string net_name = extract_quoted(na_line);
                    if (!net_name.empty()) {
                        data.nets[net_id] = net_name;
                    }
                }
            }
            continue;
        }

        // --- Parse component modules ($MODULE) ---
        if (trimmed.substr(0, 8) == "$MODULE ") {
            std::string footprint = trimmed.substr(8);

            std::string ref = "?";
            std::string value;
            std::vector<BrdPad> pads;

            while (std::getline(file, line)) {
                pos = line.find_first_not_of(" \t\r\n");
                if (pos == std::string::npos) continue;
                std::string mline = line.substr(pos);

                if (mline.substr(0, 10) == "$EndMODULE") {
                    break;
                }

                // T0 line: reference designator (last quoted string)
                if (mline.substr(0, 3) == "T0 ") {
                    ref = extract_quoted(mline);
                }
                // T1 line: component value
                else if (mline.substr(0, 3) == "T1 ") {
                    value = extract_quoted(mline);
                }
                // Pad section
                else if (mline == "$PAD") {
                    std::string pad_name = "?";
                    int pad_net_id = 0;
                    std::string pad_net_name;

                    while (std::getline(file, line)) {
                        pos = line.find_first_not_of(" \t\r\n");
                        if (pos == std::string::npos) continue;
                        std::string pline = line.substr(pos);

                        if (pline == "$EndPAD") break;

                        // Sh "<pin_name>" ...
                        if (pline.substr(0, 3) == "Sh ") {
                            pad_name = extract_quoted(pline);
                        }
                        // Ne <net_id> "<net_name>"
                        else if (pline.substr(0, 3) == "Ne ") {
                            std::istringstream ss(pline.substr(3));
                            ss >> pad_net_id;
                            pad_net_name = extract_quoted(pline);
                        }
                    }

                    pads.push_back(BrdPad{pad_name, pad_net_id, pad_net_name});
                }
            }

            data.components.push_back(BrdComponent{
                std::move(ref),
                std::move(value),
                std::move(footprint),
                std::move(pads)
            });
        }
    }

    spdlog::info("BRD parsed: {} nets, {} components",
                 data.nets.size(), data.components.size());
    return data;
}

} // namespace bench
