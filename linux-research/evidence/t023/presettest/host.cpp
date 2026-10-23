// Headless CLAP host for the T-023 review: logs clap_host_params calls (thread, flags, value read back).
// Usage: host <plugin.clap> [noparams]
// No timer-support / posix-fd-support, so iPlug runs its timers on a plug-in thread.
#include <clap/clap.h>
#include <dlfcn.h>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>

static std::thread::id gMain;
static std::atomic<bool> gCallbackRequested { false };
static const clap_plugin_t* gPlugin = nullptr;
static const clap_plugin_params_t* gParams = nullptr;
static bool gNoParams = false;
static int gRescans = 0;

static const char* ThreadName() { return std::this_thread::get_id() == gMain ? "main" : "OTHER"; }

static void Rescan(const clap_host_t*, clap_param_rescan_flags flags)
{
  ++gRescans;
  double v = -1;
  clap_param_info info {};
  if (gParams && gParams->get_info(gPlugin, 0, &info))
    gParams->get_value(gPlugin, info.id, &v);
  fprintf(stderr, "[host] params.rescan(flags=0x%x) on %s thread; param0 get_value=%.4f\n", flags, ThreadName(), v);
}
static void Clear(const clap_host_t*, clap_id, clap_param_clear_flags) { fprintf(stderr, "[host] params.clear\n"); }
static void RequestFlush(const clap_host_t*) { fprintf(stderr, "[host] params.request_flush on %s thread\n", ThreadName()); }
static const clap_host_params_t kHostParams { Rescan, Clear, RequestFlush };

static bool IsMain(const clap_host_t*) { return std::this_thread::get_id() == gMain; }
static bool IsAudio(const clap_host_t*) { return std::this_thread::get_id() != gMain; }
static const clap_host_thread_check_t kThreadCheck { IsMain, IsAudio };

static const void* GetExtension(const clap_host_t*, const char* id)
{
  if (!strcmp(id, CLAP_EXT_PARAMS)) return gNoParams ? nullptr : &kHostParams;
  if (!strcmp(id, CLAP_EXT_THREAD_CHECK)) return &kThreadCheck;
  return nullptr;
}
static void RequestRestart(const clap_host_t*) {}
static void RequestProcess(const clap_host_t*) {}
static void RequestCallback(const clap_host_t*)
{
  fprintf(stderr, "[host] request_callback on %s thread\n", ThreadName());
  gCallbackRequested = true;
}

static void PumpMain(int ms)
{
  auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
  while (std::chrono::steady_clock::now() < end)
  {
    if (gCallbackRequested.exchange(false))
    {
      fprintf(stderr, "[host] -> on_main_thread\n");
      gPlugin->on_main_thread(gPlugin);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
}

int main(int argc, char** argv)
{
  gMain = std::this_thread::get_id();
  gNoParams = argc > 2 && !strcmp(argv[2], "noparams");
  void* lib = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
  if (!lib) { fprintf(stderr, "dlopen: %s\n", dlerror()); return 1; }
  auto* entry = (const clap_plugin_entry_t*) dlsym(lib, "clap_entry");
  entry->init(argv[1]);
  auto* factory = (const clap_plugin_factory_t*) entry->get_factory(CLAP_PLUGIN_FACTORY_ID);
  const clap_plugin_descriptor_t* desc = factory->get_plugin_descriptor(factory, 0);

  clap_host_t host { CLAP_VERSION, nullptr, "t023r-host", "review", "", "1", GetExtension, RequestRestart, RequestProcess, RequestCallback };
  gPlugin = factory->create_plugin(factory, &host, desc->id);
  if (!gPlugin || !gPlugin->init(gPlugin)) { fprintf(stderr, "create/init failed\n"); return 1; }
  gParams = (const clap_plugin_params_t*) gPlugin->get_extension(gPlugin, CLAP_EXT_PARAMS);
  fprintf(stderr, "[host] init done (host params: %s)\n", gNoParams ? "absent" : "present");

  fprintf(stderr, "[host] activate\n");
  gPlugin->activate(gPlugin, 48000, 1, 512);
  PumpMain(50);

  std::atomic<bool> stop { false };
  std::thread audio([&] {
    gPlugin->start_processing(gPlugin);
    std::vector<float> l(512), r(512), ol(512), orr(512);
    float* in[2] { l.data(), r.data() };
    float* out[2] { ol.data(), orr.data() };
    clap_audio_buffer_t ib {}, ob {};
    ib.data32 = in; ib.channel_count = 2;
    ob.data32 = out; ob.channel_count = 2;
    clap_input_events_t ie { nullptr, [](const clap_input_events_t*) -> uint32_t { return 0; },
                             [](const clap_input_events_t*, uint32_t) -> const clap_event_header_t* { return nullptr; } };
    clap_output_events_t oe { nullptr, [](const clap_output_events_t*, const clap_event_header_t*) { return true; } };
    clap_process_t p {};
    p.frames_count = 512; p.steady_time = -1;
    p.audio_inputs = &ib; p.audio_outputs = &ob; p.audio_inputs_count = 1; p.audio_outputs_count = 1;
    p.in_events = &ie; p.out_events = &oe;
    while (!stop) { gPlugin->process(gPlugin, &p); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
    gPlugin->stop_processing(gPlugin);
  });
  PumpMain(1500);
  stop = true;
  audio.join();
  PumpMain(50);
  gPlugin->deactivate(gPlugin);
  gPlugin->destroy(gPlugin);
  entry->deinit();
  fprintf(stderr, "[host] done, %d rescan(s)\n", gRescans);
  return 0;
}
