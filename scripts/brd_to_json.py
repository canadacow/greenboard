"""Extract PCB traces, vias, and component outlines from KiCad legacy BRD file.

Outputs a JSON file with all non-power-rail geometry for board visualization.

Usage: .venv/Scripts/python.exe scripts/brd_to_json.py
"""
import sys, re, json, math

sys.stdout.reconfigure(encoding='utf-8')

BRD_PATH = "assets/pcb/64_256KB_SYSTEM_BOARD_rev1_2a.brd"
OUT_PATH = "assets/board_traces.json"

# Power rail net numbers to exclude
POWER_NETS = {0, 1, 2, 3, 4, 84}  # "", +12V, +5V, -12V, -5V, GND

# KiCad legacy units: 0.0001 inch (1/10 mil). Convert to mils for sanity.
def to_mils(v):
    return v / 10.0

def parse_brd(path):
    with open(path, 'r', encoding='utf-8', errors='replace') as f:
        content = f.read()

    # --- Board bounds ---
    m = re.search(r'^Di\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)', content, re.MULTILINE)
    bounds = {
        "x_min": to_mils(int(m.group(1))),
        "y_min": to_mils(int(m.group(2))),
        "x_max": to_mils(int(m.group(3))),
        "y_max": to_mils(int(m.group(4))),
    }

    # --- Layers ---
    layers = {}
    for lm in re.finditer(r'^Layer\[(\d+)\]\s+(\S+)\s+(\S+)', content, re.MULTILINE):
        layers[int(lm.group(1))] = {"name": lm.group(2), "type": lm.group(3)}

    # --- Nets (EQUIPOT) ---
    nets = {}
    for nm in re.finditer(r'^\$EQUIPOT\s*\n\s*Na\s+(\d+)\s+"([^"]*)"\s*\n',
                          content, re.MULTILINE):
        net_id = int(nm.group(1))
        nets[net_id] = nm.group(2)

    # --- Tracks & Vias ---
    track_section = content.split('$TRACK')[1].split('$EndTRACK')[0]
    track_lines = track_section.strip().split('\n')

    traces = []   # line segments
    vias = []     # through-hole vias

    i = 0
    while i < len(track_lines):
        line = track_lines[i].strip()
        if line.startswith('Po '):
            po_parts = line.split()
            # Po shape x1 y1 x2 y2 width drill
            shape = int(po_parts[1])
            x1 = to_mils(int(po_parts[2]))
            y1 = to_mils(int(po_parts[3]))
            x2 = to_mils(int(po_parts[4]))
            y2 = to_mils(int(po_parts[5]))
            width = to_mils(int(po_parts[6]))
            drill = int(po_parts[7]) if len(po_parts) > 7 else -1

            # Next line should be De
            if i + 1 < len(track_lines):
                de_line = track_lines[i + 1].strip()
                if de_line.startswith('De '):
                    de_parts = de_line.split()
                    layer = int(de_parts[1])
                    net_code = int(de_parts[3])
                    i += 2

                    # Skip power rails
                    if net_code in POWER_NETS:
                        continue

                    net_name = nets.get(net_code, "")

                    if shape == 3 or (x1 == x2 and y1 == y2 and drill > 0):
                        # Via
                        vias.append({
                            "x": x1, "y": y1,
                            "dia": width,
                            "drill": to_mils(drill) if drill > 0 else width * 0.6,
                            "net": net_code,
                        })
                    else:
                        # Track segment
                        traces.append({
                            "x1": x1, "y1": y1,
                            "x2": x2, "y2": y2,
                            "w": width,
                            "layer": layer,
                            "net": net_code,
                        })
                    continue
        i += 1

    # --- Components (MODULEs) ---
    components = []
    for mod_block in content.split('$MODULE')[1:]:
        lines = mod_block.strip().split('\n')

        # First line is footprint name (remainder of $MODULE line)
        footprint = lines[0].strip()

        # Po line: x y orientation layer timestamp1 timestamp2 ~~
        ref = None
        value = None
        mod_x = mod_y = mod_orient = 0

        pads = []
        outline_segs = []

        for li, line in enumerate(lines):
            line = line.strip()

            if line.startswith('Po ') and li < 5:
                parts = line.split()
                mod_x = to_mils(int(parts[1]))
                mod_y = to_mils(int(parts[2]))
                mod_orient = int(parts[3])  # tenths of degree

            elif line.startswith('T0 '):
                m = re.search(r'"(.+?)"', line)
                if m:
                    ref = m.group(1)

            elif line.startswith('T1 '):
                m = re.search(r'"(.+?)"', line)
                if m:
                    value = m.group(1)

            elif line.startswith('DS '):
                # Draw segment (outline): DS x1 y1 x2 y2 width layer
                parts = line.split()
                if len(parts) >= 7:
                    outline_segs.append({
                        "x1": to_mils(int(parts[1])),
                        "y1": to_mils(int(parts[2])),
                        "x2": to_mils(int(parts[3])),
                        "y2": to_mils(int(parts[4])),
                    })

        # Parse pads
        for pad_block in mod_block.split('$PAD')[1:]:
            pad_lines = pad_block.strip().split('\n')
            pad_name = None
            pad_x = pad_y = 0
            pad_net = 0
            pad_shape = "C"
            pad_w = pad_h = 55.0  # default

            for pl in pad_lines:
                pl = pl.strip()
                if pl.startswith('Sh '):
                    parts = pl.split()
                    pad_name = parts[1].strip('"')
                    pad_shape = parts[2]  # R=rect, C=circle, O=oval
                    pad_w = to_mils(int(parts[3]))
                    pad_h = to_mils(int(parts[4]))
                elif pl.startswith('Po '):
                    parts = pl.split()
                    pad_x = to_mils(int(parts[1]))
                    pad_y = to_mils(int(parts[2]))
                elif pl.startswith('Ne '):
                    m = re.match(r'Ne\s+(\d+)\s+"(.*)"', pl)
                    if m:
                        pad_net = int(m.group(1))

            if pad_name is not None:
                pads.append({
                    "name": pad_name,
                    "x": pad_x, "y": pad_y,
                    "w": pad_w, "h": pad_h,
                    "shape": pad_shape,
                    "net": pad_net,
                })

        if ref:
            components.append({
                "ref": ref,
                "value": value or "",
                "footprint": footprint,
                "x": mod_x,
                "y": mod_y,
                "orient": mod_orient,
                "pads": pads,
                "outline": outline_segs,
            })

    # --- Board outline (DRAWSEGMENT on edge layer 28) ---
    board_outline = []
    for ds_match in re.finditer(
        r'\$DRAWSEGMENT\s*\n(.*?)\$EndDRAWSEGMENT', content, re.DOTALL
    ):
        block = ds_match.group(1)
        layer_m = re.search(r'^De\s+(\d+)', block, re.MULTILINE)
        if layer_m and int(layer_m.group(1)) == 28:  # edge cuts layer
            po_m = re.search(r'^Po\s+\d+\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)',
                             block, re.MULTILINE)
            if po_m:
                board_outline.append({
                    "x1": to_mils(int(po_m.group(1))),
                    "y1": to_mils(int(po_m.group(2))),
                    "x2": to_mils(int(po_m.group(3))),
                    "y2": to_mils(int(po_m.group(4))),
                })

    # --- Net name lookup (non-power only) ---
    net_names = {}
    for nid, nname in nets.items():
        if nid not in POWER_NETS:
            net_names[str(nid)] = nname

    return {
        "units": "mils",
        "bounds": bounds,
        "layers": {str(k): v for k, v in layers.items()},
        "net_names": net_names,
        "board_outline": board_outline,
        "components": components,
        "traces": traces,
        "vias": vias,
        "stats": {
            "total_nets": len(nets),
            "signal_nets": len(net_names),
            "total_traces": len(traces),
            "total_vias": len(vias),
            "total_components": len(components),
        }
    }


def main():
    print(f"Parsing {BRD_PATH}...")
    data = parse_brd(BRD_PATH)

    print(f"Board bounds: ({data['bounds']['x_min']:.0f}, {data['bounds']['y_min']:.0f}) "
          f"to ({data['bounds']['x_max']:.0f}, {data['bounds']['y_max']:.0f}) mils")
    print(f"Components: {data['stats']['total_components']}")
    print(f"Signal nets: {data['stats']['signal_nets']} "
          f"(of {data['stats']['total_nets']} total, {len(POWER_NETS)} power excluded)")
    print(f"Traces: {data['stats']['total_traces']}")
    print(f"Vias: {data['stats']['total_vias']}")
    print(f"Board outline segments: {len(data['board_outline'])}")

    with open(OUT_PATH, 'w') as f:
        json.dump(data, f, separators=(',', ':'))

    # Also write a pretty version for inspection
    with open(OUT_PATH.replace('.json', '_pretty.json'), 'w') as f:
        json.dump(data["stats"], f, indent=2)

    import os
    size_mb = os.path.getsize(OUT_PATH) / (1024 * 1024)
    print(f"\nWrote {OUT_PATH} ({size_mb:.1f} MB)")
    print("Done.")


if __name__ == '__main__':
    main()
