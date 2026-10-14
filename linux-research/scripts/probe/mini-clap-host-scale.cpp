// Minimal CLAP host for the P3a review: a level-triggered poll() reactor with timer-support and posix-fd-support,
// an X11 parent window of its own, and scenarios REAPER cannot produce on demand.
// T-007 copy (evidence/p3a-review/mini-clap-host.cpp + scale scenarios): request_resize is logged, and
//   MINI_SET_SCALE=<s>  calls gui.set_scale(s) right after gui.create, as the CLAP spec orders it (default: never)
//   scale      open, run, report the sizes (no set_scale unless MINI_SET_SCALE: the desktop's scale applies)
//   rescale    open, run 1.5 s, gui.set_scale(<secs>) on the open editor, run 1.5 s, report the sizes
//   reopen     open, close, open again, report the sizes each time
// Build (in the container): g++ -std=c++17 -O1 -g -I<CLAP_SDK>/include mini-clap-host-scale.cpp -o mini-clap-host-scale -ldl -lX11
// Usage: mini-clap-host-scale <plugin.clap> <scenario> [secs]
//   fd         open, run; report on_fd / on_timer counts (drive the mouse from outside)
//   fd-only    same, but never fire the plug-in's timers: input must still be drained through on_fd
//   parent-death  open, destroy the parent X window, keep running, then destroy the editor and the plug-in
//   two        two instances, one editor each; report the process' sockets
//   churn      open/close the editor <secs> times; report fds/threads before and after
//   nogl       open with whatever libGL there is (run without one): the host must survive
#include <clap/clap.h>
#include <X11/Xlib.h>
#include <dlfcn.h>
#include <poll.h>
#include <unistd.h>
#include <dirent.h>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

using Clock = std::chrono::steady_clock;

struct Host;
struct Timer { uint32_t ms; Clock::time_point next; };

struct Instance
{
  Host* host = nullptr;
  clap_host_t clapHost {};
  const clap_plugin_t* plugin = nullptr;
  const clap_plugin_gui_t* gui = nullptr;
  const clap_plugin_timer_support_t* timerExt = nullptr;
  const clap_plugin_posix_fd_support_t* fdExt = nullptr;
  std::map<clap_id, Timer> timers;
  std::map<int, clap_posix_fd_flags_t> fds;
  clap_id nextTimer = 1;
  Window parent = 0;
  long onFd = 0, onTimer = 0, callbacks = 0;
  bool callbackRequested = false;
};

struct Host
{
  Display* display = nullptr;
  const clap_plugin_entry_t* entry = nullptr;
  const clap_plugin_factory_t* factory = nullptr;
  bool fireTimers = true;
};

static Instance* Inst(const clap_host_t* h) { return static_cast<Instance*>(h->host_data); }

static bool RegisterTimer(const clap_host_t* h, uint32_t ms, clap_id* id)
{
  Instance* i = Inst(h);
  *id = i->nextTimer++;
  i->timers[*id] = { ms, Clock::now() + std::chrono::milliseconds(ms) };
  return true;
}
static bool UnregisterTimer(const clap_host_t* h, clap_id id) { return Inst(h)->timers.erase(id) > 0; }
static const clap_host_timer_support_t sTimerSupport { RegisterTimer, UnregisterTimer };

static bool RegisterFd(const clap_host_t* h, int fd, clap_posix_fd_flags_t flags)
{
  printf("register_fd fd=%d flags=%u\n", fd, flags);
  Inst(h)->fds[fd] = flags;
  return true;
}
static bool ModifyFd(const clap_host_t* h, int fd, clap_posix_fd_flags_t flags) { Inst(h)->fds[fd] = flags; return true; }
static bool UnregisterFd(const clap_host_t* h, int fd) { printf("unregister_fd fd=%d\n", fd); return Inst(h)->fds.erase(fd) > 0; }
static const clap_host_posix_fd_support_t sFdSupport { RegisterFd, ModifyFd, UnregisterFd };

static void ResizeHintsChanged(const clap_host_t*) {}
static bool RequestResize(const clap_host_t* h, uint32_t w, uint32_t hh)
{
  Instance* i = Inst(h);
  printf("request_resize %ux%u\n", w, hh);
  XResizeWindow(i->host->display, i->parent, w, hh);
  XFlush(i->host->display);
  return true;
}
static bool RequestShow(const clap_host_t*) { return false; }
static bool RequestHide(const clap_host_t*) { return false; }
static void Closed(const clap_host_t*, bool) {}
static const clap_host_gui_t sHostGui { ResizeHintsChanged, RequestResize, RequestShow, RequestHide, Closed };

static bool IsMainThread(const clap_host_t*) { return true; }
static bool IsAudioThread(const clap_host_t*) { return false; }
static const clap_host_thread_check_t sThreadCheck { IsMainThread, IsAudioThread };

static void ParamsRescan(const clap_host_t*, clap_param_rescan_flags) {}
static void ParamsClear(const clap_host_t*, clap_id, clap_param_clear_flags) {}
static void ParamsRequestFlush(const clap_host_t*) {}
static const clap_host_params_t sParams { ParamsRescan, ParamsClear, ParamsRequestFlush };

static const void* GetExtension(const clap_host_t*, const char* id)
{
  if (!strcmp(id, CLAP_EXT_PARAMS)) return &sParams;
  if (!strcmp(id, CLAP_EXT_TIMER_SUPPORT)) return &sTimerSupport;
  if (!strcmp(id, CLAP_EXT_POSIX_FD_SUPPORT)) return &sFdSupport;
  if (!strcmp(id, CLAP_EXT_GUI)) return &sHostGui;
  if (!strcmp(id, CLAP_EXT_THREAD_CHECK)) return &sThreadCheck;
  return nullptr;
}
static void RequestRestart(const clap_host_t*) {}
static void RequestProcess(const clap_host_t*) {}
static void RequestCallback(const clap_host_t* h) { Inst(h)->callbackRequested = true; }

static int Count(const char* dir, const char* match)
{
  int n = 0;
  if (DIR* d = opendir(dir))
  {
    while (dirent* e = readdir(d))
    {
      if (e->d_name[0] == '.') continue;
      if (!match) { n++; continue; }
      char path[256], target[256];
      snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
      ssize_t len = readlink(path, target, sizeof(target) - 1);
      if (len > 0) { target[len] = 0; if (strstr(target, match)) n++; }
    }
    closedir(d);
  }
  return n;
}
static void Census(const char* label)
{
  printf("%s: fds=%d sockets=%d threads=%d\n", label, Count("/proc/self/fd", nullptr), Count("/proc/self/fd", "socket:"),
         Count("/proc/self/task", nullptr));
}

static Instance* Create(Host& host)
{
  auto* i = new Instance;
  i->host = &host;
  i->clapHost = { CLAP_VERSION, i, "mini-clap-host", "review", "", "1", GetExtension, RequestRestart, RequestProcess, RequestCallback };
  const clap_plugin_descriptor_t* desc = host.factory->get_plugin_descriptor(host.factory, 0);
  i->plugin = host.factory->create_plugin(host.factory, &i->clapHost, desc->id);
  if (!i->plugin || !i->plugin->init(i->plugin)) { printf("create/init failed\n"); return nullptr; }
  i->gui = static_cast<const clap_plugin_gui_t*>(i->plugin->get_extension(i->plugin, CLAP_EXT_GUI));
  i->timerExt = static_cast<const clap_plugin_timer_support_t*>(i->plugin->get_extension(i->plugin, CLAP_EXT_TIMER_SUPPORT));
  i->fdExt = static_cast<const clap_plugin_posix_fd_support_t*>(i->plugin->get_extension(i->plugin, CLAP_EXT_POSIX_FD_SUPPORT));
  return i;
}

static bool OpenEditor(Instance* i, int x)
{
  Display* d = i->host->display;
  uint32_t w = 0, h = 0;
  if (!i->gui->create(i->plugin, CLAP_WINDOW_API_X11, false)) { printf("gui.create failed\n"); return false; }
  if (const char* scale = getenv("MINI_SET_SCALE"))
    printf("set_scale(%s) -> %d\n", scale, i->gui->set_scale(i->plugin, atof(scale)));
  i->gui->get_size(i->plugin, &w, &h);
  i->parent = XCreateSimpleWindow(d, DefaultRootWindow(d), x, 0, w, h, 0, 0, 0x404040);
  XStoreName(d, i->parent, "mini-clap-host");
  XMapWindow(d, i->parent);
  XSync(d, False);
  clap_window_t win {};
  win.api = CLAP_WINDOW_API_X11;
  win.x11 = i->parent;
  const bool parented = i->gui->set_parent(i->plugin, &win);
  const bool shown = i->gui->show(i->plugin);
  printf("editor %ux%u parent=0x%lx set_parent=%d show=%d\n", w, h, i->parent, parented, shown);
  return true;
}

// get_size, the parent's size and the editor's (the parent's only child)
static void ReportSizes(Instance* i, const char* label)
{
  Display* d = i->host->display;
  uint32_t w = 0, h = 0;
  i->gui->get_size(i->plugin, &w, &h);
  XWindowAttributes parent {}, child {};
  XGetWindowAttributes(d, i->parent, &parent);
  Window root = 0, up = 0, *pChildren = nullptr;
  unsigned n = 0;
  XQueryTree(d, i->parent, &root, &up, &pChildren, &n);
  if (n > 0) XGetWindowAttributes(d, pChildren[0], &child);
  if (pChildren) XFree(pChildren);
  printf("%s: get_size %ux%u, parent %dx%d, editor %dx%d\n", label, w, h, parent.width, parent.height, child.width, child.height);
}

static void CloseEditor(Instance* i, bool destroyParent = true)
{
  i->gui->destroy(i->plugin);
  if (destroyParent && i->parent)
    XDestroyWindow(i->host->display, i->parent);
  i->parent = 0;
  XSync(i->host->display, False);
}

static void Destroy(Instance* i)
{
  i->plugin->destroy(i->plugin);
  delete i;
}

// One reactor pass over every instance: poll the registered fds (level-triggered) until the next timer is due.
static void RunFor(Host& host, std::vector<Instance*> insts, double secs)
{
  const auto end = Clock::now() + std::chrono::milliseconds(static_cast<int>(secs * 1000));
  while (Clock::now() < end)
  {
    std::vector<pollfd> pfds;
    std::vector<Instance*> owners;
    auto due = end;
    for (Instance* i : insts)
    {
      for (auto& [fd, flags] : i->fds)
      {
        pfds.push_back({ fd, static_cast<short>(((flags & CLAP_POSIX_FD_READ) ? POLLIN : 0) | ((flags & CLAP_POSIX_FD_WRITE) ? POLLOUT : 0)), 0 });
        owners.push_back(i);
      }
      if (host.fireTimers)
        for (auto& [id, t] : i->timers)
          due = std::min(due, t.next);
    }
    // the host's own display, so ConfigureNotify etc. do not pile up
    while (XPending(host.display)) { XEvent e; XNextEvent(host.display, &e); }
    const int timeout = std::max<int>(0, std::chrono::duration_cast<std::chrono::milliseconds>(due - Clock::now()).count());
    const int n = poll(pfds.data(), pfds.size(), timeout);
    if (n > 0)
    {
      for (size_t k = 0; k < pfds.size(); k++)
      {
        if (!pfds[k].revents) continue;
        Instance* i = owners[k];
        if (!i->fds.count(pfds[k].fd)) continue;
        clap_posix_fd_flags_t flags = 0;
        if (pfds[k].revents & POLLIN) flags |= CLAP_POSIX_FD_READ;
        if (pfds[k].revents & POLLOUT) flags |= CLAP_POSIX_FD_WRITE;
        if (pfds[k].revents & (POLLERR | POLLHUP | POLLNVAL)) flags |= CLAP_POSIX_FD_ERROR;
        i->onFd++;
        i->fdExt->on_fd(i->plugin, pfds[k].fd, flags);
      }
    }
    const auto now = Clock::now();
    for (Instance* i : insts)
    {
      if (host.fireTimers)
      {
        std::vector<clap_id> ready;
        for (auto& [id, t] : i->timers)
          if (t.next <= now) ready.push_back(id);
        for (clap_id id : ready)
        {
          auto it = i->timers.find(id);
          if (it == i->timers.end()) continue;
          it->second.next = now + std::chrono::milliseconds(it->second.ms);
          i->onTimer++;
          i->timerExt->on_timer(i->plugin, id);
        }
      }
      if (i->callbackRequested) { i->callbackRequested = false; i->callbacks++; i->plugin->on_main_thread(i->plugin); }
    }
  }
}

int main(int argc, char** argv)
{
  if (argc < 3) { fprintf(stderr, "usage: %s <plugin.clap> <scenario> [secs]\n", argv[0]); return 2; }
  const std::string scenario = argv[2];
  const double secs = argc > 3 ? atof(argv[3]) : 4.0;
  setvbuf(stdout, nullptr, _IOLBF, 0);

  Host host;
  host.display = XOpenDisplay(nullptr);
  if (!host.display) { printf("no display\n"); return 1; }
  void* lib = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
  if (!lib) { printf("dlopen: %s\n", dlerror()); return 1; }
  host.entry = static_cast<const clap_plugin_entry_t*>(dlsym(lib, "clap_entry"));
  host.entry->init(argv[1]);
  host.factory = static_cast<const clap_plugin_factory_t*>(host.entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
  host.fireTimers = scenario != "fd-only";

  Census("start");
  Instance* a = Create(host);
  if (!a) return 1;

  if (scenario == "fd" || scenario == "fd-only" || scenario == "nogl")
  {
    OpenEditor(a, 0);
    RunFor(host, { a }, secs);
    printf("on_fd=%ld on_timer=%ld on_main_thread=%ld\n", a->onFd, a->onTimer, a->callbacks);
    CloseEditor(a);
  }
  else if (scenario == "scale")
  {
    OpenEditor(a, 0);
    RunFor(host, { a }, secs);
    ReportSizes(a, "open");
    CloseEditor(a);
  }
  else if (scenario == "rescale")
  {
    OpenEditor(a, 0);
    RunFor(host, { a }, 1.5);
    ReportSizes(a, "before");
    printf("set_scale(%g) -> %d\n", secs, a->gui->set_scale(a->plugin, secs));
    RunFor(host, { a }, 1.5);
    ReportSizes(a, "after");
    CloseEditor(a);
  }
  else if (scenario == "reopen")
  {
    OpenEditor(a, 0);
    RunFor(host, { a }, 1.0);
    ReportSizes(a, "first open");
    CloseEditor(a);
    OpenEditor(a, 0);
    RunFor(host, { a }, 1.0);
    ReportSizes(a, "second open");
    CloseEditor(a);
  }
  else if (scenario == "parent-death")
  {
    OpenEditor(a, 0);
    RunFor(host, { a }, 1.0);
    printf("destroying the parent 0x%lx before gui.destroy\n", a->parent);
    XDestroyWindow(host.display, a->parent);
    XSync(host.display, False);
    RunFor(host, { a }, 1.0);
    CloseEditor(a, false);
    RunFor(host, { a }, 0.5);
    printf("after gui.destroy: timers=%zu fds=%zu\n", a->timers.size(), a->fds.size());
  }
  else if (scenario == "two")
  {
    Instance* b = Create(host);
    OpenEditor(a, 0);
    OpenEditor(b, 700);
    RunFor(host, { a, b }, secs);
    Census("two open");
    printf("a: on_fd=%ld on_timer=%ld  b: on_fd=%ld on_timer=%ld\n", a->onFd, a->onTimer, b->onFd, b->onTimer);
    CloseEditor(a);
    RunFor(host, { a, b }, 0.5);
    CloseEditor(b);
    Census("two closed");
    Destroy(b);
  }
  else if (scenario == "churn")
  {
    OpenEditor(a, 0); RunFor(host, { a }, 0.3); CloseEditor(a);
    Census("after first cycle");
    for (int k = 0; k < static_cast<int>(secs); k++)
    {
      OpenEditor(a, 0);
      RunFor(host, { a }, (k % 5 == 0) ? 0.0 : 0.05);
      CloseEditor(a);
    }
    Census("after churn");
    printf("registrations left: timers=%zu fds=%zu\n", a->timers.size(), a->fds.size());
  }
  Destroy(a);
  Census("end");
  host.entry->deinit();
  XCloseDisplay(host.display);
  printf("HOST ALIVE, exit 0\n");
  return 0;
}
