#pragma once

// `std::format`'s floating-point path calls `std::to_chars(double)`, whose
// out-of-line implementation lives in the system libc++.dylib and was only
// introduced in macOS 13.3. On an older deployment target the symbol is
// unavailable (compile error, and absent at load time), so route all
// formatting through the vendored header-only {fmt}, whose float->string is
// self-contained (Dragonbox, no dylib dependency). On a new-enough target we
// keep the native std::format with zero behavioural change.
//
// Use `hvoya::format(...)` everywhere instead of `std::format(...)`; the
// format-string grammar is identical ({fmt} is the reference std::format came
// from), so no format strings need to change.

#if defined(__APPLE__) && defined(__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__) \
    && __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ < 130300

  #ifndef FMT_HEADER_ONLY
    #define FMT_HEADER_ONLY
  #endif
  #include <hvoya/libs/fmt/format.h>

  namespace hvoya { using fmt::format; }

#else

  #include <format>

  namespace hvoya { using std::format; }

#endif
