/*
 ==============================================================================

 This file is part of the iPlug 2 library. Copyright (C) the iPlug 2 developers.

 See LICENSE.txt for  more info.

 ==============================================================================
*/

#pragma once

/** @file
 * @brief System fonts by family name through fontconfig, which is dlopen'ed: without libfontconfig.so.1 the
 * plug-in still loads, and fonts by name are simply not found. Main thread only.
 */

#include <dlfcn.h>

#include "wdlstring.h"

BEGIN_IPLUG_NAMESPACE
BEGIN_IGRAPHICS_NAMESPACE

namespace linux_fonts
{

/** Fontconfig as far as a family + style lookup needs it; the opaque types and enum values are fontconfig's ABI */
struct Fontconfig
{
  struct Pattern;
  struct Config;
  static constexpr int kMatchPattern = 0;
  static constexpr int kResultMatch = 0;
  static constexpr int kWeightRegular = 80;
  static constexpr int kWeightBold = 200;
  static constexpr int kSlantRoman = 0;
  static constexpr int kSlantItalic = 100;

  Pattern* (*PatternCreate)() = nullptr;
  int (*PatternAddString)(Pattern*, const char*, const unsigned char*) = nullptr;
  int (*PatternAddInteger)(Pattern*, const char*, int) = nullptr;
  int (*ConfigSubstitute)(Config*, Pattern*, int) = nullptr;
  void (*DefaultSubstitute)(Pattern*) = nullptr;
  Pattern* (*FontMatch)(Config*, Pattern*, int*) = nullptr;
  void (*PatternDestroy)(Pattern*) = nullptr;
  int (*PatternGetString)(const Pattern*, const char*, int, unsigned char**) = nullptr;
  int (*PatternGetInteger)(const Pattern*, const char*, int, int*) = nullptr;

  /** The process-wide table, or nullptr without a usable libfontconfig. Never unloaded: fontconfig keeps
   * process-wide caches that the host may share. */
  static const Fontconfig* Get()
  {
    static Fontconfig sFC;
    static bool sTried = false;
    if (!sTried)
    {
      sTried = true;
      sFC.Load();
    }
    return sFC.mLib ? &sFC : nullptr;
  }

private:
  void Load()
  {
    mLib = dlopen("libfontconfig.so.1", RTLD_NOW | RTLD_LOCAL);
    if (!mLib)
      return;

    bool ok = true;
    auto load = [&](auto& pFunc, const char* name) {
      pFunc = reinterpret_cast<std::remove_reference_t<decltype(pFunc)>>(dlsym(mLib, name));
      ok = ok && pFunc;
    };
    load(PatternCreate, "FcPatternCreate");
    load(PatternAddString, "FcPatternAddString");
    load(PatternAddInteger, "FcPatternAddInteger");
    load(ConfigSubstitute, "FcConfigSubstitute");
    load(DefaultSubstitute, "FcDefaultSubstitute");
    load(FontMatch, "FcFontMatch");
    load(PatternDestroy, "FcPatternDestroy");
    load(PatternGetString, "FcPatternGetString");
    load(PatternGetInteger, "FcPatternGetInteger");
    if (!ok)
    {
      dlclose(mLib);
      mLib = nullptr;
    }
  }

  void* mLib = nullptr;
};

/** Finds the font file for a family name and style. Like Windows' font mapper, a missing family gives the
 * system's substitute (e.g. Liberation Sans for Arial), never no font.
 * @return true and the file path plus face index (TrueType collections) on success */
inline bool FindSystemFont(const char* familyName, ETextStyle style, WDL_String& path, int& faceIdx)
{
  const Fontconfig* pFC = Fontconfig::Get();
  if (!pFC || !familyName || !*familyName)
    return false;

  Fontconfig::Pattern* pPattern = pFC->PatternCreate();
  if (!pPattern)
    return false;

  pFC->PatternAddString(pPattern, "family", reinterpret_cast<const unsigned char*>(familyName));
  pFC->PatternAddInteger(pPattern, "weight", style == ETextStyle::Bold ? Fontconfig::kWeightBold : Fontconfig::kWeightRegular);
  pFC->PatternAddInteger(pPattern, "slant", style == ETextStyle::Italic ? Fontconfig::kSlantItalic : Fontconfig::kSlantRoman);
  pFC->ConfigSubstitute(nullptr, pPattern, Fontconfig::kMatchPattern);
  pFC->DefaultSubstitute(pPattern);

  int result = -1;
  Fontconfig::Pattern* pMatch = pFC->FontMatch(nullptr, pPattern, &result);
  pFC->PatternDestroy(pPattern);
  if (!pMatch)
    return false;

  bool found = false;
  unsigned char* pFile = nullptr;
  if (pFC->PatternGetString(pMatch, "file", 0, &pFile) == Fontconfig::kResultMatch && pFile)
  {
    int index = 0;
    if (pFC->PatternGetInteger(pMatch, "index", 0, &index) != Fontconfig::kResultMatch)
      index = 0;
    path.Set(reinterpret_cast<const char*>(pFile));
    faceIdx = index;
    found = true;
  }
  pFC->PatternDestroy(pMatch);
  return found;
}

} // namespace linux_fonts

END_IGRAPHICS_NAMESPACE
END_IPLUG_NAMESPACE
