/*
 ==============================================================================

 This file is part of the iPlug 2 library. Copyright (C) the iPlug 2 developers.

 See LICENSE.txt for  more info.

 ==============================================================================
*/

// The Linux WebViewEditorDelegate. Hosts size an X11 editor in pixels, while the page, Resize() and the editor
// size in config.h are in UI units, as on mac: the editor's pixel size is its UI size times the screen scale,
// which also zooms the page.

#include "IPlugWebViewEditorDelegate.h"

#include <cmath>

using namespace iplug;

WebViewEditorDelegate::WebViewEditorDelegate(int nParams)
: IEditorDelegate(nParams)
, IWebView()
{
}

WebViewEditorDelegate::~WebViewEditorDelegate()
{
  CloseWindow();
}

void* WebViewEditorDelegate::OpenWindow(void* pParent)
{
  if (!(mUIWidth > 0.f))
    SetUISize(GetEditorWidth() / mScreenScale, GetEditorHeight() / mScreenScale);
  SetWebViewHostLoop(GetEditorHostLoop());
  SetWebViewScreenScale(mScreenScale);
  mView = OpenWebView(pParent, 0.f, 0.f, mUIWidth, mUIHeight, 1.f);
  return mView;
}

void WebViewEditorDelegate::Resize(int width, int height)
{
  SetUISize(static_cast<float>(width), static_cast<float>(height));
  EditorResizeFromUI(static_cast<int>(std::lround(width * mScreenScale)), static_cast<int>(std::lround(height * mScreenScale)), true);
}

void WebViewEditorDelegate::OnParentWindowResize(int width, int height)
{
  SetUISize(width / mScreenScale, height / mScreenScale);
  EditorResizeFromUI(width, height, false);
}

void WebViewEditorDelegate::SetScreenScale(float scale)
{
  if (!(scale > 0.f))
    return;
  mHostScreenScale = scale;
  RescaleEditorSize(scale);
}

void WebViewEditorDelegate::UpdateEditorSizeForScreenScale()
{
  RescaleEditorSize(mHostScreenScale > 0.f ? mHostScreenScale : GetLinuxDesktopScale());
}

void WebViewEditorDelegate::RescaleEditorSize(float screenScale)
{
  if (!(mUIWidth > 0.f))
  {
    mUIWidth = GetEditorWidth() / mScreenScale;
    mUIHeight = GetEditorHeight() / mScreenScale;
  }
  mScreenScale = screenScale;
  SetEditorSize(static_cast<int>(std::lround(mUIWidth * screenScale)), static_cast<int>(std::lround(mUIHeight * screenScale)));
  SetWebViewScreenScale(screenScale);
}

void WebViewEditorDelegate::SetUISize(float width, float height)
{
  mUIWidth = width;
  mUIHeight = height;
  SetWebViewBounds(0.f, 0.f, width, height);
}

bool WebViewEditorDelegate::OnKeyDown(const IKeyPress& key)
{
  return false;
}

bool WebViewEditorDelegate::OnKeyUp(const IKeyPress& key)
{
  return false;
}
