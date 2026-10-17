// Offline CLAP renderer for T-024: feeds a deterministic stereo signal through a plug-in and writes the output.
// Build: c++ -std=c++17 -O2 -I<CLAP_SDK>/include render-clap.cpp -o render-clap -ldl
// Usage: render-clap <plugin.clap> <out.raw|-> [key=value ...]
//   sr=48000  block=512 (0 = random 0..1024, zero-frame blocks included)  frames=96000  f64=0|1
//   inplace=0|1  seed=1  skipzero=0|1 (same block sequence, but 0-frame blocks are not passed to the plug-in)  p<id>=<CLAP value: normalized for iPlug doubles, plain for int/enum> (sent with the first block)
// Output: interleaved float64 stereo. Prints non-finite count, whether any input sample was changed,
// and the process() time.
#include <clap/clap.h>
#include <dlfcn.h>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

static const void* hostGetExtension(const clap_host_t*, const char*) { return nullptr; }
static void hostNoop(const clap_host_t*) {}

static uint32_t evSize(const clap_input_events_t* l) { return (uint32_t) ((std::vector<clap_event_param_value_t>*) l->ctx)->size(); }
static const clap_event_header_t* evGet(const clap_input_events_t* l, uint32_t i) { return &(*(std::vector<clap_event_param_value_t>*) l->ctx)[i].header; }
static bool outPush(const clap_output_events_t*, const clap_event_header_t*) { return true; }

int main(int argc, char** argv)
{
  if (argc < 3) { fprintf(stderr, "usage: %s plugin.clap out.raw [k=v ...]\n", argv[0]); return 2; }
  std::string path = argv[1];
  const char* outPath = argv[2];
  std::map<std::string, std::string> kv;
  for (int i = 3; i < argc; i++) { const char* eq = strchr(argv[i], '='); if (eq) kv[std::string(argv[i], eq - argv[i])] = eq + 1; }
  auto num = [&](const char* k, double d) { return kv.count(k) ? atof(kv[k].c_str()) : d; };
  const double sr = num("sr", 48000);
  const int block = (int) num("block", 512);
  const long frames = (long) num("frames", 96000);
  const bool f64 = num("f64", 0) != 0;
  const bool inplace = num("inplace", 0) != 0;
  const bool skipZero = num("skipzero", 0) != 0;
  uint64_t rng = (uint64_t) num("seed", 1);
  auto rnd = [&]() { rng = rng * 6364136223846793005ULL + 1442695040888963407ULL; return (rng >> 11) * (1.0 / 9007199254740992.0); };

  std::string bin = path;
#ifdef __APPLE__
  { std::string base = path.substr(path.find_last_of('/') + 1); bin = path + "/Contents/MacOS/" + base.substr(0, base.rfind('.')); }
#endif
  void* lib = dlopen(bin.c_str(), RTLD_NOW | RTLD_LOCAL);
  if (!lib) { fprintf(stderr, "dlopen: %s\n", dlerror()); return 1; }
  auto* entry = (const clap_plugin_entry_t*) dlsym(lib, "clap_entry");
  if (!entry || !entry->init(path.c_str())) { fprintf(stderr, "entry\n"); return 1; }
  auto* factory = (const clap_plugin_factory_t*) entry->get_factory(CLAP_PLUGIN_FACTORY_ID);
  const clap_plugin_descriptor_t* desc = factory->get_plugin_descriptor(factory, 0);

  clap_host_t host {CLAP_VERSION, nullptr, "t024-render", "t024", "", "1", hostGetExtension, hostNoop, hostNoop, hostNoop};
  const clap_plugin_t* plug = factory->create_plugin(factory, &host, desc->id);
  if (!plug || !plug->init(plug)) { fprintf(stderr, "init\n"); return 1; }
  const int maxBlock = block > 0 ? block : 1024;
  if (!plug->activate(plug, sr, 1, maxBlock) || !plug->start_processing(plug)) { fprintf(stderr, "activate\n"); return 1; }

  const int nch = 2;
  std::vector<double> in(nch * frames), out(nch * frames, 0.0);
  for (long i = 0; i < frames; i++)
  {
    const double t = i / sr;
    in[i] = 0.4 * std::sin(2 * M_PI * 110 * t) + 0.2 * std::sin(2 * M_PI * 3520 * t) + 0.1 * (rnd() * 2 - 1);
    in[frames + i] = 0.3 * std::sin(2 * M_PI * 220 * t + 1) + 0.1 * (rnd() * 2 - 1);
  }

  std::vector<clap_event_param_value_t> events;
  for (auto& [k, v] : kv)
    if (k.size() > 1 && k[0] == 'p' && isdigit((unsigned char) k[1]))
    {
      clap_event_param_value_t e {};
      e.header = {sizeof(e), 0, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_PARAM_VALUE, 0};
      e.param_id = (clap_id) atoi(k.c_str() + 1); e.cookie = nullptr; e.note_id = -1; e.port_index = -1; e.channel = -1; e.key = -1;
      e.value = atof(v.c_str());
      events.push_back(e);
    }
  std::vector<clap_event_param_value_t> none;

  std::vector<float> in32(nch * maxBlock), out32(nch * maxBlock);
  std::vector<double> in64(nch * maxBlock), out64(nch * maxBlock), inCopy(nch * maxBlock);
  long nonFinite = 0, inputChanged = 0, blocks = 0, zeroBlocks = 0;
  double secs = 0;
  long pos = 0;
  while (pos < frames)
  {
    int n = block > 0 ? block : (int) (rnd() * 1025);
    if (block <= 0 && rnd() < 0.05) n = 0;
    if (n > maxBlock) n = maxBlock;
    if (pos + n > frames) n = (int) (frames - pos);
    float* i32[nch]; float* o32[nch]; double* i64[nch]; double* o64[nch];
    for (int c = 0; c < nch; c++)
    {
      i32[c] = &in32[c * maxBlock]; o32[c] = inplace ? i32[c] : &out32[c * maxBlock];
      i64[c] = &in64[c * maxBlock]; o64[c] = inplace ? i64[c] : &out64[c * maxBlock];
      for (int s = 0; s < n; s++) { in32[c * maxBlock + s] = (float) in[c * frames + pos + s]; in64[c * maxBlock + s] = in[c * frames + pos + s]; }
      for (int s = 0; s < n; s++) inCopy[c * maxBlock + s] = f64 ? in64[c * maxBlock + s] : in32[c * maxBlock + s];
    }
    clap_audio_buffer_t ain {f64 ? nullptr : i32, f64 ? i64 : nullptr, nch, 0, 0};
    clap_audio_buffer_t aout {f64 ? nullptr : o32, f64 ? o64 : nullptr, nch, 0, 0};
    auto& evs = blocks == 0 ? events : none;
    clap_input_events_t inEv {&evs, evSize, evGet};
    clap_output_events_t outEv {nullptr, outPush};
    clap_process_t proc {pos, (uint32_t) n, nullptr, &ain, &aout, 1, 1, &inEv, &outEv};
    auto t0 = std::chrono::steady_clock::now();
    if (n > 0 || !skipZero)
      plug->process(plug, &proc);
    secs += std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    for (int c = 0; c < nch; c++)
      for (int s = 0; s < n; s++)
      {
        const double o = f64 ? o64[c][s] : o32[c][s];
        if (!std::isfinite(o)) nonFinite++;
        out[(pos + s) * nch + c] = o;
        if (!inplace && (f64 ? i64[c][s] : i32[c][s]) != inCopy[c * maxBlock + s]) inputChanged++;
      }
    pos += n; blocks++; if (n == 0) zeroBlocks++;
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
  printf("sr=%g block=%d f64=%d inplace=%d frames=%ld blocks=%ld zero-blocks=%ld non-finite=%ld input-changed=%ld process-s=%.4f\n",
         sr, block, f64, inplace, frames, blocks, zeroBlocks, nonFinite, inputChanged, secs);
  return nonFinite ? 3 : 0;
}
