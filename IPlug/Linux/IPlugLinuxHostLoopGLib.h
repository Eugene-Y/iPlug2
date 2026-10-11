/*
 ==============================================================================

 This file is part of the iPlug 2 library. Copyright (C) the iPlug 2 developers.

 See LICENSE.txt for  more info.

 ==============================================================================
*/

#pragma once

/** @file
 * @brief HostLoop backend on the default GLib main context, for the standalone APP only: a plug-in must
 * not assume the host runs GLib. SWELL's message loop iterates that context (SWELL_RunEvents), so the
 * sources fire on the APP's main thread. */

#include <glib.h>
#include <glib-unix.h>

#include "IPlugLinuxHostLoop.h"

BEGIN_IPLUG_NAMESPACE

class GLibHostLoopBackend final : public HostLoopBackend
{
public:
  explicit GLibHostLoopBackend(HostLoop& loop) : HostLoopBackend(loop) {}

  const char* GetName() const override { return "glib-main-context"; }
  bool IsHostDriven() const override { return true; }

  bool Register(HostLoopEntry& entry) override
  {
    guint sourceId;
    if (entry.mIsFd)
    {
      const GIOCondition condition = (GIOCondition) (((entry.mFdFlags & HostLoop::kFdRead) ? G_IO_IN : 0)
                                                   | ((entry.mFdFlags & HostLoop::kFdWrite) ? G_IO_OUT : 0)
                                                   | G_IO_ERR | G_IO_HUP);
      sourceId = g_unix_fd_add(entry.mFd, condition, OnFd, this);
    }
    else
      sourceId = g_timeout_add(entry.mIntervalMs, OnTimeout, this);

    if (!sourceId)
      return false;

    // The callbacks find their entry by source id (g_main_current_source), so user_data can be the backend.
    entry.mHandle = sourceId;
    return true;
  }

  void Unregister(HostLoopEntry& entry) override
  {
    g_source_remove((guint) entry.mHandle);
  }

private:
  static gboolean OnTimeout(gpointer pData)
  {
    auto* pBackend = static_cast<GLibHostLoopBackend*>(pData);
    pBackend->mLoop.Dispatch(false, g_source_get_id(g_main_current_source()));
    return G_SOURCE_CONTINUE;
  }

  static gboolean OnFd(gint fd, GIOCondition condition, gpointer pData)
  {
    auto* pBackend = static_cast<GLibHostLoopBackend*>(pData);
    const int flags = ((condition & G_IO_IN) ? HostLoop::kFdRead : 0) | ((condition & G_IO_OUT) ? HostLoop::kFdWrite : 0)
                    | ((condition & (G_IO_ERR | G_IO_HUP)) ? HostLoop::kFdError : 0);
    pBackend->mLoop.Dispatch(true, g_source_get_id(g_main_current_source()), flags);
    return G_SOURCE_CONTINUE;
  }
};

END_IPLUG_NAMESPACE
