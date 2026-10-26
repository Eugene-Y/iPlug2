/*
 ==============================================================================

 This file is part of the iPlug 2 library. Copyright (C) the iPlug 2 developers.

 See LICENSE.txt for  more info.

 ==============================================================================
*/

#pragma once

/** @file
 * @brief Touchscreen input for one editor connection through XInput 2.2: each finger is a touch with an ITouchID of
 * its own while it is down. libXi is dlopen'ed and never unloaded (the host may use it too).
 * Include after <X11/Xlib.h>. Main thread only.
 */

#include <dlfcn.h>
#include <algorithm>
#include <vector>

#include <X11/extensions/XInput2.h>

BEGIN_IPLUG_NAMESPACE
BEGIN_IGRAPHICS_NAMESPACE

class LinuxTouch
{
public:
  struct Event
  {
    ETouchEvent phase = ETouchEvent::Invalid;
    ITouchID id = 0;
    float x = 0.f; // physical pixels in the window
    float y = 0.f;
    float dX = 0.f; // since the touch's previous event
    float dY = 0.f;
    unsigned state = 0; // modifier keys, as in a core event's state
    Time time = CurrentTime;
  };

  /** Whether the server has XInput 2.2 and a touchscreen, asked once per connection. Asking announces XInput 2.2 on
   * this connection only; the host's own is untouched. */
  bool HasTouchscreen(Display* pDisplay)
  {
    if (!mChecked)
    {
      mChecked = true;
      mDisplay = pDisplay;
      mHasTouchscreen = QueryTouchscreen();
    }
    return mHasTouchscreen;
  }

  /** Once HasTouchscreen said yes. The window's touches then arrive as touch events, and the server no longer emulates
   * mouse events from them for this client. */
  void Select(Window window)
  {
    if (!mHasTouchscreen)
      return;
    unsigned char bits[XIMaskLen(XI_LASTEVENT)] = {};
    XISetMask(bits, XI_TouchBegin);
    XISetMask(bits, XI_TouchUpdate);
    XISetMask(bits, XI_TouchEnd);
    XIEventMask mask { XIAllMasterDevices, static_cast<int>(sizeof(bits)), bits };
    GetXi()->SelectEvents(mDisplay, window, &mask, 1);
    mSelected = true;
  }

  /** @return false for events that are not a touch of a selected window */
  bool Translate(const XEvent& event, Event& touch)
  {
    if (!mSelected || event.type != GenericEvent || event.xcookie.extension != mOpcode)
      return false;
    XGenericEventCookie cookie = event.xcookie;
    if (!XGetEventData(mDisplay, &cookie))
      return false;
    const bool isTouch = cookie.evtype == XI_TouchBegin || cookie.evtype == XI_TouchUpdate || cookie.evtype == XI_TouchEnd;
    const bool translated = isTouch && Translate(*static_cast<const XIDeviceEvent*>(cookie.data), touch);
    XFreeEventData(mDisplay, &cookie);
    return translated;
  }

  bool AnyDown() const { return !mTouches.empty(); }

private:
  struct Xi
  {
    decltype(&XIQueryVersion) QueryVersion = nullptr;
    decltype(&XIQueryDevice) QueryDevice = nullptr;
    decltype(&XIFreeDeviceInfo) FreeDeviceInfo = nullptr;
    decltype(&XISelectEvents) SelectEvents = nullptr;
  };

  /** @return nullptr without libXi */
  static const Xi* GetXi()
  {
    static const Xi sXi = []() {
      Xi xi;
      if (void* pLib = dlopen("libXi.so.6", RTLD_NOW | RTLD_LOCAL))
      {
        xi.QueryVersion = reinterpret_cast<decltype(xi.QueryVersion)>(dlsym(pLib, "XIQueryVersion"));
        xi.QueryDevice = reinterpret_cast<decltype(xi.QueryDevice)>(dlsym(pLib, "XIQueryDevice"));
        xi.FreeDeviceInfo = reinterpret_cast<decltype(xi.FreeDeviceInfo)>(dlsym(pLib, "XIFreeDeviceInfo"));
        xi.SelectEvents = reinterpret_cast<decltype(xi.SelectEvents)>(dlsym(pLib, "XISelectEvents"));
      }
      return xi;
    }();
    const bool complete = sXi.QueryVersion && sXi.QueryDevice && sXi.FreeDeviceInfo && sXi.SelectEvents;
    return complete ? &sXi : nullptr;
  }

  bool QueryTouchscreen()
  {
    const Xi* pXi = GetXi();
    int firstEvent = 0, firstError = 0;
    if (!pXi || !XQueryExtension(mDisplay, "XInputExtension", &mOpcode, &firstEvent, &firstError))
      return false;
    // The server answers with the lower of its version and ours, and sends touch events only to clients of 2.2 on.
    int major = 2, minor = 2;
    if (pXi->QueryVersion(mDisplay, &major, &minor) != Success || major < 2 || (major == 2 && minor < 2))
      return false;

    // A touchpad is a dependent touch device: its fingers belong to the pointer, which already brings them as mouse
    // events, not to the window under them. A touchscreen plugged in later is unknown here, and still welcome.
    bool found = false;
    int nDevices = 0;
    XIDeviceInfo* pDevices = pXi->QueryDevice(mDisplay, XIAllDevices, &nDevices);
    for (int d = 0; d < nDevices; d++)
    {
      const XIDeviceInfo& device = pDevices[d];
      for (int c = 0; c < device.num_classes; c++)
      {
        const XIAnyClassInfo* pClass = device.classes[c];
        if (device.use != XISlavePointer || pClass->type != XITouchClass)
          continue;
        if (reinterpret_cast<const XITouchClassInfo*>(pClass)->mode == XIDirectTouch)
          found = true;
        else
          mTouchpads.push_back(device.deviceid);
      }
    }
    if (pDevices)
      pXi->FreeDeviceInfo(pDevices);
    return found;
  }

  bool Translate(const XIDeviceEvent& xiEvent, Event& touch)
  {
    if (std::find(mTouchpads.begin(), mTouchpads.end(), xiEvent.sourceid) != mTouchpads.end())
      return false;
    // Touch ids are unique per device only; a counter also keeps 0, the mouse's id, out.
    auto isThisTouch = [&xiEvent](const Touch& t) { return t.sourceID == xiEvent.sourceid && t.detail == xiEvent.detail; };
    auto pTouch = std::find_if(mTouches.begin(), mTouches.end(), isThisTouch);
    const float x = static_cast<float>(xiEvent.event_x);
    const float y = static_cast<float>(xiEvent.event_y);
    if (xiEvent.evtype == XI_TouchBegin)
    {
      if (pTouch != mTouches.end())
        return false;
      mTouches.push_back({ xiEvent.sourceid, xiEvent.detail, ++mLastID, x, y });
      pTouch = std::prev(mTouches.end());
      touch.phase = ETouchEvent::Began;
    }
    else if (pTouch == mTouches.end())
      return false;
    else
      touch.phase = xiEvent.evtype == XI_TouchUpdate ? ETouchEvent::Moved : ETouchEvent::Ended;

    touch.id = pTouch->id;
    touch.x = x;
    touch.y = y;
    touch.dX = x - pTouch->x;
    touch.dY = y - pTouch->y;
    touch.state = static_cast<unsigned>(xiEvent.mods.effective);
    touch.time = xiEvent.time;
    if (touch.phase == ETouchEvent::Ended)
      mTouches.erase(pTouch);
    else
    {
      pTouch->x = x;
      pTouch->y = y;
    }
    return true;
  }

  struct Touch
  {
    int sourceID;
    int detail;
    ITouchID id;
    float x;
    float y;
  };

  Display* mDisplay = nullptr;
  bool mChecked = false;
  bool mHasTouchscreen = false;
  bool mSelected = false;
  int mOpcode = 0;
  ITouchID mLastID = 0;
  std::vector<int> mTouchpads; // device ids
  std::vector<Touch> mTouches;
};

END_IGRAPHICS_NAMESPACE
END_IPLUG_NAMESPACE
