import ida_auto
import ida_bytes
import ida_funcs
import ida_hexrays
import ida_idaapi
import ida_name
import ida_segment
import ida_pro
import idautils

OUT = r"C:\Users\Idiosyncratic\Downloads\No-More-Room-In-Hell-1-cheat-main\No-More-Room-In-Hell-1-cheat-main\setupbones_refs_ida.txt"
TARGET = 0x1800EAAA0

ida_auto.auto_wait()
with open(OUT, "w", encoding="utf-8") as out:
    refs = list(idautils.CodeRefsTo(TARGET, False))
    out.write("TARGET %016X direct_code_refs=%d\n" % (TARGET, len(refs)))
    for frm in refs:
        f = ida_funcs.get_func(frm)
        out.write("REF %016X FUNC %016X %s\n" % (frm, f.start_ea if f else 0, ida_name.get_name(f.start_ea) if f else ""))
        if f:
            try:
                out.write(str(ida_hexrays.decompile(f.start_ea))[:10000])
            except Exception as ex:
                out.write("DECOMP_ERR %r" % ex)
            out.write("\n---\n")
    for seg_ea in idautils.Segments():
        seg = ida_segment.getseg(seg_ea)
        if not seg or ida_segment.get_segm_name(seg) not in (".rdata", ".data"):
            continue
        ea = seg.start_ea
        while ea + 8 <= seg.end_ea:
            if ida_bytes.get_qword(ea) == TARGET:
                out.write("VTABLE_CANDIDATE entry=%016X name=%s\n" % (ea, ida_name.get_name(ea)))
            ea += 8

ida_pro.qexit(0)
