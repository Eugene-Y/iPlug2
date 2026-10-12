// Key translation (IGraphicsLinuxKeys.h) and fonts by name (IGraphicsLinuxFontconfig.h), against a real X server.
#include "IPlugStructs.h"
#include "IGraphicsConstants.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>

#include "IGraphicsLinuxKeys.h"
#include "IGraphicsLinuxFontconfig.h"

#include <cstring>
#include <iostream>
#include <string>
#include <vector>

using namespace iplug;
using namespace iplug::igraphics;

namespace
{
int gFailures = 0;

template <typename T>
void Expect(const T& actual, const T& expected, const std::string& label)
{
  if (!(actual == expected))
  {
    std::cerr << "FAIL " << label << ": expected '" << expected << "', got '" << actual << "'\n";
    ++gFailures;
  }
}

void TestKeySymTables()
{
  using namespace linux_keys;
  Expect(VirtualKeyFromKeySym(XK_Return), static_cast<int>(kVK_RETURN), "Return");
  Expect(VirtualKeyFromKeySym(XK_KP_Enter), static_cast<int>(kVK_RETURN), "KP_Enter");
  Expect(VirtualKeyFromKeySym(XK_KP_5), static_cast<int>(kVK_NUMPAD5), "KP_5");
  Expect(VirtualKeyFromKeySym(XK_KP_End), static_cast<int>(kVK_END), "KP_End (NumLock off)");
  Expect(VirtualKeyFromKeySym(XK_F13), static_cast<int>(kVK_F13), "F13");
  Expect(VirtualKeyFromKeySym(XK_ISO_Left_Tab), static_cast<int>(kVK_TAB), "Shift+Tab");
  Expect(VirtualKeyFromKeySym(XK_a), static_cast<int>(kVK_NONE), "a is a character key");
  Expect(CharacterVirtualKey(XK_a), static_cast<int>('A'), "a");
  Expect(CharacterVirtualKey(XK_Z), static_cast<int>('Z'), "Z");
  Expect(CharacterVirtualKey(XK_7), static_cast<int>('7'), "7");
  Expect(CharacterVirtualKey(XK_Cyrillic_a), static_cast<int>(kVK_NONE), "Cyrillic a has no VK of its own");

  Expect(KeySymToUnicode(XK_eacute), 0xe9u, "eacute");
  Expect(KeySymToUnicode(0x1000430), 0x430u, "Unicode keysym");
  Expect(KeySymToUnicode(XK_Cyrillic_a), 0u, "legacy Cyrillic keysym is the input method's");

  char utf8[5];
  EncodeUtf8(0x430, utf8);
  Expect(std::string(utf8), std::string("\xd0\xb0"), "U+0430");
  EncodeUtf8(0x1f600, utf8);
  Expect(std::string(utf8), std::string("\xf0\x9f\x98\x80"), "U+1F600");
  EncodeUtf8(0x1b, utf8);
  Expect(std::string(utf8), std::string(), "Escape gives no text");
}

struct Press
{
  std::string utf8;
  int vk;
  bool s, c, a;
};

std::vector<Press> Translate(Display* pDisplay, XIC pIC, KeySym sym, unsigned state)
{
  XKeyEvent event {};
  event.type = KeyPress;
  event.display = pDisplay;
  event.window = DefaultRootWindow(pDisplay);
  event.root = event.window;
  event.keycode = XKeysymToKeycode(pDisplay, sym);
  event.state = state;
  event.same_screen = True;
  std::vector<Press> presses;
  linux_keys::ForEachKeyPress(pDisplay, pIC, event, [&](const IKeyPress& key) {
    presses.push_back({ key.utf8, key.VK, key.S, key.C, key.A });
  });
  return presses;
}

void ExpectPress(Display* pDisplay, XIC pIC, KeySym sym, unsigned state, const std::string& utf8, int vk, bool c, const std::string& label)
{
  const auto presses = Translate(pDisplay, pIC, sym, state);
  Expect(presses.size(), size_t(1), label + " count");
  if (presses.empty())
    return;
  Expect(presses[0].utf8, utf8, label + " text");
  Expect(presses[0].vk, vk, label + " VK");
  Expect(presses[0].c, c, label + " Ctrl");
}

void TestKeyEvents(Display* pDisplay, XIC pIC, const char* label)
{
  const std::string l = label;
  ExpectPress(pDisplay, pIC, XK_a, 0, "a", 'A', false, l + " a");
  ExpectPress(pDisplay, pIC, XK_a, ShiftMask, "A", 'A', false, l + " Shift+a");
  ExpectPress(pDisplay, pIC, XK_c, ControlMask, "c", 'C', true, l + " Ctrl+c");
  ExpectPress(pDisplay, pIC, XK_1, ShiftMask, "!", '1', false, l + " Shift+1");
  ExpectPress(pDisplay, pIC, XK_Return, 0, "", kVK_RETURN, false, l + " Return");
  ExpectPress(pDisplay, pIC, XK_Escape, 0, "", kVK_ESCAPE, false, l + " Escape");
  ExpectPress(pDisplay, pIC, XK_BackSpace, 0, "", kVK_BACK, false, l + " BackSpace");
  ExpectPress(pDisplay, pIC, XK_space, 0, " ", kVK_SPACE, false, l + " space");
  ExpectPress(pDisplay, pIC, XK_KP_1, Mod2Mask, "1", kVK_NUMPAD1, false, l + " KP_1 NumLock on");
}

// With a second, Cyrillic group (setxkbmap -layout us,ru): letters keep the VK of their Latin key, so
// shortcuts do not depend on the layout, while the text is Cyrillic.
void TestCyrillicGroup(Display* pDisplay, XIC pIC)
{
  const KeyCode keycodeF = XKeysymToKeycode(pDisplay, XK_f);
  if (XkbKeycodeToKeysym(pDisplay, keycodeF, 1, 0) != XK_Cyrillic_a)
  {
    std::cout << "skip: no Cyrillic second group (run `setxkbmap -layout us,ru` first)\n";
    return;
  }
  const unsigned group2 = 1u << 13;
  XKeyEvent event {};
  event.type = KeyPress;
  event.display = pDisplay;
  event.window = DefaultRootWindow(pDisplay);
  event.root = event.window;
  event.keycode = keycodeF;
  event.state = group2;
  event.same_screen = True;
  std::vector<Press> presses;
  linux_keys::ForEachKeyPress(pDisplay, pIC, event, [&](const IKeyPress& key) { presses.push_back({ key.utf8, key.VK, key.S, key.C, key.A }); });
  Expect(presses.size(), size_t(1), "ru f count");
  if (!presses.empty())
  {
    Expect(presses[0].vk, static_cast<int>('F'), "ru f VK");
    if (pIC)
      Expect(presses[0].utf8, std::string("\xd0\xb0"), "ru f text");
  }
  event.state = group2 | ControlMask;
  presses.clear();
  event.keycode = XKeysymToKeycode(pDisplay, XK_c);
  linux_keys::ForEachKeyPress(pDisplay, pIC, event, [&](const IKeyPress& key) { presses.push_back({ key.utf8, key.VK, key.S, key.C, key.A }); });
  if (!presses.empty())
    Expect(presses[0].vk, static_cast<int>('C'), "ru Ctrl+c VK");
  std::cout << "Cyrillic group tested\n";
}

void TestFonts()
{
  if (!linux_fonts::Fontconfig::Get())
  {
    std::cout << "skip: no libfontconfig.so.1\n";
    return;
  }
  WDL_String path;
  int faceIdx = -1;
  Expect(linux_fonts::FindSystemFont("DejaVu Sans", ETextStyle::Normal, path, faceIdx), true, "DejaVu Sans found");
  Expect(std::string(path.get_filepart()), std::string("DejaVuSans.ttf"), "DejaVu Sans file");
  Expect(faceIdx, 0, "DejaVu Sans face");
  Expect(linux_fonts::FindSystemFont("DejaVu Sans", ETextStyle::Bold, path, faceIdx), true, "DejaVu Sans Bold found");
  Expect(std::string(path.get_filepart()), std::string("DejaVuSans-Bold.ttf"), "DejaVu Sans Bold file");
  Expect(linux_fonts::FindSystemFont("No Such Family 12345", ETextStyle::Normal, path, faceIdx), true, "a missing family gets a substitute");
  Expect(path.GetLength() > 0, true, "substitute path");
  Expect(linux_fonts::FindSystemFont("", ETextStyle::Normal, path, faceIdx), false, "empty name");
  // fontconfig maps Times New Roman to Nimbus Roman, a CFF .otf that NanoVG cannot draw: a TrueType font instead
  Expect(linux_fonts::FindSystemFont("Times New Roman", ETextStyle::Normal, path, faceIdx), true, "Times New Roman");
  const std::string file = path.get_filepart();
  Expect(file.size() > 4 && (file.compare(file.size() - 4, 4, ".ttf") == 0 || file.compare(file.size() - 4, 4, ".ttc") == 0), true,
         "Times New Roman substitute is TrueType (" + file + ")");
}
} // namespace

int main()
{
  TestKeySymTables();
  TestFonts();

  Display* pDisplay = XOpenDisplay(nullptr);
  if (!pDisplay)
  {
    std::cerr << "FAIL no X display\n";
    return 1;
  }
  TestKeyEvents(pDisplay, nullptr, "no IM:");

  XIM pIM = XOpenIM(pDisplay, nullptr, nullptr, nullptr);
  XIC pIC = nullptr;
  if (pIM)
  {
    Window window = XCreateSimpleWindow(pDisplay, DefaultRootWindow(pDisplay), 0, 0, 10, 10, 0, 0, 0);
    pIC = XCreateIC(pIM, XNInputStyle, XIMPreeditNothing | XIMStatusNothing, XNClientWindow, window, XNFocusWindow, window, nullptr);
  }
  if (pIC)
    TestKeyEvents(pDisplay, pIC, "IM:");
  else
    std::cout << "skip: no input method\n";
  TestCyrillicGroup(pDisplay, pIC);

  if (pIC)
    XDestroyIC(pIC);
  if (pIM)
    XCloseIM(pIM);
  XCloseDisplay(pDisplay);

  if (gFailures)
    std::cerr << gFailures << " failure(s)\n";
  else
    std::cout << "PASS\n";
  return gFailures ? 1 : 0;
}
