# Qtractor: CLAP plug-ins' timers stop when the last CLAP editor closes — draft upstream report

Found in T-013 (`tasks/T-013-host-matrix.md`, Log 2026-10-22, "Qtractor"). Evidence:
`evidence/t013/qtractor/docker-clap/` (`clap-timer-counts.txt`, `timers-fresh-session.txt`,
`onidle-count-editor-closed.txt`). Not reported yet: the user decides whether to send it (as an issue at
https://github.com/rncbc/qtractor/issues). The text below is ready to paste.

---

**Title:** CLAP: registered plug-in timers stop firing after the last CLAP editor is closed

**Qtractor version:** 1.5.3 (Ubuntu noble-backports, tested). The same code is in v1.6.4 and on `main`
(314600e), `src/qtractorClapPlugin.cpp`.

**Summary**

All CLAP plug-ins' timers (`clap.timer-support`) are driven by one shared `QTimer` in `qtractorClapPluginHost`.
Two independent mechanisms start and stop it:

- `register_timer()` starts it, and `unregister_timer()` stops it once `m_timers` is empty;
- `qtractorClapPlugin::openEditor()` calls `g_host.startTimer(200)` (reference counted), and `closeEditor()` calls
  `g_host.stopTimer()`, which stops the shared `QTimer` when the reference count drops to 0.

`stopTimer()` doesn't check whether plug-ins still have registered timers:

```cpp
void qtractorClapPluginHost::stopTimer (void)
	{ if (m_timerRefCount > 0 && --m_timerRefCount == 0) m_pTimer->stop(); }
```

So when the last CLAP editor closes, the shared `QTimer` stops while plug-ins still have timers registered, and no
plug-in's `on_timer` is called again until an editor opens or some plug-in registers a new timer.

**Steps to reproduce**

1. Load a CLAP plug-in that registers a timer in `init()` or `activate()` and keeps it while its GUI is closed. Many
   plug-ins do this for idle work: UI-independent housekeeping, deferred host notifications such as latency or
   parameter changes made on the main thread. Any plug-in built with iPlug2 does.
2. Count `clap_plugin_timer_support::on_timer` calls, e.g. with gdb breakpoints or a log line in the plug-in.
3. Open the plug-in's editor, then close it. It must be the last CLAP editor open.

**Expected:** `on_timer` keeps being called at the registered period while the timer is registered, with or without
an editor.

**Actual** (three plug-ins, roughly 8 s per measurement):
- before any editor was opened, and while one is open: about 150 `on_timer` calls/s in total;
- after the last editor closed: 0 calls for every plug-in. Audio processing (`process`) continues normally.

**Effect:** plug-ins that do main-thread work from their timer stop doing it whenever no CLAP editor is open. In our
case this is reporting a latency change, which is done on the main thread and so is never reported, and draining
processor-to-UI queues.

**Suggested fix:** let the editor reference count stop the shared timer only when no plug-in timers are registered:

```cpp
void qtractorClapPluginHost::stopTimer (void)
{
	if (m_timerRefCount > 0 && --m_timerRefCount == 0 && m_timers.isEmpty())
		m_pTimer->stop();
}
```

`unregister_timer()` already stops it when the last registered timer goes.

**Also seen, for context (not part of this bug):**
- Qtractor ignores `clap_host_latency::changed()` and skips `request_restart()` while its engine runs. It reads
  `latency.get` on the main thread when playback starts.
- From reading v1.6.4's source (untested: we had no 1.6.x build), the VST3 host's run loop seems to follow the same
  pattern: a shared `QTimer` driving the host-context `IRunLoop` timers, started and stopped around editors.
