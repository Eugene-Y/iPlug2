/*
 ==============================================================================

 This file is part of the iPlug 2 library. Copyright (C) the iPlug 2 developers.

 See LICENSE.txt for  more info.

 ==============================================================================
*/

#pragma once

#include <memory>

#include "IGraphics_select.h"

union _XEvent;

BEGIN_IPLUG_NAMESPACE
BEGIN_IGRAPHICS_NAMESPACE

/** IGraphics platform class for Linux: an X11 child window of the host's window, drawn with GLX.
 * Host-driven: X events and frames are pumped by the plug-in instance's HostLoop (host main thread);
 * the editor owns no thread.
 * @ingroup PlatformClasses */
class IGraphicsLinux final : public IGRAPHICS_DRAW_CLASS
{
public:
  IGraphicsLinux(IGEditorDelegate& dlg, int w, int h, int fps, float scale);
  ~IGraphicsLinux();

  void* OpenWindow(void* pParent) override;
  void CloseWindow() override;
  void* GetWindow() override;
  bool WindowIsOpen() override;
  void PlatformResize(bool parentHasResized) override;
  float GetPlatformWindowScale() const override { return GetScreenScale(); }
  void DrawNow() override;

  const char* GetPlatformAPIStr() override { return "X11"; }

  void HideMouseCursor(bool hide, bool lock) override {}
  void MoveMouseCursor(float x, float y) override {}
  void GetMouseLocation(float& x, float& y) const override;

  void ForceEndUserEdit() override {}
  void UpdateTooltips() override {}

  EMsgBoxResult ShowMessageBox(const char* str, const char* title, EMsgBoxType type, IMsgBoxCompletionHandlerFunc completionHandler) override;
  void PromptForFile(WDL_String& fileName, WDL_String& path, EFileAction action, const char* ext, IFileDialogCompletionHandlerFunc completionHandler) override;
  void PromptForDirectory(WDL_String& dir, IFileDialogCompletionHandlerFunc completionHandler) override;
  bool PromptForColor(IColor& color, const char* str, IColorPickerHandlerFunc func) override { return false; }

  bool OpenURL(const char* url, const char* msgWindowTitle, const char* confirmMsg, const char* errMsgOnFailure) override;
  bool RevealPathInExplorerOrFinder(WDL_String& path, bool select) override;

  bool GetTextFromClipboard(WDL_String& str) override { str.Set(""); return false; }
  bool SetTextInClipboard(const char* str) override { return false; }

protected:
  IPopupMenu* CreatePlatformPopupMenu(IPopupMenu& menu, const IRECT bounds, bool& isAsync) override { return nullptr; }
  void CreatePlatformTextEntry(int paramIdx, const IText& text, const IRECT& bounds, int length, const char* str) override {}

  void ActivateGLContext() override;
  void DeactivateGLContext() override;

private:
  PlatformFontPtr LoadPlatformFont(const char* fontID, const char* fileNameOrResID) override;
  PlatformFontPtr LoadPlatformFont(const char* fontID, const char* fontName, ETextStyle style) override { return nullptr; }
  PlatformFontPtr LoadPlatformFont(const char* fontID, void* pData, int dataSize) override;
  void CachePlatformFont(const char* fontID, const PlatformFontPtr& font) override {}

  void OnDisplayTimer();
  void OnXConnectionReadable(int flags);
  /** @return false when the window closed while handling an event: the caller must return without touching this. */
  bool ProcessXEvents();
  void HandleXEvent(const _XEvent& event);
  void HandleButton(const _XEvent& event);
  void HandleMotion(const _XEvent& event);
  void HandleConfigure(const _XEvent& event);
  void DrawDirtyControls();
  void DestroyWindowResources();

  struct Impl;
  std::unique_ptr<Impl> mImpl;
};

END_IGRAPHICS_NAMESPACE
END_IPLUG_NAMESPACE
