/*
 ==============================================================================

 This file is part of the iPlug 2 library. Copyright (C) the iPlug 2 developers.

 See LICENSE.txt for  more info.

 ==============================================================================
*/

#include <cstdio>

#include "IGraphicsLinux.h"
#include "Linux/IPlugLinuxOpen.h"

using namespace iplug;
using namespace igraphics;

IGraphicsLinux::IGraphicsLinux(IGEditorDelegate& dlg, int w, int h, int fps, float scale)
: IGRAPHICS_DRAW_CLASS(dlg, w, h, fps, scale)
{
}

IGraphicsLinux::~IGraphicsLinux() = default;

void* IGraphicsLinux::OpenWindow(void* pParent)
{
  static bool sWarned = false;
  if (!sWarned)
  {
    sWarned = true;
    fprintf(stderr, "iPlug2: IGraphicsLinux is a stub, the editor has no window yet\n");
  }
  return nullptr;
}

EMsgBoxResult IGraphicsLinux::ShowMessageBox(const char* str, const char* title, EMsgBoxType type, IMsgBoxCompletionHandlerFunc completionHandler)
{
  const EMsgBoxResult result = EMsgBoxResult::kCANCEL;
  if (completionHandler)
    completionHandler(result);
  return result;
}

void IGraphicsLinux::PromptForFile(WDL_String& fileName, WDL_String& path, EFileAction action, const char* ext, IFileDialogCompletionHandlerFunc completionHandler)
{
  fileName.Set("");
}

void IGraphicsLinux::PromptForDirectory(WDL_String& dir, IFileDialogCompletionHandlerFunc completionHandler)
{
  dir.Set("");
}

bool IGraphicsLinux::OpenURL(const char* url, const char* msgWindowTitle, const char* confirmMsg, const char* errMsgOnFailure)
{
  return LinuxOpenWithDefaultApp(url);
}

bool IGraphicsLinux::RevealPathInExplorerOrFinder(WDL_String& path, bool select)
{
  WDL_String dir(path.Get());
  if (select)
    dir.remove_filepart();
  return LinuxOpenWithDefaultApp(dir.Get());
}

#if defined IGRAPHICS_NANOVG
  #include "IGraphicsNanoVG.cpp"
#elif defined IGRAPHICS_SKIA
  #include "IGraphicsSkia.cpp"
#else
  #error Either NO_IGRAPHICS or one and only one choice of graphics library must be defined!
#endif
