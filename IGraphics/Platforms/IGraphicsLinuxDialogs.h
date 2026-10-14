/*
 ==============================================================================

 This file is part of the iPlug 2 library. Copyright (C) the iPlug 2 developers.

 See LICENSE.txt for  more info.

 ==============================================================================
*/

#pragma once

/** @file
 * @brief File, folder, message and colour dialogs on Linux, without GTK in the plug-in's process.
 *
 * Files and folders: the xdg-desktop-portal FileChooser over D-Bus (libdbus loaded at run time), else zenity or
 * kdialog as a separate process. Message boxes and colours: zenity or kdialog (the portal has neither).
 * Nothing blocks: every dialog completes later, from the plug-in's HostLoop, on the host's main thread.
 */

#include <functional>
#include <memory>

#include "IGraphicsConstants.h"
#include "IGraphicsStructs.h"
#include "ptrlist.h"
#include "wdlstring.h"

BEGIN_IPLUG_NAMESPACE
class HostLoop;
BEGIN_IGRAPHICS_NAMESPACE

/** What to ask. Strings are UTF-8. */
struct LinuxDialogRequest
{
  enum class EKind { OpenFile, SaveFile, Directory, Message, Color };

  EKind kind = EKind::OpenFile;
  WDL_String title;
  WDL_String text;
  /** Where a file or folder dialog starts; empty: the dialog's own choice (the portal) or the home folder */
  WDL_String folder;
  /** The name a save dialog proposes */
  WDL_String fileName;
  /** As IGraphics::PromptForFile takes them: "fxp", "wav aif", ".wav .aif" */
  WDL_String extensions;
  EMsgBoxType messageType = kMB_OK;
  IColor color;
  /** The X11 top-level window the dialog belongs to; 0 if there is none */
  unsigned long parentWindow = 0;
};

struct LinuxDialogResult
{
  bool accepted = false;
  /** The file chosen, or the folder with a trailing '/' */
  WDL_String path;
  EMsgBoxResult message = kCANCEL;
  IColor color;
};

/** The dialogs of one editor. Several may be open at once; each one's handler runs exactly once, unless the
 * dialogs are dismissed first (editor closed): then the dialogs go away and no handler runs. */
class LinuxDialogs final
{
public:
  using Handler = std::function<void(const LinuxDialogResult& result)>;

  /** @param loop the plug-in instance's loop: handlers run from its callbacks. It must outlive this object. */
  explicit LinuxDialogs(HostLoop& loop);
  /** Dismisses every open dialog; no handler runs */
  ~LinuxDialogs();
  LinuxDialogs(const LinuxDialogs&) = delete;
  LinuxDialogs& operator=(const LinuxDialogs&) = delete;

  /** Opens the dialog and returns at once. The handler is called later from the loop, never from inside Show(),
   * also when no dialog can be shown at all (then as cancelled). It may close the editor and so destroy this. */
  void Show(const LinuxDialogRequest& request, Handler handler);

  /** Closes every open dialog without calling its handler */
  void DismissAll();

  struct Job;

private:
  void Poll();
  void UpdateWatches();

  HostLoop& mLoop;
  WDL_PtrList<Job> mJobs;
  int mTimerId = 0;
  // Shared with Poll() on the stack: a handler may destroy this object.
  std::shared_ptr<bool> mAlive;
};

END_IGRAPHICS_NAMESPACE
END_IPLUG_NAMESPACE
