set pagination off
python
import gdb, time
class B(gdb.Breakpoint):
    def stop(self):
        print("DRAW %.4f" % time.time(), flush=True)
        return False
B("iplug::igraphics::IGraphics::Draw(iplug::igraphics::IRECTList&)")
print("ATTACHED", flush=True)
end
continue
