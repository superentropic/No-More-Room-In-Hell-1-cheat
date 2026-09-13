import ida_auto, idautils, ida_funcs, ida_pro, ida_xref
out=open(r'C:\Users\Idiosyncratic\Downloads\No-More-Room-In-Hell-1-cheat-main\No-More-Room-In-Hell-1-cheat-main\ida_scan_results.txt','w')
ida_auto.auto_wait()
for s in idautils.Strings():
    t=str(s)
    if any(x.lower() in t.lower() for x in ('setupbones','traceray','engine_trace','bone')):
        out.write('STR %x %s\n' % (s.ea,t[:200]))
        for xr in idautils.XrefsTo(s.ea):
            fn=ida_funcs.get_func_name(xr.frm)
            out.write('XREF %x from %x %s\n' % (s.ea,xr.frm,fn))
for ea in idautils.Functions():
    n=ida_funcs.get_func_name(ea)
    if any(x.lower() in n.lower() for x in ('setupbones','traceray','trace_ray','bones')):
        out.write('FUNC %x %s\n' % (ea,n))
out.close()
ida_pro.qexit(0)
