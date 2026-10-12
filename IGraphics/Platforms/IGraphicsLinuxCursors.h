/*
 ==============================================================================

 This file is part of the iPlug 2 library. Copyright (C) the iPlug 2 developers.

 See LICENSE.txt for  more info.

 ==============================================================================
*/

#pragma once

/** @file
 * @brief Mouse cursors for one editor connection: the desktop's Xcursor theme (libXcursor is dlopen'ed),
 * else the core X cursor font, plus a blank cursor for hidden-cursor drags.
 * Include after <X11/Xlib.h> and <X11/cursorfont.h>. Main thread only.
 */

#include <dlfcn.h>

BEGIN_IPLUG_NAMESPACE
BEGIN_IGRAPHICS_NAMESPACE

class LinuxCursors
{
public:
  static constexpr int kNumCursors = static_cast<int>(ECursor::HELP) + 1;

  void Init(Display* pDisplay) { mDisplay = pDisplay; }

  /** Before XCloseDisplay */
  void Free()
  {
    for (Cursor& cursor : mCursors)
    {
      if (cursor)
        XFreeCursor(mDisplay, cursor);
      cursor = 0;
    }
    if (mBlank)
      XFreeCursor(mDisplay, mBlank);
    mBlank = 0;
    mDisplay = nullptr;
  }

  Cursor Get(ECursor type)
  {
    Cursor& cursor = mCursors[static_cast<int>(type)];
    if (!cursor)
      cursor = Load(type);
    return cursor;
  }

  Cursor GetBlank(Window window)
  {
    if (!mBlank)
    {
      const char bits[1] = {};
      Pixmap pixmap = XCreateBitmapFromData(mDisplay, window, bits, 1, 1);
      XColor black {};
      mBlank = XCreatePixmapCursor(mDisplay, pixmap, pixmap, &black, &black, 0, 0);
      XFreePixmap(mDisplay, pixmap);
    }
    return mBlank;
  }

private:
  struct Shape
  {
    const char* themeNames[3]; // CSS name first, then the names older themes use
    unsigned fontShape;
  };

  static Shape ShapeFor(ECursor type)
  {
    switch (type)
    {
      case ECursor::IBEAM: return { { "text", "xterm", nullptr }, XC_xterm };
      case ECursor::WAIT: return { { "wait", "watch", nullptr }, XC_watch };
      case ECursor::CROSS: return { { "crosshair", "cross", nullptr }, XC_crosshair };
      case ECursor::UPARROW: return { { "up-arrow", "sb_up_arrow", nullptr }, XC_sb_up_arrow };
      case ECursor::SIZENWSE: return { { "nwse-resize", "size_fdiag", "bd_double_arrow" }, XC_sizing };
      case ECursor::SIZENESW: return { { "nesw-resize", "size_bdiag", "fd_double_arrow" }, XC_sizing };
      case ECursor::SIZEWE: return { { "ew-resize", "sb_h_double_arrow", nullptr }, XC_sb_h_double_arrow };
      case ECursor::SIZENS: return { { "ns-resize", "sb_v_double_arrow", nullptr }, XC_sb_v_double_arrow };
      case ECursor::SIZEALL: return { { "move", "fleur", nullptr }, XC_fleur };
      case ECursor::INO: return { { "not-allowed", "crossed_circle", nullptr }, XC_X_cursor };
      case ECursor::HAND: return { { "pointer", "hand2", nullptr }, XC_hand2 };
      case ECursor::APPSTARTING: return { { "progress", "left_ptr_watch", nullptr }, XC_watch };
      case ECursor::HELP: return { { "help", "question_arrow", nullptr }, XC_question_arrow };
      case ECursor::ARROW:
      default: return { { "default", "left_ptr", nullptr }, XC_left_ptr };
    }
  }

  using LoadThemeCursorFunc = Cursor (*)(Display*, const char*);

  static LoadThemeCursorFunc LoadThemeCursor()
  {
    static LoadThemeCursorFunc sFunc = nullptr;
    static bool sTried = false;
    if (!sTried)
    {
      sTried = true;
      // Never unloaded: the host may use libXcursor too.
      if (void* pLib = dlopen("libXcursor.so.1", RTLD_NOW | RTLD_LOCAL))
        sFunc = reinterpret_cast<LoadThemeCursorFunc>(dlsym(pLib, "XcursorLibraryLoadCursor"));
    }
    return sFunc;
  }

  Cursor Load(ECursor type)
  {
    const Shape shape = ShapeFor(type);
    if (LoadThemeCursorFunc loadThemeCursor = LoadThemeCursor())
    {
      for (const char* pName : shape.themeNames)
      {
        if (!pName)
          break;
        if (Cursor cursor = loadThemeCursor(mDisplay, pName))
          return cursor;
      }
    }
    return XCreateFontCursor(mDisplay, shape.fontShape);
  }

  Display* mDisplay = nullptr;
  Cursor mCursors[kNumCursors] = {};
  Cursor mBlank = 0;
};

END_IGRAPHICS_NAMESPACE
END_IPLUG_NAMESPACE
