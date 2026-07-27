import sys, struct
sys.stdout.reconfigure(encoding='utf-8')
from unicorn import *
from unicorn.x86_const import *

DISK1 = open('assets/pirates_1.img','rb').read()
mu = Uc(UC_ARCH_X86, UC_MODE_16)
mu.mem_map(0, 0x110000)
mu.mem_write(0x7C00, DISK1[:512])

def hook_code(uc, addr, size, user):
    print(f'  exec {addr:06X} size={size}')
    if addr > 0x7C40: uc.emu_stop()
mu.hook_add(UC_HOOK_CODE, hook_code)

mu.reg_write(UC_X86_REG_CS, 0)
mu.reg_write(UC_X86_REG_IP, 0x7C00)
mu.reg_write(UC_X86_REG_SS, 0)
mu.reg_write(UC_X86_REG_SP, 0x7000)
print('start at linear 0x7C00, CS=0 IP=7C00')
try:
    mu.emu_start(0x7C00, 0x7C40, 0, 40)
except UcError as e:
    print('UcError:', e, f'at {mu.reg_read(UC_X86_REG_CS):04X}:{mu.reg_read(UC_X86_REG_IP):04X}')
print('done')
