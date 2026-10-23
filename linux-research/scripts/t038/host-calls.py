# gdb: log every VST3 host notification a plug-in sends (the host's own IComponentHandler methods, found through
# the plug-in's componentHandler vtable at the first SetLatency) and the plug-in side around them, with the caller's
# LWP and a wall-clock time. Needs a Debug build of an iplug::IPlugVST3 plug-in. Used by reaper-deferred-test.sh.
import gdb, time

SLOTS = {3: "beginEdit", 4: "performEdit", 5: "endEdit", 6: "restartComponent"}  # IComponentHandler after FUnknown's 3
installed = set()

def log(what):
    print("CALL %.3f lwp=%d %s" % (time.time(), gdb.selected_thread().ptid[1], what), flush=True)

class HostMethod(gdb.Breakpoint):
    def __init__(self, addr, name):
        super().__init__("*0x%x" % addr, internal=True)
        self.name = name
    def stop(self):
        extra = ""
        if self.name == "restartComponent":
            extra = " flags=%d" % int(gdb.parse_and_eval("$x1"))
        log("HOST " + self.name + extra)
        return False

class PlugMethod(gdb.Breakpoint):
    def __init__(self, spec, name, install=False):
        super().__init__(spec, internal=True)
        self.name = name
        self.install = install
    def stop(self):
        log("PLUG " + self.name)
        if self.install:
            handler = int(gdb.parse_and_eval("this->componentHandler.ptr"))
            if handler and handler not in installed:
                installed.add(handler)
                vtable = int(gdb.parse_and_eval("*(unsigned long*)%d" % handler))
                for slot, name in SLOTS.items():
                    HostMethod(int(gdb.parse_and_eval("*(unsigned long*)%d" % (vtable + 8 * slot))), name)
                log("installed host breakpoints for handler 0x%x" % handler)
        return False

PlugMethod("iplug::IPlugVST3::SetLatency", "SetLatency", install=True)
PlugMethod("iplug::VST3HostThread::SendDeferred", "SendDeferred")
PlugMethod("iplug::IPlugVST3::getLatencySamples", "getLatencySamples")
