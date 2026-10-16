/*
 ==============================================================================

 This file is part of the iPlug 2 library. Copyright (C) the iPlug 2 developers.

 See LICENSE.txt for  more info.

 ==============================================================================
*/

#pragma once

/** Linux parts of the standalone APP that SWELL and the shared APP code do not cover. */

#include "IPlugPlatform.h"
#include "IPlugSWELL.h"

BEGIN_IPLUG_NAMESPACE
namespace linux_app {

/** Keep SWELL's own settings (and an optional libSwell.colortheme) in the APP's XDG config folder instead of
 * ~/.libSwell.ini. Call first in main(), before SWELL reads them. */
void KeepSwellSettingsInConfigFolder(const char* appName);

/** SIGINT and SIGTERM close the APP's main window like the window manager's close button. */
void CloseMainWindowOnSignals();

/** Take the keyboard shortcuts from the menu's item texts, as written after the tab ("&Preferences...\tCtrl+,")
 * and run their commands, also while the plug-in UI has the keyboard focus, as on macOS and Windows. Only items in
 * the menu at the time of the call get one, so remove items first (the Debug items of a release build). */
void SetMenuShortcuts(HWND mainWindow, HMENU menu);

/** Save the plug-in UI (its own X window inside the main window, so without the menu bar) as a PNG. */
bool SaveWindowScreenshot(HWND hwnd, const char* path);

} // namespace linux_app
END_IPLUG_NAMESPACE
