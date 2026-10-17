/*
 ==============================================================================

 This file is part of the iPlug 2 library. Copyright (C) the iPlug 2 developers.

 See LICENSE.txt for  more info.

 ==============================================================================
*/

#include <dlfcn.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <atomic>
#include <filesystem>

#include "IPlugPaths.h"
#include "IPlugLinuxPathUtils.h"

BEGIN_IPLUG_NAMESPACE

namespace {

// A .clap or standalone copied without its <Name>.resources/ folder still loads, with no fonts or images; the
// customer gets a hint where it was expected instead of a silently blank UI.
void WarnIfResourceFolderMissing(const char* resourcePath)
{
  static std::atomic<bool> warned {false};
  std::error_code error;
  if (std::filesystem::is_directory(resourcePath, error) || warned.exchange(true))
    return;

  fprintf(stderr, "iPlug2: resource folder %s not found; copy it next to the plug-in.\n", resourcePath);
}

bool ModuleFilePath(void* symbol, std::filesystem::path& result)
{
  if (!symbol)
    return false;

  Dl_info info {};
  if (!dladdr(symbol, &info) || !info.dli_fname)
    return false;

  char resolvedPath[PATH_MAX] {};
  if (!realpath(info.dli_fname, resolvedPath))
    return false;

  result = resolvedPath;
  return true;
}

void SetDirectoryPath(WDL_String& path, const std::filesystem::path& directory)
{
  path.Set(directory.lexically_normal().string().c_str());
  if (path.GetLength() > 0 && path.Get()[path.GetLength() - 1] != '/')
    path.Append("/");
}

void SetXDGPath(
  WDL_String& path, const char* environmentName, const char* fallbackRelativePath)
{
  const auto resolved = linux_paths::ResolveXDGPath(
    getenv(environmentName), getenv("HOME"), fallbackRelativePath);
  path.Set(resolved.c_str());
}

} // namespace

void HostPath(WDL_String& path, const char* bundleID)
{
  (void) bundleID;
  path.Set("");

  char executablePath[PATH_MAX] {};
  const auto length = readlink("/proc/self/exe", executablePath, sizeof(executablePath) - 1);
  if (length <= 0)
    return;

  executablePath[length] = '\0';
  SetDirectoryPath(path, std::filesystem::path(executablePath).parent_path());
}

void PluginPath(WDL_String& path, void* pExtra)
{
  path.Set("");
  std::filesystem::path modulePath;
  void* symbol = pExtra ? pExtra : reinterpret_cast<void*>(&PluginPath);
  if (ModuleFilePath(symbol, modulePath))
    SetDirectoryPath(path, modulePath.parent_path());
}

void BundleResourcePath(WDL_String& path, void* pExtra)
{
  path.Set("");
  std::filesystem::path modulePath;
  void* symbol = pExtra ? pExtra : reinterpret_cast<void*>(&BundleResourcePath);
  if (!ModuleFilePath(symbol, modulePath))
    return;

  const auto resourcePath = linux_paths::PluginResourcePathFromModule(modulePath);
  path.Set(resourcePath.c_str());
}

void DesktopPath(WDL_String& path)
{
  const char* home = getenv("HOME");
  if (!home || home[0] != '/')
  {
    path.Set("");
    return;
  }

  path.Set((std::filesystem::path(home) / "Desktop").lexically_normal().string().c_str());
}

void UserHomePath(WDL_String& path)
{
  const char* home = getenv("HOME");
  path.Set(home && home[0] == '/' ? home : "");
}

void AppSupportPath(WDL_String& path, bool isSystem)
{
  if (isSystem)
    path.Set("/usr/share");
  else
    SetXDGPath(path, "XDG_DATA_HOME", ".local/share");
}

void VST3PresetsPath(
  WDL_String& path, const char* mfrName, const char* pluginName, bool isSystem)
{
  path.Set("");
  if (!CStringHasContents(mfrName) || !CStringHasContents(pluginName))
    return;

  std::filesystem::path basePath;
  if (isSystem)
  {
    basePath = "/usr/share/vst3/presets";
  }
  else
  {
    const char* home = getenv("HOME");
    if (!home || home[0] != '/')
      return;
    basePath = std::filesystem::path(home) / ".vst3" / "presets";
  }

  path.Set((basePath / mfrName / pluginName).lexically_normal().string().c_str());
}

void INIPath(WDL_String& path, const char* pluginName)
{
  SetXDGPath(path, "XDG_CONFIG_HOME", ".config");
  if (path.GetLength() > 0 && CStringHasContents(pluginName))
  {
    const auto settingsPath = std::filesystem::path(path.Get()) / pluginName;
    path.Set(settingsPath.lexically_normal().string().c_str());
  }
}

void WebViewCachePath(WDL_String& path)
{
  SetXDGPath(path, "XDG_CACHE_HOME", ".cache");
  if (path.GetLength() > 0)
  {
    const auto cachePath = std::filesystem::path(path.Get()) / "iPlug2" / "WebViewCache";
    path.Set(cachePath.lexically_normal().string().c_str());
  }
}

EResourceLocation LocateResource(
  const char* name,
  const char* type,
  WDL_String& result,
  const char* bundleID,
  void* pHInstance,
  const char* sharedResourcesSubPath)
{
  (void) type;
  (void) bundleID;
  result.Set("");
  if (!CStringHasContents(name))
    return EResourceLocation::kNotFound;

  WDL_String bundlePath;
  BundleResourcePath(bundlePath, pHInstance);
  if (bundlePath.GetLength() > 0)
  {
    WarnIfResourceFolderMissing(bundlePath.Get());
    const auto resource = linux_paths::FindResource(bundlePath.Get(), name);
    if (!resource.empty())
    {
      result.Set(resource.c_str());
      return EResourceLocation::kAbsolutePath;
    }
  }

  if (CStringHasContents(sharedResourcesSubPath))
  {
    WDL_String dataPath;
    AppSupportPath(dataPath, false);
    if (dataPath.GetLength() > 0)
    {
      const auto sharedPath =
        std::filesystem::path(dataPath.Get()) / sharedResourcesSubPath / "Resources";
      const auto resource = linux_paths::FindResource(sharedPath, name);
      if (!resource.empty())
      {
        result.Set(resource.c_str());
        return EResourceLocation::kAbsolutePath;
      }
    }
  }

  std::error_code error;
  if (std::filesystem::is_regular_file(name, error) && !error)
  {
    result.Set(std::filesystem::absolute(name).lexically_normal().string().c_str());
    return EResourceLocation::kAbsolutePath;
  }

  return EResourceLocation::kNotFound;
}

END_IPLUG_NAMESPACE
