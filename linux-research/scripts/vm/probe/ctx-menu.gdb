set pagination off
python
import gdb, time
class Ret(gdb.FinishBreakpoint):
    def __init__(self, t0):
        super().__init__(gdb.newest_frame(), internal=True); self.t0 = t0
    def stop(self):
        print("CTX return after %.3f s" % (time.time() - self.t0), flush=True); return False
    def out_of_scope(self):
        print("CTX out of scope", flush=True)
class Enter(gdb.Breakpoint):
    def stop(self):
        t = time.time(); print("CTX enter %.3f" % t, flush=True); Ret(t); return False
class Draw(gdb.Breakpoint):
    def stop(self):
        print("DRAW %.3f" % time.time(), flush=True); return False
Enter("iplug::igraphics::IGraphics::PopupHostContextMenuForParam(iplug::igraphics::IControl*, int, float, float)")
Draw("iplug::igraphics::IGraphics::Draw(iplug::igraphics::IRECTList&)")
print("ATTACHED", flush=True)
end
continue
