import ida_dbg, ida_kernwin, ida_idd, idaapi

RVA_SETUPBONES = 0x0EAAA0

def find_client_base():
    for i in range(ida_dbg.get_processes_count()):
        pass
    mi = ida_dbg.get_module_info('client.dll')
    return mi.base if mi else idaapi.BADADDR

class Probe(ida_dbg.DBG_Hooks):
    def dbg_bpt(self, tid, bptea):
        if bptea == self.addr:
            regs = {}
            for name in ('RIP','RSP','RCX','RDX','R8','R9'):
                try: regs[name] = ida_dbg.get_reg_val(name)
                except Exception: regs[name] = 0
            ida_kernwin.msg('[SetupBones] ' + ' '.join('%s=%X' % (k,v) for k,v in regs.items()) + '\n')
            with open(r'C:\Users\Idiosyncratic\Downloads\No-More-Room-In-Hell-1-cheat-main\No-More-Room-In-Hell-1-cheat-main\setupbones_runtime.txt','a') as f:
                f.write(' '.join('%s=0x%X' % (k,v) for k,v in regs.items())+'\n')
        return 0

base = find_client_base()
if base == idaapi.BADADDR:
    ida_kernwin.warning('Attach to nmrih_win64.exe first; client.dll is not loaded.')
else:
    addr = base + RVA_SETUPBONES
    ida_dbg.add_bpt(addr)
    h = Probe(); h.addr = addr; h.hook()
    ida_kernwin.msg('[SetupBones] client.dll base=0x%X breakpoint=0x%X\n' % (base, addr))
