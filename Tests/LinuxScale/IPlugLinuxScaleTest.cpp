// The desktop scale an editor uses when no host gives one (IGraphicsLinuxScale.h).
#include "IGraphicsLinuxScale.h"

#include <clocale>
#include <iostream>
#include <string>

using namespace iplug::igraphics::linux_scale;

namespace
{
int gFailures = 0;

void Expect(double actual, double expected, const std::string& label)
{
  if (actual != expected)
  {
    std::cerr << "FAIL " << label << ": expected " << expected << ", got " << actual << "\n";
    ++gFailures;
  }
}

void TestParseDecimal()
{
  Expect(ParseDecimal("144"), 144.0, "integer");
  Expect(ParseDecimal("1.5"), 1.5, "fraction");
  Expect(ParseDecimal("1.25"), 1.25, "two decimals");
  Expect(ParseDecimal(".5"), 0.5, "no integer part");
  Expect(ParseDecimal("2."), 2.0, "trailing point");
  Expect(ParseDecimal("120 // comment"), 120.0, "number, then text");
  Expect(ParseDecimal(""), 0.0, "empty");
  Expect(ParseDecimal(nullptr), 0.0, "unset");
  Expect(ParseDecimal("abc"), 0.0, "no number");
  Expect(ParseDecimal("."), 0.0, "point only");
  Expect(ParseDecimal("-2"), 0.0, "negative");
  Expect(ParseDecimal("1,5"), 1.0, "a decimal comma is not a decimal point");
}

void TestQuantizeScale()
{
  Expect(QuantizeScale(1.0), 1.0, "1");
  Expect(QuantizeScale(1.25), 1.25, "GNOME 125 %");
  Expect(QuantizeScale(1.75), 1.75, "GNOME 175 %");
  Expect(QuantizeScale(97.0 / 96.0), 1.0, "96 dpi plus a measuring error");
  Expect(QuantizeScale(110.0 / 96.0), 1.125, "110 dpi");
  Expect(QuantizeScale(0.75), 1.0, "below 1");
  Expect(QuantizeScale(9.0), 4.0, "above 4");
}

void TestDesktopScale()
{
  Expect(DesktopScale("144", nullptr, nullptr), 1.5, "Xft.dpi 144");
  Expect(DesktopScale("192", nullptr, nullptr), 2.0, "Xft.dpi 192");
  Expect(DesktopScale("96", "2", "3"), 1.0, "Xft.dpi wins over the environment");
  Expect(DesktopScale(nullptr, "2", "1.5"), 2.0, "GDK_SCALE before QT_SCALE_FACTOR");
  Expect(DesktopScale("", "", "1.5"), 1.5, "empty values do not count");
  Expect(DesktopScale("x", "0", "1.5"), 1.5, "unparsable or zero values do not count");
  Expect(DesktopScale(nullptr, nullptr, nullptr), 1.0, "nothing set");
}

void TestLocale()
{
  // A host may run with a decimal comma (setlocale(LC_ALL, "") under de_DE); the parse must not change.
  if (!std::setlocale(LC_NUMERIC, "de_DE.UTF-8"))
  {
    std::cout << "SKIP locale case: de_DE.UTF-8 not installed\n";
    return;
  }
  Expect(ParseDecimal("1.5"), 1.5, "1.5 under de_DE");
  Expect(DesktopScale(nullptr, nullptr, "1.5"), 1.5, "QT_SCALE_FACTOR 1.5 under de_DE");
  std::setlocale(LC_NUMERIC, "C");
}
} // namespace

int main()
{
  TestParseDecimal();
  TestQuantizeScale();
  TestDesktopScale();
  TestLocale();
  std::cout << (gFailures ? "FAILED: " : "PASS, failures: ") << gFailures << "\n";
  return gFailures ? 1 : 0;
}
