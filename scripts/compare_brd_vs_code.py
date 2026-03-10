"""Compare BRD components against motherboard.h to find discrepancies."""
import sys
import json
import re

sys.stdout.reconfigure(encoding='utf-8')

# Load BRD data
with open(r'c:\dev\bench\scripts\brd_components.json') as f:
    brd = json.load(f)

# Build BRD component map: ref -> {value, footprint, pin_count, pads}
brd_parts = {}
for c in brd['components']:
    ref = c['ref']
    if ref == '?':
        continue
    brd_parts[ref] = c

# Parse motherboard.h for sockets, signals, and other components
with open(r'c:\dev\bench\src\board\motherboard.h') as f:
    header = f.read()

# Find Socket declarations
code_sockets = {}
for m in re.finditer(r'Socket\s+(\w+)\s*\{\s*"([^"]+)"\s*,\s*"([^"]+)"\s*,\s*(\d+)\s*\}', header):
    var = m.group(1)
    ref = m.group(2)
    value = m.group(3)
    pins = int(m.group(4))
    code_sockets[ref] = {'var': var, 'value': value, 'pins': pins}

# RAM banks are created dynamically - add them
for bank, start, end in [('0', 37, 45), ('1', 53, 61), ('2', 69, 77), ('3', 85, 93)]:
    for i in range(start, end + 1):
        ref = f'U{i}'
        code_sockets[ref] = {'var': f'ram_bank{bank}[{i-start}]', 'value': '4164', 'pins': 16}

# Components in BRD but NOT ICs (passives, connectors, etc.)
# Check what's modeled in code
print("=" * 70)
print("  COMPARISON: BRD vs CODE")
print("=" * 70)

# 1. ICs in BRD
brd_ics = {ref: c for ref, c in brd_parts.items() if ref.startswith('U')}
print(f"\n--- ICs in BRD: {len(brd_ics)} ---")

missing_from_code = []
mismatched = []
for ref in sorted(brd_ics.keys(), key=lambda x: int(re.search(r'\d+', x).group())):
    brd_c = brd_ics[ref]
    if ref in code_sockets:
        code_c = code_sockets[ref]
        if brd_c['pin_count'] != code_c['pins']:
            mismatched.append((ref, brd_c, code_c))
    else:
        missing_from_code.append((ref, brd_c))

extra_in_code = []
for ref in sorted(code_sockets.keys(), key=lambda x: int(re.search(r'\d+', x).group()) if re.search(r'\d+', x) else 0):
    if ref.startswith('U') and ref not in brd_ics:
        extra_in_code.append((ref, code_sockets[ref]))

if missing_from_code:
    print(f"\n  MISSING FROM CODE ({len(missing_from_code)}):")
    for ref, c in missing_from_code:
        print(f"    {ref:8s} {c['value']:25s} {c['footprint']:25s} pins={c['pin_count']}")

if extra_in_code:
    print(f"\n  IN CODE BUT NOT IN BRD ({len(extra_in_code)}):")
    for ref, c in extra_in_code:
        print(f"    {ref:8s} {c['value']:25s} pins={c['pins']}")

if mismatched:
    print(f"\n  PIN COUNT MISMATCH ({len(mismatched)}):")
    for ref, brd_c, code_c in mismatched:
        print(f"    {ref:8s} BRD={brd_c['pin_count']} CODE={code_c['pins']}  (BRD: {brd_c['value']}, CODE: {code_c['value']})")

# 2. Value/part mismatches (74LS vs 74S etc)
print(f"\n--- IC VALUE COMPARISON ---")
for ref in sorted(set(brd_ics.keys()) & set(code_sockets.keys()), key=lambda x: int(re.search(r'\d+', x).group())):
    brd_val = brd_ics[ref]['value']
    code_val = code_sockets[ref]['value']
    # Normalize for comparison
    brd_norm = brd_val.upper().replace('-', '').replace('_', '')
    code_norm = code_val.upper().replace('-', '').replace('_', '')
    if brd_norm != code_norm:
        print(f"  {ref:8s} BRD={brd_val:20s} CODE={code_val}")

# 3. Non-IC components
print(f"\n--- NON-IC COMPONENTS IN BRD ---")
non_ic = {ref: c for ref, c in brd_parts.items() if not ref.startswith('U') and not ref.startswith('H')}
for cat_name, prefix_list in [
    ("RESISTORS", ['R']),
    ("RESISTOR NETWORKS", ['RN']),
    ("CAPACITORS", ['C']),
    ("CONNECTORS (ISA/KBD/CASS)", ['J', 'P']),
    ("SWITCHES", ['SW']),
    ("DELAY LINES", ['TD']),
    ("CRYSTAL", ['Y']),
    ("DIODE", ['D']),
    ("RELAY", ['K']),
    ("TRIMMER", ['VC']),
    ("SOCKET (8087)", ['XU']),
]:
    parts = {ref: c for ref, c in non_ic.items() if any(ref.startswith(p) and (len(p) > 1 or not ref[1:2].isalpha()) for p in prefix_list)}
    if parts:
        print(f"\n  {cat_name} ({len(parts)}):")
        for ref in sorted(parts.keys()):
            c = parts[ref]
            nets = [p for p in c['pads'] if p['net_id'] != 0]
            print(f"    {ref:8s} {c['value']:20s} {c['footprint']:20s} pins={c['pin_count']}  nets={len(nets)}")

# Check what the code models for passives
print(f"\n--- PASSIVE COMPONENTS IN CODE ---")
print(f"  DipSwitch sw1, sw2")
print(f"  PullResistor vector (pullups)")
print(f"  Jumper vector (jumpers)")
print(f"  IsaSlot j1-j5")

# Check for unnamed/mystery caps
unnamed_caps = [c for c in brd['components'] if c['ref'].startswith('C') and c['value'] in ['C***', 'C', '_']]
named_caps = [c for c in brd['components'] if c['ref'].startswith('C') and c['value'] not in ['C***', 'C', '_']]
print(f"\n--- CAPACITOR SUMMARY ---")
print(f"  Named/valued: {len(named_caps)}")
for c in sorted(named_caps, key=lambda x: x['ref']):
    print(f"    {c['ref']:8s} {c['value']}")
print(f"  Generic bypass (no specific value): {len(unnamed_caps)}")
