// Offline CLAP host for T-026 (extends T-024's render-clap.cpp): deterministic stereo input, block-size
// schedules, 0-frame blocks, per-block parameter sweeps, a re-activation, and a heap-allocation counter
// armed only around plugin->process() on this thread.
// Build (Linux): g++ -std=c++17 -O2 -rdynamic -I<CLAP_SDK>/include render-t026.cpp -o render-t026 -ldl
//   -DT026_NO_ALLOC_HOOK for runs under ASan (LD_PRELOAD libasan).
//   -rdynamic exports this file's malloc family, so the plug-in's (and libstdc++'s) calls resolve here.
// Usage: render-t026 <plugin.clap> <out.raw|-> [key=value ...]
//   sr=48000 frames=96000 f64=0|1 inplace=0|1 seed=1
//   blocks=512[,64,1024,...]  block sizes, each held for seg frames (default frames / count), then cycled
//   block=0                   random sizes 1..1024 instead (maxblock 1024)
//   maxblock=N                max_frames_count passed to activate (default: largest block)
//   zeroevery=N               a 0-frame block before every Nth block (carries that block's events)
//   zeronull=1                0-frame blocks carry null channel pointers (VST3 parameter flushes, e.g. FL Studio)
//   p<id>=v                   sent with the first block (CLAP value: normalized for doubles, plain for int/enum)
//   sweep=<id>:<lo>:<hi>[:i][/<id>:...]  triangle lo..hi over sweepframes (default sr/2), one event per block;
//                             ":i" rounds (enums/ints)
//   react=<frame>             deactivate + activate again at that frame, with sr2 (default sr) and maxblock2
//                             (default maxblock); blocks2 replaces blocks from there on
//   bt=N                      print a backtrace for the first N counted allocations (stderr)
// Output: interleaved float64 stereo. Prints non-finite count, input change, process() time and allocations
// (count only while armed; "after-first" excludes the first block after each activate).
#include <clap/clap.h>
#include <dlfcn.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#if defined(__linux__) && !defined(T026_NO_ALLOC_HOOK) // the hook would bypass ASan's allocator
#include <execinfo.h>
#include <malloc.h>
extern "C" {
void* __libc_malloc(size_t);
void* __libc_calloc(size_t, size_t);
void* __libc_realloc(void*, size_t);
void* __libc_memalign(size_t, size_t);
void __libc_free(void*);
}

static thread_local int tArmed = 0;
static long gAllocs = 0, gAllocBytes = 0, gBacktraces = 0;

static void noteAlloc(size_t n)
{
  if (!tArmed) return;
  gAllocs++; gAllocBytes += (long) n;
  if (gBacktraces > 0)
  {
    gBacktraces--;
    tArmed = 0;
    void* frames[40];
    const int depth = backtrace(frames, 40);
    fprintf(stderr, "--- allocation of %zu bytes:\n", n);
    backtrace_symbols_fd(frames, depth, 2);
    tArmed = 1;
  }
}

extern "C" {
void* malloc(size_t n) { noteAlloc(n); return __libc_malloc(n); }
void* calloc(size_t a, size_t b) { noteAlloc(a * b); return __libc_calloc(a, b); }
void* realloc(void* p, size_t n) { noteAlloc(n); return __libc_realloc(p, n); }
void free(void* p) { __libc_free(p); }
void* memalign(size_t al, size_t n) { noteAlloc(n); return __libc_memalign(al, n); }
void* aligned_alloc(size_t al, size_t n) { noteAlloc(n); return __libc_memalign(al, n); }
int posix_memalign(void** out, size_t al, size_t n)
{
  noteAlloc(n);
  void* p = __libc_memalign(al, n);
  if (!p) return 12;
  *out = p;
  return 0;
}
void* valloc(size_t n) { noteAlloc(n); return __libc_memalign(4096, n); }
}
#define ARM() (tArmed = 1)
#define DISARM() (tArmed = 0)
#else
static long gAllocs = 0, gAllocBytes = 0, gBacktraces = 0;
#define ARM() ((void) 0)
#define DISARM() ((void) 0)
#endif

static const void* hostGetExtension(const clap_host_t*, const char*) { return nullptr; }
static void hostNoop(const clap_host_t*) {}

static uint32_t evSize(const clap_input_events_t* l) { return (uint32_t) ((std::vector<clap_event_param_value_t>*) l->ctx)->size(); }
static const clap_event_header_t* evGet(const clap_input_events_t* l, uint32_t i) { return &(*(std::vector<clap_event_param_value_t>*) l->ctx)[i].header; }
static bool outPush(const clap_output_events_t*, const clap_event_header_t*) { return true; }

static std::vector<int> parseList(const std::string& s)
{
  std::vector<int> v;
  for (const char* p = s.c_str(); *p;)
  {
    v.push_back(atoi(p));
    const char* c = strchr(p, ',');
    if (!c) break;
    p = c + 1;
  }
  return v;
}

struct Sweep { clap_id id; double lo, hi; bool round; };

static clap_event_param_value_t paramEvent(clap_id id, double value)
{
  clap_event_param_value_t e {};
  e.header = {sizeof(e), 0, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_PARAM_VALUE, 0};
  e.param_id = id; e.cookie = nullptr; e.note_id = -1; e.port_index = -1; e.channel = -1; e.key = -1;
  e.value = value;
  return e;
}

int main(int argc, char** argv)
{
  if (argc < 3) { fprintf(stderr, "usage: %s plugin.clap out.raw [k=v ...]\n", argv[0]); return 2; }
  std::string path = argv[1];
  const char* outPath = argv[2];
  std::map<std::string, std::string> kv;
  for (int i = 3; i < argc; i++) { const char* eq = strchr(argv[i], '='); if (eq) kv[std::string(argv[i], eq - argv[i])] = eq + 1; }
  auto num = [&](const char* k, double d) { return kv.count(k) ? atof(kv[k].c_str()) : d; };
  const double sr = num("sr", 48000);
  const long frames = (long) num("frames", 96000);
  const bool f64 = num("f64", 0) != 0;
  const bool inplace = num("inplace", 0) != 0;
  const int zeroEvery = (int) num("zeroevery", 0);
  const bool zeroNull = num("zeronull", 0) != 0;
  const long reactAt = (long) num("react", -1);
  const double sr2 = num("sr2", sr);
  gBacktraces = (long) num("bt", 0);
  uint64_t rng = (uint64_t) num("seed", 1);
  auto rnd = [&]() { rng = rng * 6364136223846793005ULL + 1442695040888963407ULL; return (rng >> 11) * (1.0 / 9007199254740992.0); };

  const bool randomBlocks = kv.count("block") && atoi(kv["block"].c_str()) == 0;
  std::vector<int> blocks = parseList(kv.count("blocks") ? kv["blocks"] : (kv.count("block") ? kv["block"] : "512"));
  std::vector<int> blocks2 = kv.count("blocks2") ? parseList(kv["blocks2"]) : blocks;
  auto maxOf = [](const std::vector<int>& v) { int m = 1; for (int b : v) m = std::max(m, b); return m; };
  const int maxBlock = (int) num("maxblock", randomBlocks ? 1024 : maxOf(blocks));
  const int maxBlock2 = (int) num("maxblock2", std::max(maxBlock, maxOf(blocks2)));
  const long seg = (long) num("seg", (double) frames / blocks.size());

  std::vector<Sweep> sweeps;
  if (kv.count("sweep"))
    for (const char* p = kv["sweep"].c_str(); *p;)
    {
      Sweep s {};
      s.id = (clap_id) atoi(p);
      const char* a = strchr(p, ':'); s.lo = atof(a + 1);
      const char* b = strchr(a + 1, ':'); s.hi = atof(b + 1);
      const char* end = strchr(p, '/');
      const char* r = strstr(b + 1, ":i");
      s.round = r && (!end || r < end);
      sweeps.push_back(s);
      if (!end) break;
      p = end + 1;
    }
  const double sweepFrames = num("sweepframes", sr / 2);

  std::string bin = path;
#ifdef __APPLE__
  { std::string base = path.substr(path.find_last_of('/') + 1); bin = path + "/Contents/MacOS/" + base.substr(0, base.rfind('.')); }
#endif
#if defined(__linux__) && !defined(T026_NO_ALLOC_HOOK)
  { void* warm[4]; backtrace(warm, 4); } // loads libgcc_s now, not inside the first counted allocation
#endif
  void* lib = dlopen(bin.c_str(), RTLD_NOW | RTLD_LOCAL);
  if (!lib) { fprintf(stderr, "dlopen: %s\n", dlerror()); return 1; }
  auto* entry = (const clap_plugin_entry_t*) dlsym(lib, "clap_entry");
  if (!entry || !entry->init(path.c_str())) { fprintf(stderr, "entry\n"); return 1; }
  auto* factory = (const clap_plugin_factory_t*) entry->get_factory(CLAP_PLUGIN_FACTORY_ID);
  const clap_plugin_descriptor_t* desc = factory->get_plugin_descriptor(factory, 0);

  clap_host_t host {CLAP_VERSION, nullptr, "t026-render", "t026", "", "1", hostGetExtension, hostNoop, hostNoop, hostNoop};
  const clap_plugin_t* plug = factory->create_plugin(factory, &host, desc->id);
  if (!plug || !plug->init(plug)) { fprintf(stderr, "init\n"); return 1; }
  if (!plug->activate(plug, sr, 1, maxBlock) || !plug->start_processing(plug)) { fprintf(stderr, "activate\n"); return 1; }

  const int nch = 2;
  const int bufFrames = std::max(maxBlock, maxBlock2);
  std::vector<double> in(nch * frames), out(nch * frames, 0.0);
  for (long i = 0; i < frames; i++)
  {
    const double t = i / sr;
    in[i] = 0.4 * std::sin(2 * M_PI * 110 * t) + 0.2 * std::sin(2 * M_PI * 3520 * t) + 0.1 * (rnd() * 2 - 1);
    in[frames + i] = 0.3 * std::sin(2 * M_PI * 220 * t + 1) + 0.1 * (rnd() * 2 - 1);
  }

  std::vector<clap_event_param_value_t> firstEvents;
  for (auto& [k, v] : kv)
    if (k.size() > 1 && k[0] == 'p' && isdigit((unsigned char) k[1]))
      firstEvents.push_back(paramEvent((clap_id) atoi(k.c_str() + 1), atof(v.c_str())));
  std::vector<clap_event_param_value_t> evs, none;
  evs.reserve(firstEvents.size() + sweeps.size() + 1);

  std::vector<float> in32(nch * bufFrames), out32(nch * bufFrames);
  std::vector<double> in64(nch * bufFrames), out64(nch * bufFrames), inCopy(nch * bufFrames);
  long nonFinite = 0, inputChanged = 0, blocks_ = 0, zeroBlocks = 0, allocBlocks = 0, allocsFirst = 0, reacts = 0;
  long blockSinceActivate = 0;
  std::string allocLog;
  double secs = 0;
  long pos = 0;
  bool reacted = false;
  std::vector<int>* sched = &blocks;
  long schedStart = 0;
  int activeMax = maxBlock;

  auto process = [&](uint32_t n, bool withEvents)
  {
    float* i32[nch]; float* o32[nch]; double* i64[nch]; double* o64[nch];
    for (int c = 0; c < nch; c++)
    {
      i32[c] = &in32[c * bufFrames]; o32[c] = inplace ? i32[c] : &out32[c * bufFrames];
      i64[c] = &in64[c * bufFrames]; o64[c] = inplace ? i64[c] : &out64[c * bufFrames];
      for (uint32_t s = 0; s < n; s++) { in32[c * bufFrames + s] = (float) in[c * frames + pos + s]; in64[c * bufFrames + s] = in[c * frames + pos + s]; }
      for (uint32_t s = 0; s < n; s++) inCopy[c * bufFrames + s] = f64 ? in64[c * bufFrames + s] : in32[c * bufFrames + s];
      if (n == 0 && zeroNull) i32[c] = o32[c] = nullptr, i64[c] = o64[c] = nullptr;
    }
    clap_audio_buffer_t ain {f64 ? nullptr : i32, f64 ? i64 : nullptr, nch, 0, 0};
    clap_audio_buffer_t aout {f64 ? nullptr : o32, f64 ? o64 : nullptr, nch, 0, 0};
    clap_input_events_t inEv {withEvents ? &evs : &none, evSize, evGet};
    clap_output_events_t outEv {nullptr, outPush};
    clap_process_t proc {pos, n, nullptr, &ain, &aout, 1, 1, &inEv, &outEv};
    const long before = gAllocs;
    auto t0 = std::chrono::steady_clock::now();
    ARM();
    plug->process(plug, &proc);
    DISARM();
    secs += std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    const long got = gAllocs - before;
    if (got)
    {
      if (blockSinceActivate == 0) allocsFirst += got;
      else allocBlocks++;
      if (allocLog.size() < 600)
        allocLog += " [block " + std::to_string(blocks_) + " n=" + std::to_string(n) + " allocs=" + std::to_string(got) + "]";
    }
    blockSinceActivate++;
    blocks_++;
    if (n == 0) { zeroBlocks++; return; }
    for (int c = 0; c < nch; c++)
      for (uint32_t s = 0; s < n; s++)
      {
        const double o = f64 ? o64[c][s] : o32[c][s];
        if (!std::isfinite(o)) nonFinite++;
        out[(pos + s) * nch + c] = o;
        if (!inplace && (f64 ? i64[c][s] : i32[c][s]) != inCopy[c * bufFrames + s]) inputChanged++;
      }
  };

  long schedBlock = 0;
  while (pos < frames)
  {
    if (!reacted && reactAt >= 0 && pos >= reactAt)
    {
      plug->stop_processing(plug);
      plug->deactivate(plug);
      if (!plug->activate(plug, sr2, 1, maxBlock2) || !plug->start_processing(plug)) { fprintf(stderr, "re-activate\n"); return 1; }
      reacted = true; reacts++; blockSinceActivate = 0;
      sched = &blocks2; schedStart = pos; schedBlock = 0; activeMax = maxBlock2;
    }
    int n;
    if (randomBlocks) n = 1 + (int) (rnd() * 1024);
    else n = (*sched)[((pos - schedStart) / seg) % sched->size()];
    if (n > activeMax) n = activeMax;
    if (pos + n > frames) n = (int) (frames - pos);

    evs.clear();
    if (blocks_ == 0) evs.insert(evs.end(), firstEvents.begin(), firstEvents.end());
    for (auto& s : sweeps)
    {
      const double ph = std::fmod(pos / sweepFrames, 1.0);
      const double tri = ph < 0.5 ? ph * 2 : 2 - ph * 2;
      double v = s.lo + (s.hi - s.lo) * tri;
      if (s.round) v = std::round(v);
      evs.push_back(paramEvent(s.id, v));
    }

    if (zeroEvery > 0 && schedBlock % zeroEvery == zeroEvery - 1)
    {
      process(0, true);
      evs.clear();
    }
    process((uint32_t) n, true);
    pos += n; schedBlock++;
  }
  plug->stop_processing(plug);
  plug->deactivate(plug);
  plug->destroy(plug);
  entry->deinit();

  if (strcmp(outPath, "-"))
  {
    FILE* f = fopen(outPath, "wb");
    fwrite(out.data(), sizeof(double), out.size(), f);
    fclose(f);
  }
  printf("sr=%g maxblock=%d f64=%d inplace=%d frames=%ld blocks=%ld zero-blocks=%ld reactivations=%ld non-finite=%ld input-changed=%ld "
         "process-s=%.4f allocs=%ld alloc-bytes=%ld allocs-first-block=%ld alloc-blocks-after-first=%ld%s\n",
         sr, maxBlock, f64, inplace, frames, blocks_, zeroBlocks, reacts, nonFinite, inputChanged, secs,
         gAllocs, gAllocBytes, allocsFirst, allocBlocks, allocLog.c_str());
  return nonFinite ? 3 : 0;
}
