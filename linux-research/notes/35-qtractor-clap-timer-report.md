# Qtractor: CLAP plug-ins' timers stop when the last CLAP editor closes — draft upstream report

Found in T-013 (`tasks/T-013-host-matrix.md`, Log 2026-10-22, "Qtractor"). Evidence on `linux/T-013`:
`evidence/t013/qtractor/docker-clap/` (`clap-timer-counts.txt`, `timers-fresh-session.txt`,
`onidle-count-editor-closed.txt`) for our plug-ins, and `evidence/t013/qtractor/timer-probe/` for the minimal
reproducer below (run 2026-10-24, the source as below). Reviewed in `notes/37-review-T-013-wayland.md` (T-013 branch). Not reported yet: the
user decides whether to send it (as an issue at https://github.com/rncbc/qtractor/issues). The text below is ready to
paste.

---

**Title:** CLAP: registered plug-in timers stop firing after the last CLAP editor is closed

**Qtractor version:** 1.5.3 (Ubuntu 24.04 noble-backports, Qt 6.4.2), tested. The same code is in v1.6.4, unchanged
on `main` as of 2026-10-24: `src/qtractorClapPlugin.cpp`, `stopTimer()` l. 1120, `unregister_timer()` l. 1144–1157,
`openEditor()` → `g_host.startTimer(200)` l. 3155, `closeEditor()` → `g_host.stopTimer()` l. 3187 (in 1.5.3: l. 1122,
1146–1159, 3157, 3189).

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

1. Build the minimal plug-in below (`timer-probe.clap`: registers a 100 ms timer in `init()`, keeps it until
   `destroy()`, prints its `on_timer` calls per second to stderr, and has an empty X11 GUI):
   `cc -std=c11 -shared -fPIC -I<clap>/include -o timer-probe.clap timer-probe.c -lX11`.
   Start Qtractor from a terminal. The lines go to its Messages window, or to the terminal with the capture of
   standard output turned off in Qtractor's options.
2. Add `timer-probe` to an audio track. A line `timer-probe: N on_timer calls in the last second` appears every
   second.
3. Close the plug-in's editor window (the window manager's close button) if it is open; it must be the last CLAP
   editor open. Watch the lines for 10 s or more.
4. Open the editor again (double-click the plug-in in the track's plug-in list).

**Expected:** `on_timer` keeps being called while the timer is registered, with or without an editor.

**Actual** (Ubuntu 24.04 arm64, Xvfb + openbox, dummy JACK):
- before any editor was opened, and while the editor is open: 8–9 calls/s;
- after the editor closed (`timer-probe: GUI destroyed`): no call at all, for the 16 s until it was opened again;
- after the editor was opened again: 8 calls/s again (the first line then counts the calls of the second in which
  the timer had stopped).

The first output line, `register_timer(100 ms) FAILED`, comes from the instance Qtractor creates and destroys before
the real one, with a host that offers no timer support; the real instance's line says `ok`.

We first saw it with three plug-ins built with an in-development Linux port of the iPlug2 framework: about 150
`on_timer` calls/s in total before and with an editor open, 0 after the last editor closed, while `process` went on.

**Effect:** a plug-in that does main-thread work from its timer stops doing it whenever no CLAP editor is open. In
ours this is the update of its latency after a parameter change (it is computed on the main thread): it never runs
while the editor is closed, so even the `latency.get` that Qtractor calls when playback starts returns the old value.
Queues from the audio thread to the main thread also stop draining.

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
- VST3, from reading the source (untested: we had no 1.6.x build): in 1.5.3 only the plug frame offers `IRunLoop`, so
  the VST3 host does not have this problem. In v1.6.4 the host context offers `IRunLoop` too, driven by a shared
  `QTimer` that `qtractorVst3PluginHost::startTimer(200)`/`stopTimer()` start and stop around editors in the same
  way; the same one-line change (`&& m_timerHandlers.isEmpty()` in its `stopTimer()`) would fix it there.

We are happy to test a patch.

<details><summary>timer-probe.c</summary>

```c
// timer-probe.clap: registers a 100 ms timer in init() and prints its on_timer calls per second to stderr.
// Its GUI is an empty X11 window. Build: cc -std=c11 -shared -fPIC -I<clap>/include -o timer-probe.clap timer-probe.c -lX11
#include <clap/clap.h>
#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
  clap_plugin_t plugin;
  const clap_host_t* host;
  clap_id timer;
  int calls;
  time_t second;
  Display* display;
  Window window;
} probe_t;

static void on_timer(const clap_plugin_t* p, clap_id id) {
  probe_t* s = p->plugin_data;
  time_t now = time(NULL);
  if (now != s->second) {
    if (s->second) fprintf(stderr, "timer-probe: %d on_timer calls in the last second\n", s->calls);
    s->second = now;
    s->calls = 0;
  }
  s->calls++;
}
static const clap_plugin_timer_support_t timer_ext = { on_timer };

static bool gui_is_api_supported(const clap_plugin_t* p, const char* api, bool floating) { return !floating && !strcmp(api, CLAP_WINDOW_API_X11); }
static bool gui_get_preferred_api(const clap_plugin_t* p, const char** api, bool* floating) { *api = CLAP_WINDOW_API_X11; *floating = false; return true; }
static bool gui_create(const clap_plugin_t* p, const char* api, bool floating) { return true; }
static void gui_destroy(const clap_plugin_t* p) {
  probe_t* s = p->plugin_data;
  if (s->display) { if (s->window) XDestroyWindow(s->display, s->window); XCloseDisplay(s->display); }
  s->display = NULL; s->window = 0;
  fprintf(stderr, "timer-probe: GUI destroyed\n");
}
static bool gui_set_scale(const clap_plugin_t* p, double scale) { return false; }
static bool gui_get_size(const clap_plugin_t* p, uint32_t* w, uint32_t* h) { *w = 300; *h = 100; return true; }
static bool gui_can_resize(const clap_plugin_t* p) { return false; }
static bool gui_get_resize_hints(const clap_plugin_t* p, clap_gui_resize_hints_t* h) { return false; }
static bool gui_adjust_size(const clap_plugin_t* p, uint32_t* w, uint32_t* h) { return false; }
static bool gui_set_size(const clap_plugin_t* p, uint32_t w, uint32_t h) { return false; }
static bool gui_set_parent(const clap_plugin_t* p, const clap_window_t* w) {
  probe_t* s = p->plugin_data;
  s->display = XOpenDisplay(NULL);
  if (!s->display) return false;
  s->window = XCreateSimpleWindow(s->display, w->x11, 0, 0, 300, 100, 0, 0, 0x808080);
  XFlush(s->display);
  fprintf(stderr, "timer-probe: GUI created\n");
  return true;
}
static bool gui_set_transient(const clap_plugin_t* p, const clap_window_t* w) { return false; }
static void gui_suggest_title(const clap_plugin_t* p, const char* t) {}
static bool gui_show(const clap_plugin_t* p) { probe_t* s = p->plugin_data; if (!s->display) return false; XMapWindow(s->display, s->window); XFlush(s->display); return true; }
static bool gui_hide(const clap_plugin_t* p) { probe_t* s = p->plugin_data; if (!s->display) return false; XUnmapWindow(s->display, s->window); XFlush(s->display); return true; }
static const clap_plugin_gui_t gui_ext = {
  gui_is_api_supported, gui_get_preferred_api, gui_create, gui_destroy, gui_set_scale, gui_get_size, gui_can_resize,
  gui_get_resize_hints, gui_adjust_size, gui_set_size, gui_set_parent, gui_set_transient, gui_suggest_title,
  gui_show, gui_hide };

static bool init(const clap_plugin_t* p) {
  probe_t* s = p->plugin_data;
  const clap_host_timer_support_t* t = s->host->get_extension(s->host, CLAP_EXT_TIMER_SUPPORT);
  bool ok = t && t->register_timer(s->host, 100, &s->timer);
  if (!ok) s->timer = CLAP_INVALID_ID;
  fprintf(stderr, "timer-probe: register_timer(100 ms) %s\n", ok ? "ok" : "FAILED");
  return true;
}
static void destroy(const clap_plugin_t* p) {
  probe_t* s = p->plugin_data;
  const clap_host_timer_support_t* t = s->host->get_extension(s->host, CLAP_EXT_TIMER_SUPPORT);
  if (t && s->timer != CLAP_INVALID_ID) t->unregister_timer(s->host, s->timer);
  free(s);
}
static bool activate(const clap_plugin_t* p, double sr, uint32_t min, uint32_t max) { return true; }
static void deactivate(const clap_plugin_t* p) {}
static bool start_processing(const clap_plugin_t* p) { return true; }
static void stop_processing(const clap_plugin_t* p) {}
static void reset(const clap_plugin_t* p) {}
static clap_process_status process(const clap_plugin_t* p, const clap_process_t* pr) { return CLAP_PROCESS_CONTINUE; }
static const void* get_extension(const clap_plugin_t* p, const char* id) {
  if (!strcmp(id, CLAP_EXT_TIMER_SUPPORT)) return &timer_ext;
  if (!strcmp(id, CLAP_EXT_GUI)) return &gui_ext;
  return NULL;
}
static void on_main_thread(const clap_plugin_t* p) {}

static const char* features[] = { CLAP_PLUGIN_FEATURE_AUDIO_EFFECT, NULL };
static const clap_plugin_descriptor_t desc = {
  CLAP_VERSION_INIT, "org.example.timer-probe", "timer-probe", "example", "", "", "", "1.0", "CLAP timer probe", features };

static uint32_t count(const clap_plugin_factory_t* f) { return 1; }
static const clap_plugin_descriptor_t* get_desc(const clap_plugin_factory_t* f, uint32_t i) { return &desc; }
static const clap_plugin_t* create(const clap_plugin_factory_t* f, const clap_host_t* host, const char* id) {
  probe_t* s = calloc(1, sizeof(probe_t));
  s->host = host;
  s->timer = CLAP_INVALID_ID;
  s->plugin = (clap_plugin_t) { &desc, s, init, destroy, activate, deactivate, start_processing, stop_processing,
                                reset, process, get_extension, on_main_thread };
  return &s->plugin;
}
static const clap_plugin_factory_t factory = { count, get_desc, create };
static bool entry_init(const char* path) { return true; }
static void entry_deinit(void) {}
static const void* entry_get_factory(const char* id) { return strcmp(id, CLAP_PLUGIN_FACTORY_ID) ? NULL : &factory; }
CLAP_EXPORT const clap_plugin_entry_t clap_entry = { CLAP_VERSION_INIT, entry_init, entry_deinit, entry_get_factory };
```
</details>
