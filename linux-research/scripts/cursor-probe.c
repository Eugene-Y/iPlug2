// Prints the current X cursor as "<name> <width>x<height> <opaque pixels>" (XFixes). Used by xact.sh expect-cursor.
// Build: gcc -O1 -o /tmp/cursor-probe scripts/cursor-probe.c -lX11 -lXfixes
#include <X11/Xlib.h>
#include <X11/extensions/Xfixes.h>
#include <stdio.h>

int main(void)
{
  Display* pDisplay = XOpenDisplay(NULL);
  if (!pDisplay)
    return 1;
  XFixesCursorImage* pImage = XFixesGetCursorImage(pDisplay);
  if (!pImage)
    return 1;
  int opaque = 0;
  for (int i = 0; i < pImage->width * pImage->height; ++i)
    opaque += (pImage->pixels[i] >> 24) != 0;
  printf("%s %dx%d %d\n", pImage->name && *pImage->name ? pImage->name : "(unnamed)", pImage->width, pImage->height, opaque);
  return 0;
}
