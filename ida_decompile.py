import ida_auto, ida_hexrays, ida_funcs, ida_pro
ida_auto.auto_wait()
out=open(r'C:\Users\Idiosyncratic\Downloads\No-More-Room-In-Hell-1-cheat-main\No-More-Room-In-Hell-1-cheat-main\setupbones_pseudocode.txt','w')
ea=0x1800EAAA0
try:
 c=ida_hexrays.decompile(ea)
 out.write(str(c) if c else 'DECOMPILATION_FAILED')
except Exception as e: out.write('ERROR '+repr(e))
out.close(); ida_pro.qexit(0)
