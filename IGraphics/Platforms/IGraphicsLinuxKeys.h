/*
 ==============================================================================

 This file is part of the iPlug 2 library. Copyright (C) the iPlug 2 developers.

 See LICENSE.txt for  more info.

 ==============================================================================
*/

#pragma once

/** @file
 * @brief X11 key events to iPlug key presses: keysym to kVK_* virtual keys (Windows semantics, which
 * ITextEntryControl and the keyboard controls expect) and the typed text as UTF-8.
 * Include after <X11/Xlib.h>, <X11/XKBlib.h> and <X11/keysym.h>.
 */

#include <cstring>

BEGIN_IPLUG_NAMESPACE
BEGIN_IGRAPHICS_NAMESPACE

namespace linux_keys
{

/** Keys that are not characters: editing, navigation, function, keypad and modifier keys */
inline int VirtualKeyFromKeySym(KeySym sym)
{
  if (sym >= XK_KP_0 && sym <= XK_KP_9)
    return kVK_NUMPAD0 + static_cast<int>(sym - XK_KP_0);
  if (sym >= XK_F1 && sym <= XK_F24)
    return kVK_F1 + static_cast<int>(sym - XK_F1);

  switch (sym)
  {
    case XK_BackSpace: return kVK_BACK;
    case XK_Tab: case XK_ISO_Left_Tab: case XK_KP_Tab: return kVK_TAB;
    case XK_Clear: case XK_KP_Begin: return kVK_CLEAR;
    case XK_Return: case XK_KP_Enter: return kVK_RETURN;
    case XK_Shift_L: case XK_Shift_R: return kVK_SHIFT;
    case XK_Control_L: case XK_Control_R: return kVK_CONTROL;
    case XK_Alt_L: case XK_Alt_R: case XK_Meta_L: case XK_Meta_R: return kVK_MENU;
    case XK_Super_L: case XK_Super_R: return kVK_LWIN;
    case XK_Pause: return kVK_PAUSE;
    case XK_Caps_Lock: return kVK_CAPITAL;
    case XK_Escape: return kVK_ESCAPE;
    case XK_space: case XK_KP_Space: return kVK_SPACE;
    case XK_Prior: case XK_KP_Prior: return kVK_PRIOR;
    case XK_Next: case XK_KP_Next: return kVK_NEXT;
    case XK_End: case XK_KP_End: return kVK_END;
    case XK_Home: case XK_KP_Home: return kVK_HOME;
    case XK_Left: case XK_KP_Left: return kVK_LEFT;
    case XK_Up: case XK_KP_Up: return kVK_UP;
    case XK_Right: case XK_KP_Right: return kVK_RIGHT;
    case XK_Down: case XK_KP_Down: return kVK_DOWN;
    case XK_Select: return kVK_SELECT;
    case XK_Print: return kVK_SNAPSHOT;
    case XK_Insert: case XK_KP_Insert: return kVK_INSERT;
    case XK_Delete: case XK_KP_Delete: return kVK_DELETE;
    case XK_Help: return kVK_HELP;
    case XK_KP_Multiply: return kVK_MULTIPLY;
    case XK_KP_Add: return kVK_ADD;
    case XK_KP_Separator: return kVK_SEPARATOR;
    case XK_KP_Subtract: return kVK_SUBTRACT;
    case XK_KP_Decimal: return kVK_DECIMAL;
    case XK_KP_Divide: return kVK_DIVIDE;
    case XK_Num_Lock: return kVK_NUMLOCK;
    case XK_Scroll_Lock: return kVK_SCROLL;
    default: return kVK_NONE;
  }
}

/** 'A'..'Z' and '0'..'9' for Latin letter and digit keysyms, else kVK_NONE */
inline int CharacterVirtualKey(KeySym sym)
{
  if (sym >= XK_a && sym <= XK_z)
    return 'A' + static_cast<int>(sym - XK_a);
  if ((sym >= XK_A && sym <= XK_Z) || (sym >= XK_0 && sym <= XK_9))
    return static_cast<int>(sym);
  return kVK_NONE;
}

/** Unicode code point of a keysym, or 0. Covers Latin-1 and the direct Unicode keysym range; the legacy
 * non-Latin keysym blocks only come from the input method, which converts them itself. */
inline unsigned KeySymToUnicode(KeySym sym)
{
  if ((sym >= 0x20 && sym <= 0x7e) || (sym >= 0xa0 && sym <= 0xff))
    return static_cast<unsigned>(sym);
  if ((sym & 0xff000000) == 0x01000000)
    return static_cast<unsigned>(sym & 0x00ffffff);
  return 0;
}

/** Writes one code point as UTF-8 plus a terminating NUL (at most 5 bytes); control characters give "" */
inline void EncodeUtf8(unsigned codePoint, char* pOut)
{
  unsigned char* p = reinterpret_cast<unsigned char*>(pOut);
  if (codePoint < 0x20 || codePoint == 0x7f || codePoint > 0x10ffff)
  {
    p[0] = 0;
  }
  else if (codePoint < 0x80)
  {
    p[0] = static_cast<unsigned char>(codePoint);
    p[1] = 0;
  }
  else if (codePoint < 0x800)
  {
    p[0] = static_cast<unsigned char>(0xc0 | (codePoint >> 6));
    p[1] = static_cast<unsigned char>(0x80 | (codePoint & 0x3f));
    p[2] = 0;
  }
  else if (codePoint < 0x10000)
  {
    p[0] = static_cast<unsigned char>(0xe0 | (codePoint >> 12));
    p[1] = static_cast<unsigned char>(0x80 | ((codePoint >> 6) & 0x3f));
    p[2] = static_cast<unsigned char>(0x80 | (codePoint & 0x3f));
    p[3] = 0;
  }
  else
  {
    p[0] = static_cast<unsigned char>(0xf0 | (codePoint >> 18));
    p[1] = static_cast<unsigned char>(0x80 | ((codePoint >> 12) & 0x3f));
    p[2] = static_cast<unsigned char>(0x80 | ((codePoint >> 6) & 0x3f));
    p[3] = static_cast<unsigned char>(0x80 | (codePoint & 0x3f));
    p[4] = 0;
  }
}

/** Length in bytes of the UTF-8 sequence that starts with this byte (1 for stray continuation bytes) */
inline int Utf8SequenceLength(unsigned char lead)
{
  if (lead >= 0xf0) return 4;
  if (lead >= 0xe0) return 3;
  if (lead >= 0xc0) return 2;
  return 1;
}

/** Letter and digit keys get the VK of their physical key, as on Windows: Shift+1 is '1' and Ctrl+C is 'C'
 * in a Cyrillic or Greek layout too, so that shortcuts do not depend on the active layout. */
inline int PhysicalCharacterVirtualKey(Display* pDisplay, unsigned keycode, unsigned state)
{
  const int activeGroup = static_cast<int>((state >> 13) & 3);
  const KeyCode code = static_cast<KeyCode>(keycode);
  int vk = CharacterVirtualKey(XkbKeycodeToKeysym(pDisplay, code, activeGroup, 0));
  if (vk == kVK_NONE)
  {
    const int shifted = CharacterVirtualKey(XkbKeycodeToKeysym(pDisplay, code, activeGroup, 1));
    if (shifted >= '0' && shifted <= '9') // the digit row of AZERTY layouts
      vk = shifted;
  }
  for (int group = 0; vk == kVK_NONE && group < 4; ++group)
  {
    if (group != activeGroup)
      vk = CharacterVirtualKey(XkbKeycodeToKeysym(pDisplay, code, group, 0));
  }
  return vk;
}

/** Calls func with the key presses of one key event: usually one, more when an input method commits several
 * characters at once.
 * @param pIC the editor's input context, or nullptr without an input method. It is used for presses only:
 *        Xutf8LookupString is undefined for releases. */
template <typename KeyPressFunc>
inline void ForEachKeyPress(Display* pDisplay, XIC pIC, XKeyEvent& event, KeyPressFunc func)
{
  const bool isPress = event.type == KeyPress;
  const bool shift = event.state & ShiftMask;
  const bool control = event.state & ControlMask;
  const bool alt = event.state & Mod1Mask;

  char text[64] = {};
  int textLength = 0;
  KeySym sym = NoSymbol;

  // With Ctrl or Alt held, the text is that of the key without them (as mac's charactersIgnoringModifiers):
  // Ctrl+A reports "a", not the control character U+0001.
  if (pIC && isPress && !control && !alt)
  {
    Status status = 0;
    textLength = Xutf8LookupString(pIC, &event, text, sizeof(text) - 1, &sym, &status);
    if (status != XLookupChars && status != XLookupBoth)
      textLength = 0;
    if (status != XLookupKeySym && status != XLookupBoth)
      sym = NoSymbol;
  }
  if (sym == NoSymbol)
  {
    XKeyEvent plain = event;
    plain.state &= ~(ControlMask | Mod1Mask);
    char latin1[8] = {};
    const int latin1Length = XLookupString(&plain, latin1, sizeof(latin1), &sym, nullptr);
    if (textLength == 0)
    {
      // XLookupString's text is Latin-1 and covers the keypad; KeySymToUnicode covers Unicode keysyms
      EncodeUtf8(latin1Length == 1 ? static_cast<unsigned char>(latin1[0]) : KeySymToUnicode(sym), text);
      textLength = static_cast<int>(strlen(text));
    }
  }
  text[textLength] = 0;

  int vk = VirtualKeyFromKeySym(sym);
  if (vk == kVK_NONE)
    vk = PhysicalCharacterVirtualKey(pDisplay, event.keycode, event.state);

  // Control characters (Return, Tab, Escape...) travel as the VK only.
  if (textLength > 0 && static_cast<unsigned char>(text[0]) < 0x20)
    textLength = 0;

  if (textLength == 0)
  {
    func(IKeyPress("", vk, shift, control, alt));
    return;
  }

  for (int pos = 0; pos < textLength;)
  {
    const int length = Utf8SequenceLength(static_cast<unsigned char>(text[pos]));
    if (pos + length > textLength)
      break;
    char utf8[5] = {};
    memcpy(utf8, text + pos, length);
    func(IKeyPress(utf8, pos == 0 ? vk : kVK_NONE, shift, control, alt));
    pos += length;
  }
}

} // namespace linux_keys

END_IGRAPHICS_NAMESPACE
END_IPLUG_NAMESPACE
