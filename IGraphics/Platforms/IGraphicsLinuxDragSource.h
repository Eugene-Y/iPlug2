/*
 ==============================================================================

 This file is part of the iPlug 2 library. Copyright (C) the iPlug 2 developers.

 See LICENSE.txt for  more info.

 ==============================================================================
*/

#pragma once

/** @file
 * @brief Dragging a file out of the editor: the XDND source side (protocol version 5), offering text/uri-list.
 * Include after <X11/Xlib.h> and IGraphicsLinuxClipboard.h. Main thread only; nothing blocks. The drag runs on the
 * mouse button's implicit grab: the press that started it keeps sending motion and the release to this window.
 */

#include <algorithm>
#include <chrono>

#include <X11/Xatom.h>
#include <X11/keysym.h>

#include "wdlstring.h"

BEGIN_IPLUG_NAMESPACE
BEGIN_IGRAPHICS_NAMESPACE

class LinuxDragSource
{
public:
  /** @param pointerEventMask the pointer events the window selects: a cursor change re-states the grab's mask */
  void Init(Display* pDisplay, Window window, LinuxCursors* pCursors, long pointerEventMask)
  {
    mDisplay = pDisplay;
    mWindow = window;
    mCursors = pCursors;
    mPointerEventMask = pointerEventMask;
    mXdndAware = XInternAtom(pDisplay, "XdndAware", False);
    mXdndProxy = XInternAtom(pDisplay, "XdndProxy", False);
    mXdndSelection = XInternAtom(pDisplay, "XdndSelection", False);
    mXdndEnter = XInternAtom(pDisplay, "XdndEnter", False);
    mXdndPosition = XInternAtom(pDisplay, "XdndPosition", False);
    mXdndStatus = XInternAtom(pDisplay, "XdndStatus", False);
    mXdndLeave = XInternAtom(pDisplay, "XdndLeave", False);
    mXdndDrop = XInternAtom(pDisplay, "XdndDrop", False);
    mXdndFinished = XInternAtom(pDisplay, "XdndFinished", False);
    mXdndActionCopy = XInternAtom(pDisplay, "XdndActionCopy", False);
    mUriList = XInternAtom(pDisplay, "text/uri-list", False);
    mTargets = XInternAtom(pDisplay, "TARGETS", False);
  }

  /** Before the window goes: tell a target we were over that the drag is over */
  void Release()
  {
    if (mDisplay && mState == EState::Dragging)
      Cancel();
    mState = EState::Idle;
    mDisplay = nullptr;
  }

  bool IsActive() const { return mState != EState::Idle; }
  /** The button is still down: pointer and keys belong to the drag */
  bool IsDragging() const { return mState == EState::Dragging; }

  /** @return false for a relative path, without a mouse button held (the drag lives on its implicit grab) or
   * without the XdndSelection */
  bool Start(const char* path, Time time)
  {
    if (!mDisplay || !path || path[0] != '/' || mState != EState::Idle)
      return false;
    Window root = 0, child = 0;
    int rootX = 0, rootY = 0, x = 0, y = 0;
    unsigned int mask = 0;
    XQueryPointer(mDisplay, mWindow, &root, &child, &rootX, &rootY, &x, &y, &mask);
    mButton = (mask & Button1Mask) ? Button1 : (mask & Button2Mask) ? Button2 : (mask & Button3Mask) ? Button3 : 0;
    if (!mButton)
      return false;
    mUri.Set("");
    LinuxClipboard::AppendFileUri(mUri, path);
    mUri.Append("\r\n");
    XSetSelectionOwner(mDisplay, mXdndSelection, mWindow, time);
    if (XGetSelectionOwner(mDisplay, mXdndSelection) != mWindow)
      return false;
    mState = EState::Dragging;
    mTarget = mTargetProxy = 0;
    mAccepted = mWaitingForStatus = mPositionPending = false;
    // Like toolkit drag sources: Escape must reach the drag, not the host window that has the focus.
    mKeyboardGrabbed = XGrabKeyboard(mDisplay, mWindow, False, GrabModeAsync, GrabModeAsync, time) == GrabSuccess;
    SetCursor(ECursor::INO);
    return true;
  }

  /** When IsDragging() turns false here the button is up: the caller ends its own mouse capture. After a drop only
   * the target's messages and requests are ours until it finishes.
   * @return true when the event belonged to the drag */
  bool HandleEvent(const XEvent& event)
  {
    if (event.type == SelectionRequest)
      return event.xselectionrequest.selection == mXdndSelection && Serve(event.xselectionrequest);
    if (event.type == ClientMessage)
      return mState != EState::Idle && HandleClientMessage(event.xclient);
    if (mState != EState::Dragging)
      return false;

    switch (event.type)
    {
      case MotionNotify:
        Move(event.xmotion.x_root, event.xmotion.y_root, event.xmotion.time);
        return true;
      case ButtonPress:
      case KeyRelease:
        return true;
      case ButtonRelease:
        // Wheel steps arrive as press/release pairs of buttons 4-7 while the drag button is still down.
        if (event.xbutton.button == mButton)
          Drop(event.xbutton.time);
        return true;
      case KeyPress:
        if (XLookupKeysym(const_cast<XKeyEvent*>(&event.xkey), 0) == XK_Escape)
          Cancel();
        return true;
      default:
        return false;
    }
  }

  /** A target that never answers a drop: give up after a while */
  void Idle()
  {
    constexpr auto kFinishTimeout = std::chrono::seconds(3);
    if (mState == EState::Dropped && std::chrono::steady_clock::now() - mDroppedAt > kFinishTimeout)
      mState = EState::Idle;
  }

private:
  enum class EState { Idle, Dragging, Dropped };
  static constexpr int kVersion = 5;

  bool HandleClientMessage(const XClientMessageEvent& message)
  {
    if (message.window != mWindow || static_cast<Window>(message.data.l[0]) != mTarget || !mTarget)
      return false;
    if (message.message_type == mXdndStatus)
    {
      mAccepted = message.data.l[1] & 1;
      mWaitingForStatus = false;
      if (mState == EState::Dragging)
        SetCursor(mAccepted ? ECursor::HAND : ECursor::INO);
      if (mPositionPending && mState == EState::Dragging)
        SendPosition();
      return true;
    }
    if (message.message_type == mXdndFinished)
    {
      if (mState == EState::Dropped)
        mState = EState::Idle;
      return true;
    }
    return false;
  }

  void Move(int rootX, int rootY, Time time)
  {
    mRootX = rootX;
    mRootY = rootY;
    mTime = time;
    Window proxy = 0;
    int version = 0;
    const Window target = FindTarget(rootX, rootY, proxy, version);
    if (target != mTarget)
    {
      if (mTarget)
        Send(mXdndLeave, 0, 0, 0, 0);
      mTarget = target;
      mTargetProxy = proxy;
      mAccepted = mWaitingForStatus = false;
      SetCursor(ECursor::INO);
      if (mTarget)
        Send(mXdndEnter, std::min(version, kVersion) << 24, static_cast<long>(mUriList), None, None);
    }
    if (!mTarget)
      return;
    // The spec asks for one Position per Status, so a slow target is not flooded.
    mPositionPending = true;
    if (!mWaitingForStatus)
      SendPosition();
  }

  void SendPosition()
  {
    Send(mXdndPosition, 0, (static_cast<long>(mRootX) << 16) | (mRootY & 0xffff), static_cast<long>(mTime), static_cast<long>(mXdndActionCopy));
    mPositionPending = false;
    mWaitingForStatus = true;
  }

  /** The drag's feedback: whether the window under the pointer takes the file */
  void SetCursor(ECursor type)
  {
    const Cursor cursor = mCursors ? mCursors->Get(type) : None;
    XChangeActivePointerGrab(mDisplay, static_cast<unsigned int>(mPointerEventMask), cursor, CurrentTime);
  }

  void EndDragging()
  {
    XChangeActivePointerGrab(mDisplay, static_cast<unsigned int>(mPointerEventMask), None, CurrentTime);
    if (mKeyboardGrabbed)
      XUngrabKeyboard(mDisplay, CurrentTime);
    mKeyboardGrabbed = false;
  }

  void Drop(Time time)
  {
    EndDragging();
    if (mTarget && mAccepted)
    {
      Send(mXdndDrop, 0, static_cast<long>(time), 0, 0);
      mState = EState::Dropped;
      mDroppedAt = std::chrono::steady_clock::now();
    }
    else
      Cancel();
  }

  void Cancel()
  {
    EndDragging();
    if (mTarget)
      Send(mXdndLeave, 0, 0, 0, 0);
    mTarget = 0;
    mState = EState::Idle;
  }

  /** The outermost XdndAware window under the pointer (toolkits mark their top-level; a host that embeds us
   * forwards to its children itself), following XdndProxy. 0 if there is none. */
  Window FindTarget(int rootX, int rootY, Window& proxy, int& version)
  {
    Window current = DefaultRootWindow(mDisplay);
    for (int depth = 0; depth < 32; ++depth)
    {
      Window child = 0;
      int x = 0, y = 0;
      if (!XTranslateCoordinates(mDisplay, DefaultRootWindow(mDisplay), current, rootX, rootY, &x, &y, &child) || !child)
        return 0;
      current = child;
      proxy = ReadWindowProperty(current, mXdndProxy);
      // A proxy must point to itself, or it is stale (XDND spec).
      if (proxy && ReadWindowProperty(proxy, mXdndProxy) != proxy)
        proxy = 0;
      const Window awareWindow = proxy ? proxy : current;
      Atom type = None;
      int format = 0;
      unsigned long count = 0, bytesAfter = 0;
      unsigned char* pValue = nullptr;
      if (XGetWindowProperty(mDisplay, awareWindow, mXdndAware, 0, 1, False, XA_ATOM, &type, &format, &count, &bytesAfter, &pValue) == Success
          && pValue && count == 1)
      {
        version = static_cast<int>(*reinterpret_cast<Atom*>(pValue));
        XFree(pValue);
        if (version >= 3)
          return current;
      }
      else if (pValue)
        XFree(pValue);
    }
    return 0;
  }

  Window ReadWindowProperty(Window window, Atom property)
  {
    Atom type = None;
    int format = 0;
    unsigned long count = 0, bytesAfter = 0;
    unsigned char* pValue = nullptr;
    Window result = 0;
    if (XGetWindowProperty(mDisplay, window, property, 0, 1, False, XA_WINDOW, &type, &format, &count, &bytesAfter, &pValue) == Success
        && pValue && count == 1)
      result = *reinterpret_cast<Window*>(pValue);
    if (pValue)
      XFree(pValue);
    return result;
  }

  void Send(Atom type, long l1, long l2, long l3, long l4)
  {
    XEvent event {};
    event.xclient.type = ClientMessage;
    event.xclient.display = mDisplay;
    event.xclient.window = mTarget;
    event.xclient.message_type = type;
    event.xclient.format = 32;
    event.xclient.data.l[0] = static_cast<long>(mWindow);
    event.xclient.data.l[1] = l1;
    event.xclient.data.l[2] = l2;
    event.xclient.data.l[3] = l3;
    event.xclient.data.l[4] = l4;
    XSendEvent(mDisplay, mTargetProxy ? mTargetProxy : mTarget, False, NoEventMask, &event);
    XFlush(mDisplay);
  }

  bool Serve(const XSelectionRequestEvent& request)
  {
    const Atom property = request.property != None ? request.property : request.target;
    bool served = false;
    if (request.target == mTargets)
    {
      const Atom targets[] = { mTargets, mUriList };
      XChangeProperty(mDisplay, request.requestor, property, XA_ATOM, 32, PropModeReplace,
                      reinterpret_cast<const unsigned char*>(targets), 2);
      served = true;
    }
    else if (request.target == mUriList && mUri.GetLength())
    {
      XChangeProperty(mDisplay, request.requestor, property, mUriList, 8, PropModeReplace,
                      reinterpret_cast<const unsigned char*>(mUri.Get()), mUri.GetLength());
      served = true;
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
    return true;
  }

  Display* mDisplay = nullptr;
  Window mWindow = 0;
  EState mState = EState::Idle;
  WDL_String mUri;
  Window mTarget = 0;
  Window mTargetProxy = 0;
  bool mAccepted = false;
  bool mWaitingForStatus = false;
  bool mPositionPending = false;
  bool mKeyboardGrabbed = false;
  unsigned int mButton = 0;
  LinuxCursors* mCursors = nullptr;
  long mPointerEventMask = 0;
  int mRootX = 0, mRootY = 0;
  Time mTime = CurrentTime;
  std::chrono::steady_clock::time_point mDroppedAt;
  Atom mXdndAware = None, mXdndProxy = None, mXdndSelection = None;
  Atom mXdndEnter = None, mXdndPosition = None, mXdndStatus = None, mXdndLeave = None, mXdndDrop = None, mXdndFinished = None;
  Atom mXdndActionCopy = None, mUriList = None, mTargets = None;
};

END_IGRAPHICS_NAMESPACE
END_IPLUG_NAMESPACE
