"""Verify motherboard.h has all BRD components represented."""
import sys
import json
import re

sys.stdout.reconfigure(encoding='utf-8')

# Load BRD data
with open(r'c:\dev\bench\scripts\brd_components.json') as f:
    brd = json.load(f)

# Load motherboard.h
with open(r'c:\dev\bench\src\board\motherboard.h') as f:
    header = f.read()

# Build BRD component set (skip mounting holes and unnamed)
brd_refs = set()
for c in brd['components']:
    ref = c['ref']
    if ref == '?' or ref.startswith('H'):  # skip holes
        continue
    brd_refs.add(ref)

# Find all component declarations in header
# Socket declarations
code_refs = set()
for m in re.finditer(r'Socket\s+\w+\s*\{\s*"([^"]+)"', header):
    code_refs.add(m.group(1))
# RAM banks (dynamically created)
for bank, start, end in [('0', 37, 45), ('1', 53, 61), ('2', 69, 77), ('3', 85, 93)]:
    for i in range(start, end + 1):
        code_refs.add(f'U{i}')
# IsaSlot
for m in re.finditer(r'IsaSlot\s+\w+\s*\{\s*"([^"]+)"', header):
    code_refs.add(m.group(1))
# DipSwitch
for m in re.finditer(r'DipSwitch\s+\w+\s*\{\s*"([^"]+)"', header):
    code_refs.add(m.group(1))
# Connector
for m in re.finditer(r'Connector\s+\w+\s*\{\s*"([^"]+)"', header):
    code_refs.add(m.group(1))
# Resistor
for m in re.finditer(r'Resistor\s+\w+\s*\{\s*"([^"]+)"', header):
    code_refs.add(m.group(1))
# ResistorNetwork
for m in re.finditer(r'ResistorNetwork\s+\w+\s*\{\s*"([^"]+)"', header):
    code_refs.add(m.group(1))
# Capacitor
for m in re.finditer(r'Capacitor\s+\w+\s*\{\s*"([^"]+)"', header):
    code_refs.add(m.group(1))
# Crystal
for m in re.finditer(r'Crystal\s+\w+\s*\{\s*"([^"]+)"', header):
    code_refs.add(m.group(1))
# Diode
for m in re.finditer(r'Diode\s+\w+\s*\{\s*"([^"]+)"', header):
    code_refs.add(m.group(1))
# Relay
for m in re.finditer(r'Relay\s+\w+\s*\{\s*"([^"]+)"', header):
    code_refs.add(m.group(1))
# Trimmer
for m in re.finditer(r'Trimmer\s+\w+\s*\{\s*"([^"]+)"', header):
    code_refs.add(m.group(1))

print(f"BRD components (excl holes): {len(brd_refs)}")
print(f"Code components: {len(code_refs)}")

missing = brd_refs - code_refs
extra = code_refs - brd_refs

if missing:
    print(f"\nMISSING FROM CODE ({len(missing)}):")
    for ref in sorted(missing):
        # Find BRD info
        for c in brd['components']:
            if c['ref'] == ref:
                print(f"  {ref:10s} {c['value']:20s} {c['footprint']}")
                break

if extra:
    print(f"\nIN CODE BUT NOT IN BRD ({len(extra)}):")
    for ref in sorted(extra):
        print(f"  {ref}")

if not missing and not extra:
    print("\nPERFECT MATCH! All BRD components are represented in code.")
