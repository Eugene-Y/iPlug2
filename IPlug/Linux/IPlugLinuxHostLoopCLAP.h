/*
 ==============================================================================

 This file is part of the iPlug 2 library. Copyright (C) the iPlug 2 developers.

 See LICENSE.txt for  more info.

 ==============================================================================
*/

#pragma once

/** @file
 * @brief HostLoop backend on CLAP timer-support and posix-fd-support. Include from IPlugCLAP.cpp only.
 * The host extensions may be queried from clap_plugin.init() on, not from the plug-in constructor. */

#include "clap/ext/posix-fd-support.h"
#include "clap/ext/timer-support.h"
#include "clap/host.h"

#include "IPlugLinuxHostLoop.h"

BEGIN_IPLUG_NAMESPACE

static_assert(HostLoop::kFdRead == CLAP_POSIX_FD_READ && HostLoop::kFdWrite == CLAP_POSIX_FD_WRITE
              && HostLoop::kFdError == CLAP_POSIX_FD_ERROR, "HostLoop fd flags are passed to CLAP unconverted");

class CLAPHostLoopBackend final : public HostLoopBackend
{
public:
  /** The CLAP backend if the host has timer-support, otherwise the fallback thread. */
  static std::unique_ptr<HostLoopBackend> Make(HostLoop& loop, const clap_host* pHost)
  {
    auto* pTimers = static_cast<const clap_host_timer_support*>(pHost->get_extension(pHost, CLAP_EXT_TIMER_SUPPORT));
    auto* pFds = static_cast<const clap_host_posix_fd_support*>(pHost->get_extension(pHost, CLAP_EXT_POSIX_FD_SUPPORT));
#ifdef IPLUG_TEST_CLAP_NO_TIMER_SUPPORT // exercises the fallback in hosts that have timer-support
    HostLoopTrace("%s: CLAP timer-support forced off (IPLUG_TEST_CLAP_NO_TIMER_SUPPORT)", loop.GetOwnerName());
    pTimers = nullptr;
#endif
    const bool hasTimers = pTimers && pTimers->register_timer && pTimers->unregister_timer;
    const bool hasFds = pFds && pFds->register_fd && pFds->unregister_fd;

    HostLoopTrace("%s: CLAP host '%s' timer-support=%d posix-fd-support=%d", loop.GetOwnerName(), pHost->name, hasTimers, hasFds);

    if (!hasTimers)
      return MakeFallbackHostLoopBackend(loop);

    return std::make_unique<CLAPHostLoopBackend>(loop, pHost, pTimers, hasFds ? pFds : nullptr);
  }

  CLAPHostLoopBackend(HostLoop& loop, const clap_host* pHost, const clap_host_timer_support* pTimers, const clap_host_posix_fd_support* pFds)
  : HostLoopBackend(loop), mHost(pHost), mTimers(pTimers), mFds(pFds)
  {}

  const char* GetName() const override { return "clap-timer-support"; }
  bool IsHostDriven() const override { return true; }

  bool Register(HostLoopEntry& entry) override
  {
    if (entry.mIsFd)
    {
      if (!mFds || !mFds->register_fd(mHost, entry.mFd, (clap_posix_fd_flags_t) entry.mFdFlags))
        return false;
      entry.mHandle = (uintptr_t) entry.mFd;
      return true;
    }

    clap_id timerId = CLAP_INVALID_ID;
    if (!mTimers->register_timer(mHost, entry.mIntervalMs, &timerId) || timerId == CLAP_INVALID_ID)
      return false;
    entry.mHandle = (uintptr_t) timerId;
    return true;
  }

  void Unregister(HostLoopEntry& entry) override
  {
    if (entry.mIsFd)
      mFds->unregister_fd(mHost, (int) entry.mHandle);
    else
      mTimers->unregister_timer(mHost, (clap_id) entry.mHandle);
  }

private:
  const clap_host* mHost;
  const clap_host_timer_support* mTimers;
  const clap_host_posix_fd_support* mFds;
};

/** Replaces clap_plugin.destroy with one that stops the HostLoop first. The plug-in's own destructors run
 * before ~IPlugAPIBase, and a fallback thread would keep calling OnIdle on the half-destroyed object.
 * clap-helpers offers no virtual hook before its `delete`, hence the wrapped function pointer. */
template <class TPlugin, class THelper>
struct CLAPHostLoopDestroyHook
{
  static void Install(TPlugin& plugin)
  {
    auto* pClapPlugin = const_cast<clap_plugin*>(plugin.clapPlugin()); // a non-const member of the helper
    sHelperDestroy = pClapPlugin->destroy;
    pClapPlugin->destroy = Destroy;
  }

private:
  static void Destroy(const clap_plugin* pClapPlugin)
  {
    auto* pPlugin = static_cast<TPlugin*>(static_cast<THelper*>(pClapPlugin->plugin_data));
    HostLoopTrace("%s: clap_plugin.destroy, stopping the loop before delete", pPlugin->GetHostLoop().GetOwnerName());
    pPlugin->GetHostLoop().SetBackend(nullptr);
    sHelperDestroy(pClapPlugin);
  }

  static inline void (*sHelperDestroy)(const clap_plugin*) = nullptr;
};

END_IPLUG_NAMESPACE
