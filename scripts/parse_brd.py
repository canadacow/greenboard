"""Parse KiCad legacy BRD file to extract components and nets."""
import sys
import re
import json
from collections import defaultdict

sys.stdout.reconfigure(encoding='utf-8')

def parse_brd(filepath):
    with open(filepath, 'r', encoding='utf-8') as f:
        lines = f.readlines()

    # 1. Parse all nets (EQUIPOT sections)
    nets = {}
    i = 0
    while i < len(lines):
        line = lines[i].strip()
        if line == '$EQUIPOT':
            i += 1
            na_line = lines[i].strip()
            m = re.match(r'Na (\d+) "([^"]*)"', na_line)
            if m:
                net_id = int(m.group(1))
                net_name = m.group(2)
                if net_name:
                    nets[net_id] = net_name
        i += 1

    # 2. Parse all modules (components)
    components = []
    i = 0
    while i < len(lines):
        line = lines[i].strip()
        if line.startswith('$MODULE '):
            footprint = line[8:].strip()
            ref = '?'
            value = ''
            pos_x, pos_y = 0, 0
            pads = []

            i += 1
            while i < len(lines):
                mline = lines[i].strip()
                if mline.startswith('$EndMODULE'):
                    break

                # Reference (T0)
                if mline.startswith('T0 '):
                    m = re.search(r'"([^"]*)"', mline)
                    if m:
                        ref = m.group(1)

                # Value (T1)
                elif mline.startswith('T1 '):
                    m = re.search(r'"([^"]*)"', mline)
                    if m:
                        value = m.group(1)

                # Position
                elif mline.startswith('Po ') and not pads and ref == '?':
                    # Module position line (before pads)
                    parts = mline.split()
                    if len(parts) >= 3:
                        try:
                            pos_x = int(parts[1])
                            pos_y = int(parts[2])
                        except ValueError:
                            pass

                # Pad section
                elif mline == '$PAD':
                    pad_name = '?'
                    net_id = 0
                    net_name = ''
                    i += 1
                    while i < len(lines):
                        pline = lines[i].strip()
                        if pline == '$EndPAD':
                            break
                        if pline.startswith('Sh '):
                            m = re.search(r'"([^"]*)"', pline)
                            if m:
                                pad_name = m.group(1)
                        elif pline.startswith('Ne '):
                            m = re.match(r'Ne (\d+) "([^"]*)"', pline)
                            if m:
                                net_id = int(m.group(1))
                                net_name = m.group(2)
                        i += 1
                    pads.append({
                        'pin': pad_name,
                        'net_id': net_id,
                        'net_name': net_name
                    })

                i += 1

            components.append({
                'ref': ref,
                'footprint': footprint,
                'value': value,
                'pos': [pos_x, pos_y],
                'pads': pads,
                'pin_count': len(pads)
            })
        i += 1

    return nets, components


def categorize(ref):
    """Categorize component by reference designator prefix."""
    # Special multi-char prefixes first
    if ref.startswith('RN'):
        return 'RESISTOR_NETWORK'
    if ref.startswith('SW'):
        return 'SWITCH'
    if ref.startswith('TD'):
        return 'DELAY_LINE'
    if ref.startswith('SP'):
        return 'SPEAKER'

    prefix = re.match(r'([A-Za-z])', ref)
    if not prefix:
        return 'OTHER'
    p = prefix.group(1).upper()
    categories = {
        'U': 'IC',
        'R': 'RESISTOR',
        'C': 'CAPACITOR',
        'D': 'DIODE',
        'Q': 'TRANSISTOR',
        'L': 'INDUCTOR',
        'Y': 'CRYSTAL',
        'J': 'CONNECTOR',
        'P': 'CONNECTOR',
        'E': 'ISA_SLOT',
    }
    return categories.get(p, 'OTHER')


def main():
    filepath = r'c:\dev\bench\assets\pcb\64_256KB_SYSTEM_BOARD_rev1_2a.brd'
    nets, components = parse_brd(filepath)

    print(f"=== BRD PARSE RESULTS ===")
    print(f"Total nets: {len(nets)}")
    print(f"Total components: {len(components)}")

    # Group by category
    by_cat = defaultdict(list)
    for c in components:
        cat = categorize(c['ref'])
        by_cat[cat].append(c)

    for cat in sorted(by_cat.keys()):
        parts = sorted(by_cat[cat], key=lambda x: (re.match(r'([A-Za-z]+)', x['ref']).group(1) if re.match(r'([A-Za-z]+)', x['ref']) else '', int(re.search(r'(\d+)', x['ref']).group(1)) if re.search(r'(\d+)', x['ref']) else 0))
        print(f"\n{'='*70}")
        print(f"  {cat} ({len(parts)} components)")
        print(f"{'='*70}")
        for p in parts:
            nets_on = [pad for pad in p['pads'] if pad['net_id'] != 0]
            print(f"  {p['ref']:12s} {p['value']:25s} {p['footprint']:25s} pins={p['pin_count']:2d}  nets={len(nets_on)}")

    # Dump full JSON for later use
    with open(r'c:\dev\bench\scripts\brd_components.json', 'w') as f:
        json.dump({
            'nets': {str(k): v for k, v in nets.items()},
            'components': components
        }, f, indent=2)
    print(f"\n\nFull data written to scripts/brd_components.json")


if __name__ == '__main__':
    main()
