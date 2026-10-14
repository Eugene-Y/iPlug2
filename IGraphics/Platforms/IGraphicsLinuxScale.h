/*
 ==============================================================================

 This file is part of the iPlug 2 library. Copyright (C) the iPlug 2 developers.

 See LICENSE.txt for  more info.

 ==============================================================================
*/

#pragma once

/** @file
 * @brief The desktop's UI scale on X11, for an editor that no host gave a scale to.
 * X11 has no per-window scale: sizes are physical pixels, and desktops publish their scale as Xft.dpi
 * (GNOME and KDE write 96 × scale into the X resources), or only through the toolkits' environment variables.
 */

#include <algorithm>
#include <cmath>

#include "IPlugPlatform.h"

BEGIN_IPLUG_NAMESPACE
BEGIN_IGRAPHICS_NAMESPACE

namespace linux_scale
{
constexpr double kReferenceDpi = 96.0;
// Below 1 comes only from a DPI measured off a small monitor; an editor smaller than designed helps no one.
constexpr double kMinScale = 1.0;
constexpr double kMaxScale = 4.0;

/** @return the number at the start of text ("144", "1.5"), read the same in every locale (a host may have set a
 * decimal comma); 0 when text does not start with one */
inline double ParseDecimal(const char* text)
{
  if (!text)
    return 0.0;

  double value = 0.0;
  bool hasDigits = false;
  const char* p = text;
  for (; *p >= '0' && *p <= '9'; p++)
  {
    value = value * 10.0 + (*p - '0');
    hasDigits = true;
  }
  if (*p == '.')
  {
    double digitValue = 0.1;
    for (p++; *p >= '0' && *p <= '9'; p++, digitValue /= 10.0)
    {
      value += (*p - '0') * digitValue;
      hasDigits = true;
    }
  }
  return hasDigits ? value : 0.0;
}

/** Desktop scales come in quarters (GNOME, KDE) or whole numbers (GDK_SCALE). Eighths keep those exact and turn a
 * DPI a point or two off 96 into 1, so that one-pixel lines stay sharp. */
inline float QuantizeScale(double scale)
{
  return static_cast<float>(std::clamp(std::round(scale * 8.0) / 8.0, kMinScale, kMaxScale));
}

/** @param xftDpi the X resource Xft.dpi, as Xft itself reads it (nullptr if unset)
 * @param gdkScale @param qtScaleFactor the environment variables GDK_SCALE and QT_SCALE_FACTOR (nullptr if unset)
 * @return the first of Xft.dpi / 96, GDK_SCALE, QT_SCALE_FACTOR that is set to a positive number, else 1 */
inline float DesktopScale(const char* xftDpi, const char* gdkScale, const char* qtScaleFactor)
{
  if (const double dpi = ParseDecimal(xftDpi); dpi > 0.0)
    return QuantizeScale(dpi / kReferenceDpi);
  if (const double scale = ParseDecimal(gdkScale); scale > 0.0)
    return QuantizeScale(scale);
  if (const double scale = ParseDecimal(qtScaleFactor); scale > 0.0)
    return QuantizeScale(scale);
  return 1.f;
}

/** DesktopScale() of the X display in $DISPLAY, read through a connection of its own; without a display, only the
 * environment counts. Defined in IGraphicsLinux.cpp, which owns the X11 headers. */
float QueryDesktopScale();
} // namespace linux_scale

END_IGRAPHICS_NAMESPACE
END_IPLUG_NAMESPACE
