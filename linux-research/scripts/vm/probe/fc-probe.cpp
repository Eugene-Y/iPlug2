// Fonts by name on a desktop distro: the first candidates linux_fonts::FindSystemFont offers (accept = false, so it
// lists them), and the face the editor would take (the first one stb_truetype can init, as IFontData does).
// Build (Docker, 24.04 image): see the g++ line in notes/14
// Run: fc-probe "Family" ...
#include "IPlugStructs.h"
#include "IGraphicsConstants.h"
#include "IGraphicsLinuxFontconfig.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>

using namespace iplug::igraphics;

static bool StbCanInit(const char* path, int faceIdx)
{
  std::ifstream f(path, std::ios::binary);
  std::vector<unsigned char> data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  if (data.empty()) return false;
  const int offset = stbtt_GetFontOffsetForIndex(data.data(), faceIdx);
  stbtt_fontinfo info;
  return offset >= 0 && stbtt_InitFont(&info, data.data(), offset);
}

int main(int argc, char** argv)
{
  const ETextStyle styles[] = {ETextStyle::Normal, ETextStyle::Bold, ETextStyle::Italic};
  const char* names[] = {"Normal", "Bold", "Italic"};
  for (int a = 1; a < argc; ++a)
    for (int s = 0; s < 3; ++s)
    {
      int n = 0;
      printf("%s / %s: offered", argv[a], names[s]);
      linux_fonts::FindSystemFont(argv[a], styles[s], [&](const char* path, int idx) {
        if (n++ < 4) printf(" [%s #%d]", strrchr(path, '/') + 1, idx);
        return false;
      });
      WDL_String chosen; int chosenIdx = -1;
      linux_fonts::FindSystemFont(argv[a], styles[s], [&](const char* path, int idx) {
        if (!StbCanInit(path, idx)) return false;
        chosen.Set(path); chosenIdx = idx; return true;
      });
      printf("\n   -> takes %s #%d\n", chosen.Get(), chosenIdx);
    }
  return 0;
}
