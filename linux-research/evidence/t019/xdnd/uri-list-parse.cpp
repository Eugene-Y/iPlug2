#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include "IPlugPlatform.h"
#include "IGraphicsLinuxDrop.h"
#include <cstdio>
using namespace iplug::igraphics;
int main() {
  const char list[] = "# comment\r\nfile:///tmp/a%20b/%C3%B6.wav\r\nfile://localhost/tmp/x.wav\r\nfile://otherhost/tmp/y.wav\r\nhttp://e.org/z\r\nfile:///bad%00nul\r\nfile:///tmp/%zz.wav\r\nfile:///tmp/last.wav";
  WDL_TypedBuf<char> paths; int count = 0;
  LinuxFileDrop::ParseUriList(list, sizeof(list) - 1, paths, count);
  printf("count %d\n", count);
  for (const char* p = paths.Get(); count-- > 0; p += strlen(p) + 1) printf("[%s]\n", p);
}
