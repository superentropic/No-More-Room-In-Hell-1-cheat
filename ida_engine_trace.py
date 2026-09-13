import ida_auto
import ida_bytes
import ida_funcs
import ida_idaapi
import ida_name
import ida_segment
import ida_hexrays
import ida_pro
import idautils

OUT = r"C:\Users\Idiosyncratic\Downloads\No-More-Room-In-Hell-1-cheat-main\No-More-Room-In-Hell-1-cheat-main\engine_trace_ida.txt"

ida_auto.auto_wait()

def is_code_ptr(ea):
    return ida_funcs.get_func(ea) is not None

def qword(ea):
    return ida_bytes.get_qword(ea)

with open(OUT, "w", encoding="utf-8") as out:
    trace_string_ea = ida_idaapi.BADADDR
    for s in idautils.Strings():
        if str(s) == "CEngineTrace::TraceRay":
            trace_string_ea = s.ea
            out.write("TRACE_STRING %016X\n" % s.ea)
            break

    if trace_string_ea == ida_idaapi.BADADDR:
        out.write("ERROR trace implementation string missing\n")
    else:
        impls = set()
        for x in idautils.XrefsTo(trace_string_ea):
            f = ida_funcs.get_func(x.frm)
            if f:
                impls.add(f.start_ea)
        for impl in sorted(impls):
            out.write("IMPL %016X %s\n" % (impl, ida_name.get_name(impl)))
            try:
                out.write("PSEUDOCODE\n%s\nEND_PSEUDOCODE\n" % str(ida_hexrays.decompile(impl)))
            except Exception as ex:
                out.write("PSEUDOCODE_ERROR %r\n" % ex)
            for x in idautils.XrefsTo(impl):
                if x.iscode:
                    out.write("  CODE_XREF %016X\n" % x.frm)
                    continue
                entry = x.frm
                out.write("  DATA_XREF entry=%016X\n" % entry)
                # A MSVC vftable is a contiguous run of function pointers. Walk
                # back to its first function-pointer entry, stopping before the
                # RTTI complete-object-locator pointer.
                first = entry
                while first >= 8 and is_code_ptr(qword(first - 8)):
                    first -= 8
                slot = (entry - first) // 8
                out.write("  VFTABLE first=%016X slot=%d COL=%016X\n" % (first, slot, first - 8))
                for i in range(max(0, slot - 3), slot + 4):
                    fn = qword(first + i * 8)
                    out.write("    [%d] %016X %s\n" % (i, fn, ida_name.get_name(fn)))

    for s in idautils.Strings():
        if str(s) in ("EngineTraceClient003", "EngineTraceServer003"):
            out.write("INTERFACE_STRING %016X %s\n" % (s.ea, str(s)))

ida_pro.qexit(0)
