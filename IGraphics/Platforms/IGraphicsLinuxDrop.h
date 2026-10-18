/*
 ==============================================================================

 This file is part of the iPlug 2 library. Copyright (C) the iPlug 2 developers.

 See LICENSE.txt for  more info.

 ==============================================================================
*/

#pragma once

/** @file
 * @brief Files dropped onto one editor window: the XDND target side (protocol version 5), reading text/uri-list.
 * Include after <X11/Xlib.h> and <X11/Xatom.h>. Main thread only, like the rest of IGraphicsLinux.
 */

#include <chrono>
#include <cstdio>
#include <cstring>
#include <functional>
#include <vector>

#include "heapbuf.h"

#ifdef IGRAPHICS_TRACE_LINUX_INPUT
  #define TRACE_DROP(...) fprintf(stderr, "iPlug2 drop: " __VA_ARGS__)
#else
  #define TRACE_DROP(...) do {} while (0)
#endif

BEGIN_IPLUG_NAMESPACE
BEGIN_IGRAPHICS_NAMESPACE

/** GTK and Qt sources address the top-level window under the pointer, not the editor inside it. A host whose top-level
 * is XdndAware forwards the drag to an XdndAware child itself, as its own source (REAPER does, protocol version 3).
 * Nothing is set on the host's window: an XdndProxy there pointing at the editor is followed by GTK 4 sources for
 * the first position only, after which they talk to the top-level directly, and a drag over the host's own part of
 * the window then never finishes. Hosts that neither forward nor are XdndAware get no drops. */
class LinuxFileDrop
{
public:
  /** @param paths the dropped files' local paths; @param x, y where, in physical pixels of the window */
  using DropFunc = std::function<void(const std::vector<const char*>& paths, int x, int y)>;

  /** The window must select PropertyChangeMask: an INCR transfer arrives as property changes */
  void Init(Display* pDisplay, Window window)
  {
    mDisplay = pDisplay;
    mWindow = window;
    mXdndAware = XInternAtom(pDisplay, "XdndAware", False);
    mXdndEnter = XInternAtom(pDisplay, "XdndEnter", False);
    mXdndPosition = XInternAtom(pDisplay, "XdndPosition", False);
    mXdndStatus = XInternAtom(pDisplay, "XdndStatus", False);
    mXdndLeave = XInternAtom(pDisplay, "XdndLeave", False);
    mXdndDrop = XInternAtom(pDisplay, "XdndDrop", False);
    mXdndFinished = XInternAtom(pDisplay, "XdndFinished", False);
    mXdndSelection = XInternAtom(pDisplay, "XdndSelection", False);
    mXdndTypeList = XInternAtom(pDisplay, "XdndTypeList", False);
    mXdndActionCopy = XInternAtom(pDisplay, "XdndActionCopy", False);
    mUriList = XInternAtom(pDisplay, "text/uri-list", False);
    mIncr = XInternAtom(pDisplay, "INCR", False);
    mProperty = XInternAtom(pDisplay, "IPLUG_XDND", False);

    const long version = kVersion;
    XChangeProperty(pDisplay, window, mXdndAware, XA_ATOM, 32, PropModeReplace, reinterpret_cast<const unsigned char*>(&version), 1);
  }

  void Release()
  {
    mDisplay = nullptr;
    mWindow = 0;
    mDrag = Drag();
  }

  /** Gives up on a source that stopped answering, so that it is not waited for forever */
  void Idle()
  {
    constexpr auto kReadTimeout = std::chrono::seconds(2);
    if (mDrag.reading && std::chrono::steady_clock::now() - mDrag.readStart > kReadTimeout)
      FinishDrop(false);
  }

  /** @param width, height the window's physical size, for which positions are the editor's
   * @return true when the event belonged to a drag */
  bool HandleEvent(const XEvent& event, int width, int height, const DropFunc& onDrop)
  {
    if (!mDisplay)
      return false;
    if (event.type == ClientMessage && event.xclient.window != mWindow)
      return false;
    if (event.type == ClientMessage && event.xclient.format == 32)
    {
      const XClientMessageEvent& message = event.xclient;
      if (message.message_type == mXdndEnter)
        OnEnter(message);
      else if (message.message_type == mXdndPosition)
        OnPosition(message, width, height);
      else if (message.message_type == mXdndLeave)
        OnLeave(message);
      else if (message.message_type == mXdndDrop)
        OnDrop(message);
      else
        return false;
      return true;
    }
    if (event.type == SelectionNotify && event.xselection.requestor == mWindow && event.xselection.selection == mXdndSelection)
    {
      if (!mDrag.reading)
        return true;
      Atom type = None;
      if (event.xselection.property == None || !ReadProperty(type))
        FinishDrop(false);
      else if (type == mIncr)
        mDrag.incremental = true;
      else
        Deliver(onDrop);
      return true;
    }
    if (event.type == PropertyNotify && mDrag.reading && mDrag.incremental && event.xproperty.window == mWindow
        && event.xproperty.atom == mProperty && event.xproperty.state == PropertyNewValue)
    {
      const int before = mDrag.data.GetSize();
      Atom type = None;
      if (!ReadProperty(type))
        FinishDrop(false);
      else if (type != None && mDrag.data.GetSize() == before)
        Deliver(onDrop);
      return true;
    }
    return false;
  }

  /** Turns a text/uri-list into local paths, NUL-separated in @p paths: comments, other schemes and other hosts'
   * files are skipped, %XX escapes decoded. */
  static void ParseUriList(const char* pList, int length, WDL_TypedBuf<char>& paths, int& count)
  {
    paths.Resize(0);
    count = 0;
    const char* pEnd = pList + length;
    for (const char* pLine = pList; pLine < pEnd;)
    {
      const char* pLineEnd = pLine;
      while (pLineEnd < pEnd && *pLineEnd != '\r' && *pLineEnd != '\n' && *pLineEnd != '\0')
        pLineEnd++;
      AppendPath(pLine, pLineEnd, paths, count);
      pLine = pLineEnd;
      while (pLine < pEnd && (*pLine == '\r' || *pLine == '\n' || *pLine == '\0'))
        pLine++;
    }
  }

private:
  static constexpr long kVersion = 5;

  struct Drag
  {
    Window source = 0;
    long version = 0;
    bool offersFiles = false;
    bool overEditor = false;
    int x = 0;
    int y = 0;
    bool reading = false;
    bool incremental = false;
    std::chrono::steady_clock::time_point readStart;
    WDL_TypedBuf<char> data;
  };

  void OnEnter(const XClientMessageEvent& message)
  {
    TRACE_DROP("enter from 0x%lx, version %ld\n", message.data.l[0], (message.data.l[1] >> 24) & 0xff);
    if (mDrag.reading)
      FinishDrop(false);
    mDrag = Drag();
    mDrag.source = static_cast<Window>(message.data.l[0]);
    mDrag.version = (message.data.l[1] >> 24) & 0xff;
    const bool moreThanThreeTypes = message.data.l[1] & 1;
    if (moreThanThreeTypes)
      mDrag.offersFiles = SourceListsType(mDrag.source, mUriList);
    else
    {
      for (int i = 2; i < 5; i++)
        mDrag.offersFiles = mDrag.offersFiles || static_cast<Atom>(message.data.l[i]) == mUriList;
    }
  }

  void OnPosition(const XClientMessageEvent& message, int width, int height)
  {
    if (static_cast<Window>(message.data.l[0]) != mDrag.source)
      return;
    const int rootX = static_cast<int>((message.data.l[2] >> 16) & 0xffff);
    const int rootY = static_cast<int>(message.data.l[2] & 0xffff);
    int x = 0, y = 0;
    Window child = None;
    const Window root = RootWindow(mDisplay, DefaultScreen(mDisplay));
    const bool translated = XTranslateCoordinates(mDisplay, root, mWindow, rootX, rootY, &x, &y, &child);
    mDrag.overEditor = translated && x >= 0 && y >= 0 && x < width && y < height;
    mDrag.x = x;
    mDrag.y = y;
    SendStatus(mDrag.overEditor && mDrag.offersFiles);
  }

  void OnLeave(const XClientMessageEvent& message)
  {
    if (static_cast<Window>(message.data.l[0]) == mDrag.source)
      mDrag = Drag();
  }

  void OnDrop(const XClientMessageEvent& message)
  {
    TRACE_DROP("drop from 0x%lx at %d,%d, over editor %d, files %d\n", message.data.l[0], mDrag.x, mDrag.y, mDrag.overEditor, mDrag.offersFiles);
    if (static_cast<Window>(message.data.l[0]) != mDrag.source)
      return;
    if (!mDrag.overEditor || !mDrag.offersFiles)
    {
      SendFinished(false);
      mDrag = Drag();
      return;
    }
    // Version 0 sources send no time stamp
    const Time time = mDrag.version >= 1 ? static_cast<Time>(message.data.l[2]) : CurrentTime;
    XDeleteProperty(mDisplay, mWindow, mProperty);
    XConvertSelection(mDisplay, mXdndSelection, mUriList, mProperty, mWindow, time);
    XFlush(mDisplay);
    mDrag.reading = true;
    mDrag.readStart = std::chrono::steady_clock::now();
  }

  void Deliver(const DropFunc& onDrop)
  {
    WDL_TypedBuf<char> paths;
    int count = 0;
    ParseUriList(mDrag.data.Get(), mDrag.data.GetSize(), paths, count);
    const int x = mDrag.x;
    const int y = mDrag.y;
    // Finished before the plug-in sees the files: its handler may open a dialog, or close the editor.
    FinishDrop(count > 0);
    if (count == 0)
      return;
    std::vector<const char*> pathPtrs;
    for (const char* pPath = paths.Get(); pathPtrs.size() < static_cast<size_t>(count); pPath += strlen(pPath) + 1)
      pathPtrs.push_back(pPath);
    onDrop(pathPtrs, x, y);
  }

  void FinishDrop(bool accepted)
  {
    SendFinished(accepted);
    mDrag = Drag();
  }

  void SendStatus(bool accept)
  {
    XEvent event {};
    event.xclient.type = ClientMessage;
    event.xclient.display = mDisplay;
    event.xclient.window = mDrag.source;
    event.xclient.message_type = mXdndStatus;
    event.xclient.format = 32;
    event.xclient.data.l[0] = static_cast<long>(mWindow);
    // Bit 1 with the empty rectangle: every position is sent, the editor's controls are not one rectangle
    event.xclient.data.l[1] = (accept ? 1 : 0) | 2;
    event.xclient.data.l[4] = accept ? static_cast<long>(mXdndActionCopy) : 0;
    XSendEvent(mDisplay, mDrag.source, False, NoEventMask, &event);
    XFlush(mDisplay);
  }

  void SendFinished(bool accepted)
  {
    TRACE_DROP("finished %d to 0x%lx\n", accepted, mDrag.source);
    if (!mDrag.source)
      return;
    XEvent event {};
    event.xclient.type = ClientMessage;
    event.xclient.display = mDisplay;
    event.xclient.window = mDrag.source;
    event.xclient.message_type = mXdndFinished;
    event.xclient.format = 32;
    event.xclient.data.l[0] = static_cast<long>(mWindow);
    event.xclient.data.l[1] = accepted ? 1 : 0;
    event.xclient.data.l[2] = accepted ? static_cast<long>(mXdndActionCopy) : 0;
    XSendEvent(mDisplay, mDrag.source, False, NoEventMask, &event);
    XFlush(mDisplay);
  }

  bool SourceListsType(Window source, Atom wanted)
  {
    constexpr long kMaxTypes = 64;
    Atom type = None;
    int format = 0;
    unsigned long count = 0, remaining = 0;
    unsigned char* pData = nullptr;
    bool found = false;
    if (XGetWindowProperty(mDisplay, source, mXdndTypeList, 0, kMaxTypes, False, XA_ATOM, &type, &format, &count, &remaining, &pData) == Success && pData)
    {
      const long* pTypes = reinterpret_cast<long*>(pData);
      for (unsigned long i = 0; format == 32 && i < count && !found; i++)
        found = static_cast<Atom>(pTypes[i]) == wanted;
      XFree(pData);
    }
    return found;
  }

  /** Appends our property's value to the drag's data and deletes the property, which asks an INCR sender for more */
  bool ReadProperty(Atom& type)
  {
    int format = 0;
    unsigned long count = 0, bytesAfter = 0;
    unsigned char* pValue = nullptr;
    const long maxLongs = 0x1000000;
    if (XGetWindowProperty(mDisplay, mWindow, mProperty, 0, maxLongs, True, AnyPropertyType, &type, &format, &count, &bytesAfter, &pValue) != Success)
      return false;
    bool ok = bytesAfter == 0;
    if (ok && type != mIncr && pValue && count > 0)
    {
      const int oldSize = mDrag.data.GetSize();
      char* pData = format == 8 ? mDrag.data.ResizeOK(oldSize + static_cast<int>(count)) : nullptr;
      if (pData)
        memcpy(pData + oldSize, pValue, count);
      else
        ok = false;
    }
    if (pValue)
      XFree(pValue);
    return ok;
  }

  static int HexValue(char c)
  {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
  }

  static void AppendPath(const char* pLine, const char* pLineEnd, WDL_TypedBuf<char>& paths, int& count)
  {
    static constexpr char kScheme[] = "file://";
    const size_t schemeLength = sizeof(kScheme) - 1;
    if (pLine >= pLineEnd || *pLine == '#' || static_cast<size_t>(pLineEnd - pLine) <= schemeLength || strncmp(pLine, kScheme, schemeLength) != 0)
      return;
    // file://host/path: only this machine's files, named as "localhost" or by no host at all
    const char* pPath = pLine + schemeLength;
    if (*pPath != '/')
    {
      static constexpr char kLocalhost[] = "localhost";
      const size_t hostLength = sizeof(kLocalhost) - 1;
      const bool local = static_cast<size_t>(pLineEnd - pPath) > hostLength && strncmp(pPath, kLocalhost, hostLength) == 0
                      && pPath[hostLength] == '/';
      if (!local)
        return;
      pPath += hostLength;
    }

    const int start = paths.GetSize();
    for (const char* p = pPath; p < pLineEnd; p++)
    {
      char c = *p;
      if (c == '%' && pLineEnd - p > 2 && HexValue(p[1]) >= 0 && HexValue(p[2]) >= 0)
      {
        c = static_cast<char>(HexValue(p[1]) * 16 + HexValue(p[2]));
        p += 2;
      }
      if (c == '\0')
      {
        paths.Resize(start);
        return;
      }
      paths.Add(c);
    }
    paths.Add('\0');
    count++;
  }

  Display* mDisplay = nullptr;
  Window mWindow = 0;
  Drag mDrag;

  Atom mXdndAware = None;
  Atom mXdndEnter = None;
  Atom mXdndPosition = None;
  Atom mXdndStatus = None;
  Atom mXdndLeave = None;
  Atom mXdndDrop = None;
  Atom mXdndFinished = None;
  Atom mXdndSelection = None;
  Atom mXdndTypeList = None;
  Atom mXdndActionCopy = None;
  Atom mUriList = None;
  Atom mIncr = None;
  Atom mProperty = None;
};

END_IGRAPHICS_NAMESPACE
END_IPLUG_NAMESPACE
