/*
 ==============================================================================

 This file is part of the iPlug 2 library. Copyright (C) the iPlug 2 developers.

 See LICENSE.txt for  more info.

 ==============================================================================
*/

#include "IPlugAPPLinux.h"

#include <cctype>
#include <csignal>
#include <cstring>
#include <filesystem>
#include <vector>

#include <gdk/gdkx.h>
#include <glib-unix.h>

#include "IPlugPaths.h"
#include "swell-internal.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../../Dependencies/IGraphics/STB/stb_image_write.h"

extern HWND gHWND;

BEGIN_IPLUG_NAMESPACE
namespace linux_app {

void KeepSwellSettingsInConfigFolder(const char* appName)
{
  WDL_String path;
  INIPath(path, appName);
  if (!path.GetLength())
    return;

  std::error_code error;
  std::filesystem::create_directories(path.Get(), error);
  if (error)
    return;

  path.Append("/libSwell.ini");
  SWELL_ExtendedAPI("INIFILE", (void*) path.Get());
}

// Like a WM close, which SWELL ignores while a modal dialog has the main window disabled: menus and
// dialogs close first, since their procs still use the host that closing the main window destroys.
static gboolean CloseInnermostWindow(gpointer)
{
  DestroyPopupMenus();

  if (HWND modal = DialogBoxIsActive())
  {
    EndDialog(modal, IDCANCEL);
    return G_SOURCE_CONTINUE;
  }

  if (gHWND)
    PostMessage(gHWND, WM_CLOSE, 0, 0);

  return G_SOURCE_REMOVE;
}

// Removing the source restores the default handler, so a second signal still ends a hung quit.
static gboolean CloseOnSignal(gpointer)
{
  g_timeout_add(20, CloseInnermostWindow, nullptr);
  return G_SOURCE_REMOVE;
}

void CloseMainWindowOnSignals()
{
  g_unix_signal_add(SIGINT, CloseOnSignal, nullptr);
  g_unix_signal_add(SIGTERM, CloseOnSignal, nullptr);
}

struct MenuShortcut
{
  KeyCode keyCode;
  unsigned int modifiers; // ControlMask | ShiftMask | Mod1Mask
  int command;
};

static std::vector<MenuShortcut> sMenuShortcuts;

static bool ParseShortcut(Display* pDisplay, const char* pText, MenuShortcut& shortcut)
{
  struct Modifier { const char* name; unsigned int mask; };
  static const Modifier kModifiers[] = { {"Ctrl+", ControlMask}, {"Shift+", ShiftMask}, {"Alt+", Mod1Mask} };

  shortcut.modifiers = 0;
  for (bool found = true; found;)
  {
    found = false;
    for (const Modifier& modifier : kModifiers)
    {
      const size_t length = strlen(modifier.name);
      if (!strncmp(pText, modifier.name, length))
      {
        shortcut.modifiers |= modifier.mask;
        pText += length;
        found = true;
      }
    }
  }

  const bool singleCharacterKey = pText[0] && !pText[1];
  if (!shortcut.modifiers || !singleCharacterKey)
    return false;

  // Latin-1 keysyms are the characters themselves
  const KeySym keySym = tolower(static_cast<unsigned char>(pText[0]));
  shortcut.keyCode = XKeysymToKeycode(pDisplay, keySym);
  return shortcut.keyCode != 0;
}

static void CollectMenuShortcuts(Display* pDisplay, HMENU menu)
{
  for (int i = 0; i < GetMenuItemCount(menu); i++)
  {
    char text[256] = {};
    MENUITEMINFO info = { sizeof(info), MIIM_TYPE | MIIM_ID | MIIM_SUBMENU };
    info.dwTypeData = text;
    info.cch = sizeof(text);
    if (!GetMenuItemInfo(menu, i, TRUE, &info))
      continue;

    MenuShortcut shortcut;
    const char* pTab = strchr(text, '\t');
    if (info.hSubMenu)
      CollectMenuShortcuts(pDisplay, info.hSubMenu);
    else if (pTab && ParseShortcut(pDisplay, pTab + 1, shortcut))
    {
      shortcut.command = static_cast<int>(info.wID);
      sMenuShortcuts.push_back(shortcut);
    }
  }
}

static constexpr unsigned int kShortcutModifiers = ControlMask | ShiftMask | Mod1Mask;

// The grabs deliver the keys to GDK's connection as core events, which GDK (using XInput2) does not translate
static GdkFilterReturn RunGrabbedShortcut(GdkXEvent* pGdkXEvent, GdkEvent*, gpointer)
{
  const XEvent& event = *static_cast<XEvent*>(pGdkXEvent);
  if (event.type != KeyPress && event.type != KeyRelease)
    return GDK_FILTER_CONTINUE;

  for (const MenuShortcut& shortcut : sMenuShortcuts)
  {
    if (event.xkey.keycode == shortcut.keyCode && (event.xkey.state & kShortcutModifiers) == shortcut.modifiers)
    {
      if (event.type == KeyPress && gHWND && IsWindowEnabled(gHWND))
        PostMessage(gHWND, WM_COMMAND, shortcut.command, 0);
      return GDK_FILTER_REMOVE;
    }
  }
  return GDK_FILTER_CONTINUE;
}

void SetMenuShortcuts(HWND hwnd, HMENU menu)
{
  GdkWindow* pGdkWindow = hwnd ? static_cast<GdkWindow*>(SWELL_GetOSWindow(hwnd, "GdkWindow")) : nullptr;
  if (!pGdkWindow || !menu)
    return;

  Display* pDisplay = GDK_WINDOW_XDISPLAY(pGdkWindow);
  sMenuShortcuts.clear();
  CollectMenuShortcuts(pDisplay, menu);

  // A passive grab on the main window takes the key also while the focus is in a child, the plug-in UI's own X
  // window, which passes unused keys on with XSendEvent: GDK ignores those. Caps Lock and Num Lock must not matter.
  GdkDisplay* pGdkDisplay = gdk_window_get_display(pGdkWindow);
  gdk_x11_display_error_trap_push(pGdkDisplay);
  for (const MenuShortcut& shortcut : sMenuShortcuts)
  {
    for (unsigned int locks : {0u, static_cast<unsigned int>(LockMask), static_cast<unsigned int>(Mod2Mask), static_cast<unsigned int>(LockMask | Mod2Mask)})
      XGrabKey(pDisplay, shortcut.keyCode, shortcut.modifiers | locks, GDK_WINDOW_XID(pGdkWindow), True, GrabModeAsync, GrabModeAsync);
  }
  gdk_x11_display_error_trap_pop_ignored(pGdkDisplay);

  static bool sFilterAdded = false;
  if (!sFilterAdded)
    gdk_window_add_filter(nullptr, RunGrabbedShortcut, nullptr);
  sFilterAdded = true;
}

static unsigned char ColorChannel(unsigned long pixel, unsigned long mask)
{
  if (!mask)
    return 0;

  int shift = 0;
  while (!((mask >> shift) & 1))
    shift++;
  const unsigned long maximum = mask >> shift;
  return static_cast<unsigned char>(((pixel & mask) >> shift) * 255 / maximum);
}

// The plug-in UI is its own X window inside SWELL's, which also holds the menu bar
static Window LargestChildWindow(Display* pDisplay, Window parent)
{
  Window root, parentOfParent, largest = None;
  Window* pChildren = nullptr;
  unsigned int count = 0, largestArea = 0;
  if (!XQueryTree(pDisplay, parent, &root, &parentOfParent, &pChildren, &count))
    return None;

  for (unsigned int i = 0; i < count; i++)
  {
    XWindowAttributes attributes;
    if (XGetWindowAttributes(pDisplay, pChildren[i], &attributes) && attributes.map_state == IsViewable
        && static_cast<unsigned int>(attributes.width * attributes.height) > largestArea)
    {
      largestArea = attributes.width * attributes.height;
      largest = pChildren[i];
    }
  }
  if (pChildren)
    XFree(pChildren);
  return largest;
}

bool SaveWindowScreenshot(HWND hwnd, const char* path)
{
  GdkWindow* pGdkWindow = hwnd && path ? static_cast<GdkWindow*>(SWELL_GetOSWindow(hwnd, "GdkWindow")) : nullptr;
  if (!pGdkWindow)
    return false;

  Display* pDisplay = GDK_WINDOW_XDISPLAY(pGdkWindow);
  Window window = LargestChildWindow(pDisplay, GDK_WINDOW_XID(pGdkWindow));
  if (window == None)
    return false;

  XWindowAttributes attributes;
  GdkDisplay* pGdkDisplay = gdk_window_get_display(pGdkWindow);
  gdk_x11_display_error_trap_push(pGdkDisplay);
  XImage* pImage = XGetWindowAttributes(pDisplay, window, &attributes)
                 ? XGetImage(pDisplay, window, 0, 0, attributes.width, attributes.height, AllPlanes, ZPixmap)
                 : nullptr;
  gdk_x11_display_error_trap_pop_ignored(pGdkDisplay);
  if (!pImage)
    return false;

  const int width = pImage->width;
  const int height = pImage->height;
  std::vector<unsigned char> rgba(static_cast<size_t>(width) * height * 4);
  for (int y = 0; y < height; y++)
  {
    for (int x = 0; x < width; x++)
    {
      const unsigned long pixel = XGetPixel(pImage, x, y);
      unsigned char* pOut = &rgba[(static_cast<size_t>(y) * width + x) * 4];
      pOut[0] = ColorChannel(pixel, pImage->red_mask);
      pOut[1] = ColorChannel(pixel, pImage->green_mask);
      pOut[2] = ColorChannel(pixel, pImage->blue_mask);
      pOut[3] = 255;
    }
  }
  XDestroyImage(pImage);

  return stbi_write_png(path, width, height, 4, rgba.data(), width * 4) != 0;
}

} // namespace linux_app
END_IPLUG_NAMESPACE
