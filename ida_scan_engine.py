import ida_auto, idautils, ida_funcs, ida_pro
out=open(r'C:\Users\Idiosyncratic\Downloads\No-More-Room-In-Hell-1-cheat-main\No-More-Room-In-Hell-1-cheat-main\ida_engine_results.txt','w')
ida_auto.auto_wait()
for s in idautils.Strings():
 t=str(s)
 if any(x in t.lower() for x in ('traceray','engine trace','enginetrace','trace ray')):
  out.write('STR %x %s\n'%(s.ea,t[:200]))
  for xr in idautils.XrefsTo(s.ea): out.write('XREF %x %x %s\n'%(s.ea,xr.frm,ida_funcs.get_func_name(xr.frm)))
out.close(); ida_pro.qexit(0)
