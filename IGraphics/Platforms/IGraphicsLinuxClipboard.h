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
#include <algorithm>
#include <chrono>
#include <cstring>

#include "wdlstring.h"
#include "ptrlist.h"

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
    mUriList = XInternAtom(pDisplay, "text/uri-list", False);
    mGnomeCopiedFiles = XInternAtom(pDisplay, "x-special/gnome-copied-files", False);
    mIncr = XInternAtom(pDisplay, "INCR", False);
    mProperty = XInternAtom(pDisplay, "IPLUG_CLIPBOARD", False);
    if (OpenEditors().Find(this) < 0)
      OpenEditors().Add(this);
  }

  /** Before the window goes: the selection would die with it. Another open editor of this process takes it over;
   * without one the text stays readable here as "orphaned" until something else owns the clipboard. */
  void Release()
  {
    OpenEditors().DeletePtr(this);
    if (mWindow && SharedOwner() == mWindow)
    {
      SharedOwner() = 0;
      LinuxClipboard* pHeir = OpenEditors().Get(0);
      if (!pHeir || !pHeir->TakeOver())
        SharedOrphaned() = true;
    }
    mDisplay = nullptr;
    mWindow = 0;
  }

  bool SetText(const char* str, Time time)
  {
    if (!mDisplay || !str)
      return false;
    SharedText().Set(str);
    SharedFileUri().Set("");
    SharedOrphaned() = false;
    XSetSelectionOwner(mDisplay, mClipboard, mWindow, time);
    const bool owned = XGetSelectionOwner(mDisplay, mClipboard) == mWindow;
    SharedOwner() = owned ? mWindow : 0;
    return owned;
  }

  /** Offers an absolute path as a file to paste in file managers (text/uri-list, GNOME's copied-files list) and as
   * plain text, like the file on the mac/win clipboard */
  bool SetFilePath(const char* path, Time time)
  {
    if (!path || path[0] != '/')
      return false;
    WDL_String uri;
    AppendFileUri(uri, path);
    if (!SetText(path, time))
      return false;
    SharedFileUri().Set(uri.Get());
    return true;
  }

  /** file:// URI of an absolute path: every byte outside RFC 3986's unreserved set and '/' percent-encoded */
  static void AppendFileUri(WDL_String& uri, const char* path)
  {
    uri.Append("file://");
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(path); *p; ++p)
    {
      const bool unreserved = (*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9');
      if (unreserved || strchr("-._~/", *p))
        uri.Append(reinterpret_cast<const char*>(p), 1);
      else
        uri.AppendFormatted(4, "%%%02X", *p);
    }
  }

  /** Reads the clipboard as UTF-8, waiting at most timeoutMs for the owner. The host's main thread is blocked
   * meanwhile, so an owner that is served by this very thread (the host's own widgets) cannot answer in time. */
  bool GetText(WDL_String& str, int timeoutMs)
  {
    str.Set("");
    if (!mDisplay)
      return false;
    if (mHasAsyncText)
    {
      str.Set(mAsyncText.Get());
      mHasAsyncText = false;
      return true;
    }

    const Window owner = XGetSelectionOwner(mDisplay, mClipboard);
    if (owner == None)
    {
      if (!SharedOrphaned())
        return false;
      str.Set(SharedText().Get());
      return true;
    }
    if (owner != SharedOwner())
      SharedOrphaned() = false;
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
    if (!WaitForEvent(deadline, event, &IsOurSelectionNotify))
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

  /** Starts reading the clipboard without blocking: for an owner that only the host's main thread can serve
   * (the host's own widgets), which GetText's wait would starve. HandleEvent receives the text; once
   * AsyncReadDone() says so, the next GetText returns it.
   * @return false when there is nothing to wait for: no owner, or an editor of this process (GetText reads that at once) */
  bool BeginAsyncRead()
  {
    if (!mDisplay || mAsyncState != kAsyncIdle)
      return false;
    const Window owner = XGetSelectionOwner(mDisplay, mClipboard);
    if (owner == None || owner == SharedOwner())
      return false;
    XDeleteProperty(mDisplay, mWindow, mProperty);
    XConvertSelection(mDisplay, mClipboard, mUTF8String, mProperty, mWindow, CurrentTime);
    XFlush(mDisplay);
    mAsyncState = kAsyncWaitingForOwner;
    mAsyncStart = std::chrono::steady_clock::now();
    mAsyncData.Resize(0);
    mHasAsyncText = false;
    return true;
  }

  /** @return true once, when an asynchronous read ended (with or without text) or gave up after a second */
  bool AsyncReadDone()
  {
    if (mAsyncState == kAsyncIdle)
      return false;
    if (mAsyncState == kAsyncDone)
    {
      mAsyncState = kAsyncIdle;
      return true;
    }
    if (std::chrono::steady_clock::now() - mAsyncStart > std::chrono::seconds(1))
    {
      mAsyncState = kAsyncIdle;
      return true;
    }
    return false;
  }

  bool HasAsyncText() const { return mHasAsyncText; }

  /** Forgets text that an asynchronous read delivered and nobody asked for */
  void DropAsyncText() { mHasAsyncText = false; }

  /** @return true when the event belonged to the clipboard */
  bool HandleEvent(const XEvent& event)
  {
    if (event.type == SelectionNotify && event.xselection.selection == mClipboard && mAsyncState == kAsyncWaitingForOwner)
    {
      Atom type = None;
      if (event.xselection.property == None || !ReadProperty(type, mAsyncData))
        FinishAsyncRead(false);
      else if (type == mIncr)
        mAsyncState = kAsyncIncremental;
      else
        FinishAsyncRead(true);
      return true;
    }
    if (event.type == PropertyNotify && mAsyncState == kAsyncIncremental && event.xproperty.window == mWindow
        && event.xproperty.atom == mProperty && event.xproperty.state == PropertyNewValue)
    {
      const int before = mAsyncData.GetSize();
      Atom type = None;
      if (!ReadProperty(type, mAsyncData))
        FinishAsyncRead(false);
      else if (type != None && mAsyncData.GetSize() == before)
        FinishAsyncRead(true);
      return true;
    }
    if (event.type == SelectionRequest && event.xselectionrequest.selection == mClipboard)
    {
      Serve(event.xselectionrequest);
      return true;
    }
    if (event.type == SelectionClear && event.xselectionclear.selection == mClipboard)
    {
      if (SharedOwner() == mWindow)
      {
        SharedOwner() = 0;
        SharedOrphaned() = false;
      }
      return true;
    }
    return false;
  }

private:
  enum EAsyncState { kAsyncIdle, kAsyncWaitingForOwner, kAsyncIncremental, kAsyncDone };

  void FinishAsyncRead(bool ok)
  {
    mHasAsyncText = ok;
    const bool hasData = ok && mAsyncData.GetSize() > 0;
    mAsyncText.Set(hasData ? mAsyncData.Get() : "", hasData ? mAsyncData.GetSize() : 0);
    mAsyncData.Resize(0);
    mAsyncState = kAsyncDone;
  }

  static WDL_String& SharedText() { static WDL_String sText; return sText; }
  /** The file URI when the clipboard holds a file (SetFilePath), empty for plain text */
  static WDL_String& SharedFileUri() { static WDL_String sUri; return sUri; }
  static Window& SharedOwner() { static Window sOwner = 0; return sOwner; }
  static bool& SharedOrphaned() { static bool sOrphaned = false; return sOrphaned; }
  static WDL_PtrList<LinuxClipboard>& OpenEditors() { static WDL_PtrList<LinuxClipboard> sEditors; return sEditors; }

  bool TakeOver()
  {
    XSetSelectionOwner(mDisplay, mClipboard, mWindow, CurrentTime);
    const bool owned = XGetSelectionOwner(mDisplay, mClipboard) == mWindow;
    if (owned)
      SharedOwner() = mWindow;
    return owned;
  }

  using EventPredicate = Bool (*)(Display*, XEvent*, XPointer);

  static Bool IsOurSelectionNotify(Display*, XEvent* pEvent, XPointer pThis)
  {
    const auto* pClipboard = reinterpret_cast<const LinuxClipboard*>(pThis);
    return pEvent->type == SelectionNotify && pEvent->xselection.requestor == pClipboard->mWindow
        && pEvent->xselection.selection == pClipboard->mClipboard;
  }

  static Bool IsNewValueOfOurProperty(Display*, XEvent* pEvent, XPointer pThis)
  {
    const auto* pClipboard = reinterpret_cast<const LinuxClipboard*>(pThis);
    return pEvent->type == PropertyNotify && pEvent->xproperty.window == pClipboard->mWindow
        && pEvent->xproperty.atom == pClipboard->mProperty && pEvent->xproperty.state == PropertyNewValue;
  }

  /** Takes the first matching event out of the queue, leaving all others for the editor's event loop.
   * Polls the connection only when none is queued yet. */
  bool WaitForEvent(std::chrono::steady_clock::time_point deadline, XEvent& event, EventPredicate predicate)
  {
    for (;;)
    {
      if (XCheckIfEvent(mDisplay, &event, predicate, reinterpret_cast<XPointer>(this)))
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

  /** ICCCM INCR: each new value of the property is a chunk; a zero-length one ends the transfer. The owner's
   * write of the INCR property itself also left a NewValue event, which finds the property deleted (type None). */
  bool ReadIncremental(std::chrono::steady_clock::time_point deadline, WDL_TypedBuf<char>& data)
  {
    data.Resize(0);
    for (;;)
    {
      XEvent event;
      if (!WaitForEvent(deadline, event, &IsNewValueOfOurProperty))
        return false;
      const int before = data.GetSize();
      Atom type = None;
      if (!ReadProperty(type, data))
        return false;
      if (type != None && data.GetSize() == before)
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
        const Atom targets[] = { mTargets, mUTF8String, mText, XA_STRING, mUriList, mGnomeCopiedFiles };
        const int nTargets = static_cast<int>(sizeof(targets) / sizeof(targets[0])) - (SharedFileUri().GetLength() ? 0 : 2);
        XChangeProperty(mDisplay, request.requestor, property, XA_ATOM, 32, PropModeReplace,
                        reinterpret_cast<const unsigned char*>(targets), nTargets);
        served = true;
      }
      else if ((request.target == mUriList || request.target == mGnomeCopiedFiles) && SharedFileUri().GetLength())
      {
        WDL_String list(request.target == mGnomeCopiedFiles ? "copy\n" : "");
        list.Append(SharedFileUri().Get());
        if (request.target == mUriList)
          list.Append("\r\n");
        XChangeProperty(mDisplay, request.requestor, property, request.target, 8, PropModeReplace,
                        reinterpret_cast<const unsigned char*>(list.Get()), list.GetLength());
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
  Atom mUriList = None;
  Atom mGnomeCopiedFiles = None;
  Atom mIncr = None;
  Atom mProperty = None;

  EAsyncState mAsyncState = kAsyncIdle;
  std::chrono::steady_clock::time_point mAsyncStart;
  WDL_TypedBuf<char> mAsyncData;
  WDL_String mAsyncText;
  bool mHasAsyncText = false;
};

END_IGRAPHICS_NAMESPACE
END_IPLUG_NAMESPACE
