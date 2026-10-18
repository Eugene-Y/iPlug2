// T-028 table test for parameter texts, through a plug-in's CLAP params extension (the same calls a host
// and clap-validator's param-conversions make).
// Build: c++ -std=c++17 -O2 -I<CLAP_SDK>/include param-text.cpp -o param-text -ldl
// Usage: param-text <plugin.clap> [grid=4000] [random=4000] [seed=1] [verbose=0] [dump=<file>]
//   dump: writes "id<TAB>value<TAB>text" for the grid and random values (the same set for any build with the
//   same params), to diff the display texts of two builds.
//
// For every parameter and every test value v1 (all steps of a stepped param; otherwise a grid, random values
// and the values right at each point where the text changes, found by bisection, and 2 ulps either side):
//   t1 = value_to_text(v1), v2 = text_to_value(t1), t2 = value_to_text(v2)
//   FAIL      t1 != t2 (what clap-validator checks), or text_to_value refuses t1
//   JUMP      some value between v1 and v2 shows a text other than t1, i.e. v2 is not in v1's display bin
// Then "nan" must parse to a finite value, and the typed cases below (text -> value -> text) must give the
// expected text.
#include <clap/clap.h>
#include <dlfcn.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

static const void* hostGetExtension(const clap_host_t*, const char*) { return nullptr; }
static void hostNoop(const clap_host_t*) {}

struct Typed { const char* param; const char* text; const char* expect; };

// Gneiss's typed cases: what a user types into a host's parameter field or Gneiss's own text entry.
static const Typed kTyped[] = {
  {"main hz", "1.06 kHz", "1.06 kHz"}, {"main hz", "1060", "1.06 kHz"}, {"main hz", "1060 Hz", "1.06 kHz"},
  {"main hz", "1.06khz", "1.06 kHz"}, {"main hz", " 2 KHZ ", "2.00 kHz"}, {"main hz", "15.5 kHz", "15.5 kHz"},
  {"main hz", "440", "440.0 Hz"}, {"main hz", "99999", "20.0 kHz"}, {"main hz", "garbage", "1.0 Hz"},
  {"preFlt A hz", "1.06 kHz", "1.06 kHz"}, {"preFlt B hz", "250 Hz", "250.0 Hz"},
  {"gain", "-3 dB", "-3.0 dB"}, {"gain", "-3", "-3.0 dB"}, {"gain", "+6.5 dB", "6.5 dB"}, {"gain", "-0.04", "0.0 dB"},
  {"gain on dry", "50%", "50%"}, {"gain on dry", "50", "50%"}, {"gain on dry", "50 %", "50%"}, {"gain on dry", "150%", "100%"},
  {"damping hold", "H 5.0 ms", "H 5.0 ms"}, {"damping hold", "5", "H 5.0 ms"}, {"damping hold", "h5ms", "H 5.0 ms"},
  {"damping hold", "H 10 ms", "H 10 ms"},
  {"damping release", "R 1.50 s", "R 1.50 s"}, {"damping release", "1.5 s", "R 1.50 s"}, {"damping release", "1500", "R 1.50 s"},
  {"damping release", "R 42 ms", "R 42 ms"}, {"damping release", "R 5.5 ms", "R 5.5 ms"},
  {"damping release", "R inf", "R inf"}, {"damping release", "inf", "R inf"}, {"damping release", "2000", "R inf"},
  {"damping release", "R 2.00 s", "R inf"},
  {"damping thresh", "-40 dB", "-40.0 dB"}, {"damping thresh", "-40", "-40.0 dB"},
  {"sat type", "TANH", "TANH"}, {"sat type", "ASINH", "ASINH"}, {"sat type", "tassym", "TASSYM"},
  {"sat type", "ASINH>TASSYM 30%", "ASINH>TASSYM 30%"}, {"sat type", "TASSYM>TANH 70%", "TASSYM>TANH 70%"},
  {"sat type", "1.3", "ASINH>TASSYM 30%"}, {"sat type", "TANH>ASINH 100%", "ASINH"}, {"sat type", "TANH > ASINH 30%", "TANH>ASINH 30%"},
  {"main hz midi glide ms", "250 ms", "250 ms"}, {"main hz midi glide ms", "250", "250 ms"},
  {"main hz midi glide ms", "1.50 s", "1.50 s"}, {"main hz midi glide ms", "1.5s", "1.50 s"},
  {"pt tilt", "-0.0004", "0.000"}, {"preFlt band mode", "-0.0003", "0.000"}, {"main UB u", "-0.00004", "0.0000"},
  {"preFlt B UB bp", "-0.00001", "0.0000"}, {"pt tilt", "-0.25", "-0.250"}, {"main UB y4", "nan", "0.0000"},
  {"main res", "nan", "0.0000"}, {"main flt detune", "nan", "0"}, {"main hz", "nan", "1.0 Hz"},
};

int main(int argc, char** argv)
{
  if (argc < 2) { fprintf(stderr, "usage: %s plugin.clap [grid=4000] [random=4000] [seed=1] [verbose=0]\n", argv[0]); return 2; }
  std::string path = argv[1];
  std::map<std::string, std::string> kv;
  for (int i = 2; i < argc; i++) { const char* eq = strchr(argv[i], '='); if (eq) kv[std::string(argv[i], eq - argv[i])] = eq + 1; }
  auto num = [&](const char* k, double d) { return kv.count(k) ? atof(kv[k].c_str()) : d; };
  const int grid = (int) num("grid", 4000), randoms = (int) num("random", 4000), verbose = (int) num("verbose", 0);
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
  clap_host_t host {CLAP_VERSION, nullptr, "t028-param-text", "t028", "", "1", hostGetExtension, hostNoop, hostNoop, hostNoop};
  const clap_plugin_t* plug = factory->create_plugin(factory, &host, desc->id);
  if (!plug || !plug->init(plug)) { fprintf(stderr, "init\n"); return 1; }
  auto* params = (const clap_plugin_params_t*) plug->get_extension(plug, CLAP_EXT_PARAMS);
  FILE* dump = kv.count("dump") ? fopen(kv["dump"].c_str(), "w") : nullptr;
  if (!params) { fprintf(stderr, "no params ext\n"); return 1; }

  long totalTests = 0, totalFails = 0, totalJumps = 0, typedFails = 0, nanFails = 0;
  std::map<std::string, clap_id> byName;
  const uint32_t count = params->count(plug);
  for (uint32_t pi = 0; pi < count; pi++)
  {
    clap_param_info_t info {};
    params->get_info(plug, pi, &info);
    byName[info.name] = info.id;
    auto text = [&](double v) { char b[256] = {0}; if (!params->value_to_text(plug, info.id, v, b, sizeof b)) return std::string("<refused>"); return std::string(b); };
    const bool stepped = info.flags & CLAP_PARAM_IS_STEPPED;
    const double lo = info.min_value, hi = info.max_value;

    std::vector<double> values;
    if (stepped)
      for (double v = lo; v <= hi; v += 1) values.push_back(v);
    else
    {
      for (int i = 0; i <= grid; i++) values.push_back(lo + (hi - lo) * i / grid);
      for (int i = 0; i < randoms; i++) values.push_back(lo + (hi - lo) * rnd());
      if (dump)
        for (double v : values) fprintf(dump, "%u\t%.17g\t%s\n", info.id, v, text(v).c_str());
      // Where the text changes between two grid neighbours, bisect to the change and test around it.
      for (int i = 0; i < grid; i++)
      {
        double a = lo + (hi - lo) * i / grid, b = lo + (hi - lo) * (i + 1) / grid;
        const std::string ta = text(a);
        if (text(b) == ta) continue;
        for (int it = 0; it < 60 && b - a > 0; it++) { const double m = 0.5 * (a + b); if (m <= a || m >= b) break; (text(m) == ta ? a : b) = m; }
        double v = a;
        for (int k = 0; k < 2; k++) v = std::nextafter(v, lo);
        for (int k = 0; k < 6; k++, v = std::nextafter(v, hi)) if (v >= lo && v <= hi) values.push_back(v);
      }
    }

    if (dump && stepped)
      for (double v : values) fprintf(dump, "%u\t%.17g\t%s\n", info.id, v, text(v).c_str());

    long fails = 0, jumps = 0;
    std::vector<std::string> examples, jumpExamples;
    for (double v1 : values)
    {
      const std::string t1 = text(v1);
      double v2 = 0;
      const bool ok = params->text_to_value(plug, info.id, t1.c_str(), &v2);
      const std::string t2 = ok ? text(v2) : std::string("<refused>");
      totalTests++;
      if (!ok || t1 != t2)
      {
        fails++;
        if (examples.size() < 4) { char b[512]; snprintf(b, sizeof b, "%.17g '%s' -> %.17g '%s'", v1, t1.c_str(), v2, t2.c_str()); examples.push_back(b); }
        continue;
      }
      bool jumped = false;
      for (int k = 1; k < 16 && !jumped; k++) jumped = text(v1 + (v2 - v1) * k / 16) != t1;
      if (jumped)
      {
        jumps++;
        if (jumpExamples.size() < 3) { char b[512]; snprintf(b, sizeof b, "%.17g '%s' -> %.17g", v1, t1.c_str(), v2); jumpExamples.push_back(b); }
      }
    }

    double vn = 0;
    const bool nanOk = params->text_to_value(plug, info.id, "nan", &vn) ? std::isfinite(vn) : true;
    if (!nanOk) nanFails++;

    totalFails += fails; totalJumps += jumps;
    if (fails || jumps || !nanOk || verbose)
    {
      printf("%-28s id=%-3u %s tests=%zu FAIL=%ld JUMP=%ld%s\n", info.name, info.id, stepped ? "stepped" : "double ", values.size(), fails, jumps, nanOk ? "" : " NAN-PARSES-TO-NON-FINITE");
      for (auto& e : examples) printf("    fail: %s\n", e.c_str());
      for (auto& e : jumpExamples) printf("    jump: %s\n", e.c_str());
    }
  }

  printf("--- typed text -> value -> text\n");
  for (const auto& t : kTyped)
  {
    if (!byName.count(t.param)) { printf("    ? no param '%s'\n", t.param); typedFails++; continue; }
    const clap_id id = byName[t.param];
    double v = 0;
    char b[256] = {0};
    const bool ok = params->text_to_value(plug, id, t.text, &v) && params->value_to_text(plug, id, v, b, sizeof b);
    const bool pass = ok && strcmp(b, t.expect) == 0;
    if (!pass) typedFails++;
    printf("    %s %-22s '%s' -> %.10g -> '%s'%s\n", pass ? "ok  " : "FAIL", t.param, t.text, v, b, pass ? "" : (std::string("   expected '") + t.expect + "'").c_str());
  }

  printf("TOTAL params=%u tests=%ld FAIL=%ld JUMP=%ld nan-fails=%ld typed-fails=%ld\n", count, totalTests, totalFails, totalJumps, nanFails, typedFails);
  if (dump) fclose(dump);
  plug->destroy(plug);
  entry->deinit();
  return (totalFails || nanFails || typedFails) ? 1 : 0;
}
