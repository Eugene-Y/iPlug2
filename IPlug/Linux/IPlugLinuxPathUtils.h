/*
 ==============================================================================

 This file is part of the iPlug 2 library. Copyright (C) the iPlug 2 developers.

 See LICENSE.txt for more info.

 ==============================================================================
*/

#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace iplug {
namespace linux_paths {

inline std::string RemoveTrailingSeparators(std::string path)
{
  while (path.size() > 1 && path.back() == '/')
    path.pop_back();

  return path;
}

inline std::string VST3ResourcePathFromModule(const std::filesystem::path& modulePath)
{
  const auto architectureDirectory = modulePath.parent_path();
  const auto architectureName = architectureDirectory.filename().string();
  constexpr std::string_view linuxSuffix = "-linux";

  if (architectureName.size() <= linuxSuffix.size()
      || architectureName.compare(
        architectureName.size() - linuxSuffix.size(), linuxSuffix.size(), linuxSuffix) != 0)
    return {};

  const auto contentsDirectory = architectureDirectory.parent_path();
  if (contentsDirectory.filename() != "Contents")
    return {};

  const auto bundleDirectory = contentsDirectory.parent_path();
  if (bundleDirectory.extension() != ".vst3")
    return {};

  auto result = (contentsDirectory / "Resources").lexically_normal().string();
  result.push_back('/');
  return result;
}

/** A flat module (a .clap, or the standalone executable) keeps its resources in <stem>.resources/ beside it. */
inline std::string FlatModuleResourcePathFromModule(const std::filesystem::path& modulePath)
{
  if (modulePath.extension() != ".clap" && !modulePath.extension().empty())
    return {};

  auto result = modulePath.parent_path()
    / (modulePath.stem().string() + ".resources");
  auto resourcePath = result.lexically_normal().string();
  resourcePath.push_back('/');
  return resourcePath;
}

inline std::string PluginResourcePathFromModule(const std::filesystem::path& modulePath)
{
  auto resourcePath = VST3ResourcePathFromModule(modulePath);
  if (!resourcePath.empty())
    return resourcePath;

  return FlatModuleResourcePathFromModule(modulePath);
}

inline std::string ResolveXDGPath(
  const char* configuredPath, const char* homePath, const char* fallbackRelativePath)
{
  if (configuredPath && configuredPath[0] == '/')
    return RemoveTrailingSeparators(configuredPath);

  if (!homePath || homePath[0] != '/' || !fallbackRelativePath || !fallbackRelativePath[0])
    return {};

  return (std::filesystem::path(homePath) / fallbackRelativePath).lexically_normal().string();
}

inline std::string FindResource(
  const std::filesystem::path& resourceDirectory, const std::filesystem::path& relativeName)
{
  if (relativeName.empty() || relativeName.is_absolute())
    return {};

  const auto normalizedName = relativeName.lexically_normal();
  for (const auto& component : normalizedName)
  {
    if (component == "..")
      return {};
  }

  const auto candidate = (resourceDirectory / normalizedName).lexically_normal();
  std::error_code error;
  if (!std::filesystem::is_regular_file(candidate, error) || error)
    return {};

  return candidate.string();
}

} // namespace linux_paths
} // namespace iplug
