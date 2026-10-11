#include "IPlug/Linux/IPlugLinuxPathUtils.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;
using namespace iplug::linux_paths;

namespace {

int gFailures = 0;

void ExpectEqual(const std::string& actual, const std::string& expected, const char* label)
{
  if (actual != expected)
  {
    std::cerr << label << ": expected '" << expected << "', got '" << actual << "'\n";
    ++gFailures;
  }
}

void ExpectEmpty(const std::string& actual, const char* label)
{
  ExpectEqual(actual, {}, label);
}

void TestPluginResourcePath()
{
  ExpectEqual(
    PluginResourcePathFromModule(
      "/opt/Stillwell/Event Horizon.vst3/Contents/x86_64-linux/eventhorizon.so"),
    "/opt/Stillwell/Event Horizon.vst3/Contents/Resources/",
    "VST3 resource path");

  ExpectEmpty(
    PluginResourcePathFromModule("/opt/Stillwell/eventhorizon.so"),
    "non-bundle module path");
  ExpectEmpty(
    PluginResourcePathFromModule(
      "/opt/Stillwell/Event Horizon.vst3/Other/x86_64-linux/eventhorizon.so"),
    "non-Contents bundle path");
  ExpectEmpty(
    PluginResourcePathFromModule(
      "/opt/Stillwell/Event Horizon.vst3/Contents/x86_64-win/eventhorizon.so"),
    "non-Linux architecture directory");

  ExpectEqual(
    PluginResourcePathFromModule("/home/tester/.clap/eventhorizon.clap"),
    "/home/tester/.clap/eventhorizon.resources/",
    "CLAP adjacent resource path");

  ExpectEqual(
    PluginResourcePathFromModule("/opt/eventhorizon/EventHorizon"),
    "/opt/eventhorizon/EventHorizon.resources/",
    "standalone app adjacent resource path");
}

void TestXDGPaths()
{
  ExpectEqual(
    ResolveXDGPath("/tmp/xdg-data", "/home/tester", ".local/share"),
    "/tmp/xdg-data",
    "absolute XDG path");
  ExpectEqual(
    ResolveXDGPath("relative-data", "/home/tester", ".local/share"),
    "/home/tester/.local/share",
    "relative XDG path falls back");
  ExpectEqual(
    ResolveXDGPath(nullptr, "/home/tester/", ".config"),
    "/home/tester/.config",
    "HOME fallback path");
  ExpectEmpty(
    ResolveXDGPath(nullptr, nullptr, ".cache"),
    "missing XDG and HOME");
}

void TestResourceLookup()
{
  const fs::path root = fs::temp_directory_path() / "iplug-linux-paths-test";
  std::error_code error;
  fs::remove_all(root, error);
  fs::create_directories(root / "Resources" / "img");
  std::ofstream(root / "Resources" / "img" / "background.png") << "fixture";

  ExpectEqual(
    FindResource(root / "Resources", "img/background.png"),
    (root / "Resources" / "img" / "background.png").string(),
    "existing bundle resource");
  ExpectEmpty(
    FindResource(root / "Resources", "img/missing.png"),
    "missing bundle resource");
  ExpectEmpty(
    FindResource(root / "Resources", "../outside.png"),
    "resource traversal");
  ExpectEmpty(
    FindResource(root / "missing", "img/background.png"),
    "missing resource directory");

  fs::remove_all(root, error);
}

void TestCLAPResourceLookup()
{
  const fs::path root = fs::temp_directory_path() / "iplug-clap-resources-test";
  std::error_code error;
  fs::remove_all(root, error);
  fs::create_directories(root / "eventhorizon.resources");
  std::ofstream(root / "eventhorizon.resources" / "eh_bg.png") << "fixture";

  const auto resourceDirectory = PluginResourcePathFromModule(root / "eventhorizon.clap");
  ExpectEqual(
    FindResource(resourceDirectory, "eh_bg.png"),
    (root / "eventhorizon.resources" / "eh_bg.png").string(),
    "CLAP background resource lookup");

  fs::remove_all(root, error);
}

} // namespace

int main()
{
  TestPluginResourcePath();
  TestXDGPaths();
  TestResourceLookup();
  TestCLAPResourceLookup();
  return gFailures == 0 ? 0 : 1;
}
