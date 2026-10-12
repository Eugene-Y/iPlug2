/*
 ==============================================================================

 This file is part of the iPlug 2 library. Copyright (C) the iPlug 2 developers.

 See LICENSE.txt for  more info.

 ==============================================================================
*/

#pragma once

/** @file
 * @brief The X CLIPBOARD selection for one editor window: owning it, serving it, and reading it with a bounded wait.
 * Include after <X11/Xlib.h> and <X11/Xatom.h>. Main thread only, like the rest of IGraphicsLinux.
 */

#include <poll.h>
#include <chrono>
#include <cstring>

#include "wdlstring.h"

BEGIN_IPLUG_NAMESPACE
BEGIN_IGRAPHICS_NAMESPACE

class LinuxClipboard
{
public:
  /** The window must select PropertyChangeMask: an INCR transfer arrives as property changes */
  void Init(Display* pDisplay, Window window)
  {
    mDisplay = pDisplay;
    mWindow = window;
    mClipboard = XInternAtom(pDisplay, "CLIPBOARD", False);
    mUTF8String = XInternAtom(pDisplay, "UTF8_STRING", False);
    mTargets = XInternAtom(pDisplay, "TARGETS", False);
    mText = XInternAtom(pDisplay, "TEXT", False);
    mIncr = XInternAtom(pDisplay, "INCR", False);
    mProperty = XInternAtom(pDisplay, "IPLUG_CLIPBOARD", False);
  }

  /** Before the window goes: the selection dies with it, and its id may come back for another window */
  void Release()
  {
    if (mWindow && SharedOwner() == mWindow)
      SharedOwner() = 0;
    mDisplay = nullptr;
    mWindow = 0;
  }

  bool SetText(const char* str, Time time)
  {
    if (!mDisplay || !str)
      return false;
    SharedText().Set(str);
    XSetSelectionOwner(mDisplay, mClipboard, mWindow, time);
    const bool owned = XGetSelectionOwner(mDisplay, mClipboard) == mWindow;
    SharedOwner() = owned ? mWindow : 0;
    return owned;
  }

  /** Reads the clipboard as UTF-8, waiting at most timeoutMs for the owner. The host's main thread is blocked
   * meanwhile, so an owner that is served by this very thread (the host's own widgets) cannot answer in time. */
  bool GetText(WDL_String& str, int timeoutMs)
  {
    str.Set("");
    if (!mDisplay)
      return false;

    const Window owner = XGetSelectionOwner(mDisplay, mClipboard);
    if (owner == None)
      return false;
    // Any editor of this process: other editors have connections of their own, and this thread would
    // have to serve their SelectionRequest while it waits here.
    if (owner == SharedOwner())
    {
      str.Set(SharedText().Get());
      return true;
    }

    XDeleteProperty(mDisplay, mWindow, mProperty);
    XConvertSelection(mDisplay, mClipboard, mUTF8String, mProperty, mWindow, CurrentTime);
    XFlush(mDisplay);

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    XEvent event;
    if (!WaitForEvent(deadline, [&]() { return XCheckTypedWindowEvent(mDisplay, mWindow, SelectionNotify, &event)
                                               && event.xselection.selection == mClipboard; }))
      return false;
    if (event.xselection.property == None)
      return false;

    Atom type = None;
    WDL_TypedBuf<char> data;
    if (!ReadProperty(type, data))
      return false;
    if (type == mIncr && !ReadIncremental(deadline, data))
      return false;

    str.Set(data.Get(), data.GetSize());
    return true;
  }

  /** @return true when the event belonged to the clipboard */
  bool HandleEvent(const XEvent& event)
  {
    if (event.type == SelectionRequest && event.xselectionrequest.selection == mClipboard)
    {
      Serve(event.xselectionrequest);
      return true;
    }
    if (event.type == SelectionClear && event.xselectionclear.selection == mClipboard)
    {
      if (SharedOwner() == mWindow)
        SharedOwner() = 0;
      return true;
    }
    return false;
  }

private:
  static WDL_String& SharedText() { static WDL_String sText; return sText; }
  static Window& SharedOwner() { static Window sOwner = 0; return sOwner; }

  template <typename Predicate>
  bool WaitForEvent(std::chrono::steady_clock::time_point deadline, Predicate found)
  {
    for (;;)
    {
      if (found())
        return true;
      const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
      if (remaining <= 0)
        return false;
      pollfd fd { ConnectionNumber(mDisplay), POLLIN, 0 };
      poll(&fd, 1, static_cast<int>(remaining));
    }
  }

  /** Reads and deletes our property. Deleting it is what asks an INCR sender for the next chunk. */
  bool ReadProperty(Atom& type, WDL_TypedBuf<char>& data)
  {
    int format = 0;
    unsigned long count = 0;
    unsigned long bytesAfter = 0;
    unsigned char* pValue = nullptr;
    const long maxLongs = 0x1000000;
    if (XGetWindowProperty(mDisplay, mWindow, mProperty, 0, maxLongs, True, AnyPropertyType,
                           &type, &format, &count, &bytesAfter, &pValue) != Success)
      return false;

    bool ok = bytesAfter == 0;
    if (ok && type != mIncr && pValue && count > 0)
    {
      const int oldSize = data.GetSize();
      char* pData = format == 8 ? data.ResizeOK(oldSize + static_cast<int>(count)) : nullptr;
      if (pData)
        memcpy(pData + oldSize, pValue, count);
      else
        ok = false;
    }
    if (pValue)
      XFree(pValue);
    return ok;
  }

  /** ICCCM INCR: each new value of the property is a chunk; a zero-length one ends the transfer */
  bool ReadIncremental(std::chrono::steady_clock::time_point deadline, WDL_TypedBuf<char>& data)
  {
    data.Resize(0);
    for (;;)
    {
      XEvent event;
      if (!WaitForEvent(deadline, [&]() { return XCheckTypedWindowEvent(mDisplay, mWindow, PropertyNotify, &event)
                                                 && event.xproperty.atom == mProperty
                                                 && event.xproperty.state == PropertyNewValue; }))
        return false;
      const int before = data.GetSize();
      Atom type = None;
      if (!ReadProperty(type, data))
        return false;
      if (data.GetSize() == before)
        return true;
    }
  }

  void Serve(const XSelectionRequestEvent& request)
  {
    const Atom property = request.property != None ? request.property : request.target; // obsolete requestors
    const WDL_String& text = SharedText();
    bool served = false;

    if (SharedOwner() == mWindow)
    {
      if (request.target == mTargets)
      {
        const Atom targets[] = { mTargets, mUTF8String, mText, XA_STRING };
        XChangeProperty(mDisplay, request.requestor, property, XA_ATOM, 32, PropModeReplace,
                        reinterpret_cast<const unsigned char*>(targets), static_cast<int>(sizeof(targets) / sizeof(targets[0])));
        served = true;
      }
      else if ((request.target == mUTF8String || request.target == mText
                || (request.target == XA_STRING && IsAscii(text.Get())))
               && text.GetLength() < MaxDirectBytes())
      {
        // TEXT lets the owner pick the encoding; the reply's type says which one it got.
        const Atom type = request.target == XA_STRING ? XA_STRING : mUTF8String;
        XChangeProperty(mDisplay, request.requestor, property, type, 8, PropModeReplace,
                        reinterpret_cast<const unsigned char*>(text.Get()), text.GetLength());
        served = true;
      }
    }

    XEvent reply {};
    reply.xselection.type = SelectionNotify;
    reply.xselection.display = mDisplay;
    reply.xselection.requestor = request.requestor;
    reply.xselection.selection = request.selection;
    reply.xselection.target = request.target;
    reply.xselection.property = served ? property : None;
    reply.xselection.time = request.time;
    XSendEvent(mDisplay, request.requestor, False, NoEventMask, &reply);
    XFlush(mDisplay);
  }

  /** Larger texts would need INCR on the serving side; they are refused instead. */
  int MaxDirectBytes() const
  {
    long maxRequest = XExtendedMaxRequestSize(mDisplay);
    if (maxRequest <= 0)
      maxRequest = XMaxRequestSize(mDisplay);
    return static_cast<int>(std::min<long>(maxRequest * 4 - 256, 0x7fffffff));
  }

  static bool IsAscii(const char* str)
  {
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(str); *p; ++p)
    {
      if (*p >= 0x80)
        return false;
    }
    return true;
  }

  Display* mDisplay = nullptr;
  Window mWindow = 0;
  Atom mClipboard = None;
  Atom mUTF8String = None;
  Atom mTargets = None;
  Atom mText = None;
  Atom mIncr = None;
  Atom mProperty = None;
};

END_IGRAPHICS_NAMESPACE
END_IPLUG_NAMESPACE
