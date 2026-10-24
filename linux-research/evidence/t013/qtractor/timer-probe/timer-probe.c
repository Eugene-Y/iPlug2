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
