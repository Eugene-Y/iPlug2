/*
 ==============================================================================

 This file is part of the iPlug 2 library. Copyright (C) the iPlug 2 developers.

 See LICENSE.txt for  more info.

 ==============================================================================
*/

#pragma once

#include <functional>
#include <memory>

#include "IGraphics_select.h"

union _XEvent;

BEGIN_IPLUG_NAMESPACE
BEGIN_IGRAPHICS_NAMESPACE

struct LinuxDialogRequest;
struct LinuxDialogResult;

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

  void HideMouseCursor(bool hide, bool lock) override;
  void MoveMouseCursor(float x, float y) override;
  ECursor SetMouseCursor(ECursor cursorType) override;
  void GetMouseLocation(float& x, float& y) const override;

  void ForceEndUserEdit() override;
  // No tooltips on Linux yet: X has no native ones, and an IGraphics-drawn one is left for later.
  void UpdateTooltips() override {}

  EMsgBoxResult ShowMessageBox(const char* str, const char* title, EMsgBoxType type, IMsgBoxCompletionHandlerFunc completionHandler) override;
  void PromptForFile(WDL_String& fileName, WDL_String& path, EFileAction action, const char* ext, IFileDialogCompletionHandlerFunc completionHandler) override;
  void PromptForDirectory(WDL_String& dir, IFileDialogCompletionHandlerFunc completionHandler) override;
  bool PromptForColor(IColor& color, const char* str, IColorPickerHandlerFunc func) override;

  bool OpenURL(const char* url, const char* msgWindowTitle, const char* confirmMsg, const char* errMsgOnFailure) override;
  bool RevealPathInExplorerOrFinder(WDL_String& path, bool select) override;

  bool GetTextFromClipboard(WDL_String& str) override;
  bool SetTextInClipboard(const char* str) override;

protected:
  IPopupMenu* CreatePlatformPopupMenu(IPopupMenu& menu, const IRECT bounds, bool& isAsync) override;
  void CreatePlatformTextEntry(int paramIdx, const IText& text, const IRECT& bounds, int length, const char* str) override;

  void ActivateGLContext() override;
  void DeactivateGLContext() override;

private:
  PlatformFontPtr LoadPlatformFont(const char* fontID, const char* fileNameOrResID) override;
  PlatformFontPtr LoadPlatformFont(const char* fontID, const char* fontName, ETextStyle style) override;
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
  void HandleKey(const _XEvent& event);
  void HandleFocusOut(const _XEvent& event);
  void HandleClientMessage(const _XEvent& event);
  void ForwardKeyToHost(const _XEvent& event);
  /** Keyboard focus and stand-in controls follow the editing state after every event and frame */
  void UpdateEditingState();
  void TakeKeyboardFocus();
  void ReleaseKeyboardFocus();
  void EndTextEntryOnFocusLoss();
  void RemoveFinishedStandIns();
  bool IsEditing();
  bool IsMenuOpen();
  void DismissOpenMenu();
  void DrawDirtyControls();
  /** @return false when the host closed the editor meanwhile */
  bool TellHostTheOpenedSize();
  bool IsHostResizeEnabled();
  unsigned long GetDialogParentWindow();
  /** @return false when the editor is not open: then no dialog shows and the handler never runs */
  bool ShowDialog(LinuxDialogRequest& request, std::function<void(const LinuxDialogResult&)> handler);
  void DestroyWindowResources();

  struct Impl;
  std::unique_ptr<Impl> mImpl;
};

END_IGRAPHICS_NAMESPACE
END_IPLUG_NAMESPACE
