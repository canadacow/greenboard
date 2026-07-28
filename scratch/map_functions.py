"""Systematic function map for Pirates!, built from the boot sector outward.

No guessing. Every label is justified by one of:
  (a) the boot sector's own control flow,
  (b) a BIOS interrupt the function issues (INT 10/13/16),
  (c) hardware ports it touches,
  (d) a call edge from an already-labelled function.

Output: scratch/out/function_map.txt  -- address, size, callers, callees,
        interrupts used, ports used, and a label where justified.
"""
import sys, struct, collections
sys.stdout.reconfigure(encoding='utf-8')
sys.path.insert(0, 'scratch')
from stage2 import build_image
from capstone import Cs, CS_ARCH_X86, CS_MODE_16

img = build_image('assets/pirates_1.img')
md = Cs(CS_ARCH_X86, CS_MODE_16)
md.detail = True

# ---------------------------------------------------------------- call graph
# Collect near CALLs (E8) and the interrupts / ports each region issues.
calls = collections.defaultdict(set)      # target -> {callers}
callees = collections.defaultdict(set)    # caller -> {targets}
ints = collections.defaultdict(set)       # func -> {int numbers}
ports = collections.defaultdict(set)      # func -> {port numbers}

# First pass: find every E8 near-call target; these are function starts.
targets = set()
for i in range(len(img) - 3):
    if img[i] == 0xE8:
        d = int.from_bytes(img[i+1:i+3], 'little', signed=True)
        t = (i + 3 + d) & 0xFFFF
        if 0 <= t < len(img):
            targets.add(t)

# Entry point from the boot sector: LJMP 0050:0020
targets.add(0x20)
starts = sorted(targets)
print(f'{len(starts)} call targets (candidate functions)')

# Second pass: attribute calls/ints/ports to the enclosing function.
def enclosing(addr):
    lo, hi = 0, len(starts) - 1
    best = None
    while lo <= hi:
        mid = (lo + hi) // 2
        if starts[mid] <= addr:
            best = starts[mid]; lo = mid + 1
        else:
            hi = mid - 1
    return best

for i in range(len(img) - 3):
    b = img[i]
    if b == 0xE8:
        d = int.from_bytes(img[i+1:i+3], 'little', signed=True)
        t = (i + 3 + d) & 0xFFFF
        # Record the CALL SITE itself, not a guessed enclosing function.
        if t in targets:
            calls[t].add(i)
    elif b == 0xCD:                      # INT imm8
        f = enclosing(i)
        if f is not None:
            ints[f].add(img[i+1])
    elif b in (0xE4, 0xE6):              # IN/OUT imm8
        f = enclosing(i)
        if f is not None:
            ports[f].add(img[i+1])
    elif b in (0xEC, 0xEE):              # IN/OUT DX -- look back for MOV DX,imm
        f = enclosing(i)
        if f is not None and i >= 3 and img[i-3] == 0xBA:
            ports[f].add(int.from_bytes(img[i-2:i], 'little'))

# ---------------------------------------------------------------- labelling
# Facts established by reading the boot sector and the disassembly directly.
LABEL = {
    0x0020: 'entry_stage2            (LJMP target from boot sector)',
    0x0080: 'main                    (called by entry)',
    0x04e0: 'startup_video_and_config',
    0x0564: 'splash_screens',
    0x0592: 'config_menu             (GRAPHICS/DRIVE/CONTROL, reads 1/2/3)',
    0x07d2: 'wait_key_timeout        (timeout in [0x3c08])',
    0x07e9: 'wait_key                (loops until a key)',
    0x0860: 'read_key_filtered       (0xE0/0x16/V/space handling)',
    0x0896: 'key_read_wrapper        (INT 16h AH=1 peek, AH=0 get)',
    0x08a0: 'key_read_char           (same, unreferenced duplicate)',
    0x09e0: 'kbd_flush_buffer        (drains INT 16h until empty)',
    0x0a00: 'putchar_dispatch        (AL=char, picks blitter by video mode)',
    0x0a7d: 'glyph_blit_cga',
    0x0a9d: 'glyph_cell_write        (writes es:[di],+0x2000,+0x50 ...)',
    0x10fc: 'rng                     (rol/xor on [0x3c14],[0x3c16])',
    0x128d: 'install_timer_isr       (hooks INT 1Ch at 0000:0070)',
    0x12a0: 'timer_isr               (decrements [0x3c02]..[0x3c0a])',
    0x12e8: 'glyph_blit_alt',
    0x24bd: 'input_dispatch          (device -> [0x3bfa])',
    0x24a5: 'input_keyboard          (~[0x3c0e] & 0x1f)',
    0x26e0: 'kbd_isr_int09           (port 0x60 -> [0x3c0e] bitmap)',
    0x2740: 'input_poll_wrapper',
    0x2820: 'load_resource           (AX = index; descriptor table at 0x136b)',
    0x28be: 'disk_read_sectors       (INT 13h AH=2, CHS from [0x11ad..])',
    0x298a: 'disk_write_sectors      (INT 13h AH=3)',
    0x2e30: 'menu_run                (rows [0x93d3] -> pick in [0x93db])',
    0x3054: 'menu_parse_text         (counts \\r rows into [0x93d3])',
    0x3593: 'menu_select_loop        (fires when [0x3bfa] bit4 low)',
    0x3760: 'new_career',
    0x3939: 'character_creation',
    0x3ddf: 'script_interpreter      (bytecode VM, PC at [0x154d])',
    0x3fa5: 'script_apply_answer',
    0x3fbd: 'script_op_check         (protection compare -> [0x473d]=4)',
    0x40bd: 'name_entry              (family name into [0x104b])',
    0x4616: 'script_skip_on_penalty  (skips bytes when [0x473d] >= 4)',
}

# Corroborate labels with observed interrupt/port usage.
def evidence(f):
    e = []
    if ints.get(f):
        e.append('INT ' + ','.join(f'{x:02X}h' for x in sorted(ints[f])))
    if ports.get(f):
        e.append('PORT ' + ','.join(f'{x:#05x}' for x in sorted(ports[f])))
    return '  '.join(e)

out = []
out.append('Pirates! (DOS, 1987) -- function map, built outward from the boot sector')
out.append('=' * 78)
out.append('')
out.append('BOOT CHAIN (from the boot sector disassembly):')
out.append('  0000:7C00  boot sector: relocates itself to 0020:0000, sets CGA mode 4,')
out.append('             loads the title screen to B800:0000 and 168KB of program to')
out.append('             0050:0000, then LJMP 0050:0020.')
out.append('')
out.append('NOTE: "callers" below is the DIRECT call-site address (the E8 site),')
out.append('not an enclosing function -- call targets are not function boundaries,')
out.append('so attributing sites to the nearest preceding target is unreliable.')
out.append('')
out.append(f'{"addr":<8}{"size":>6}  {"label / evidence":<52}{"call sites"}')
out.append('-' * 78)

for idx, f in enumerate(starts):
    end = starts[idx+1] if idx + 1 < len(starts) else min(f + 256, len(img))
    size = end - f
    lab = LABEL.get(f, '')
    ev = evidence(f)
    if not lab and not ev and not calls.get(f):
        continue
    who = ','.join(f'{c:04x}' for c in sorted(calls.get(f, ()))[:5])
    desc = lab if lab else (ev if ev else '')
    if lab and ev:
        desc = f'{lab}  [{ev}]'
    out.append(f'{f:04x}  {size:>6}  {desc:<52}{who}')

txt = '\n'.join(out)
open('scratch/out/function_map.txt', 'w', encoding='utf-8').write(txt)
print(txt[:4000])
print(f'\n... full map in scratch/out/function_map.txt ({len(starts)} functions)')
