import ida_auto, ida_hexrays, ida_funcs, ida_xref, ida_pro
ida_auto.auto_wait(); out=open(r'C:\Users\Idiosyncratic\Downloads\No-More-Room-In-Hell-1-cheat-main\No-More-Room-In-Hell-1-cheat-main\setupbones_callers.txt','w')
target=0x1800EAAA0
for xr in ida_xref.XrefsTo(target,0):
 f=ida_funcs.get_func(xr.frm)
 if not f: continue
 out.write('CALLER %x %s\n'%(f.start_ea,ida_funcs.get_func_name(f.start_ea)))
 try:
  c=ida_hexrays.decompile(f.start_ea); out.write(str(c)[:12000]+'\n---\n')
 except Exception as e: out.write('ERR %r\n'%e)
out.close(); ida_pro.qexit(0)
