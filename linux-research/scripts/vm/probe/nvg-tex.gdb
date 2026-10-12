set pagination off
set breakpoint pending on
python
import gdb, time
T0 = time.time()
class B(gdb.Breakpoint):
    def __init__(self, spec, fmt):
        super().__init__(spec); self.fmt = fmt
    def stop(self):
        f = gdb.selected_frame()
        r = lambda n: int(f.read_register(n)) & 0xffffffffffffffff
        print("%7.2f %s" % (time.time() - T0, self.fmt(r)), flush=True)
        return False
B("glnvg__renderCreateTexture", lambda r: "create-tex type=%d %dx%d flags=0x%x" % (r("x1"), r("x2"), r("x3"), r("x4")))
B("glnvg__renderDeleteTexture", lambda r: "delete-tex id=%d" % r("x1"))
B("glnvg__renderCreate", lambda r: "nvg-create")
B("glnvg__renderDelete", lambda r: "nvg-delete")
print("%7.2f attached" % (time.time() - T0), flush=True)
end
continue
