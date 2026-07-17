#pragma once

// std::filesystem's out-of-line implementation lives in the system libc++.dylib
// and was only introduced in macOS 10.15. On an older deployment target the
// symbols are unavailable (compile error via availability attributes, and absent
// at load time), so route filesystem access through the vendored header-only
// ghc::filesystem, a drop-in std::filesystem clone with no dylib dependency. On
// a new-enough target we keep the native std::filesystem with zero change.
//
// Use `hvoya::fs` everywhere instead of `std::filesystem`; the API is identical
// (ghc::filesystem mirrors the C++17 std::filesystem spec), so no call sites need
// to change beyond the namespace.
//
// The gate keys off the per-arch deployment target: the arm64 slice is built with
// a min of 11.0 (its natural floor — Apple Silicon never runs below it), so it
// takes the native std::filesystem and Apple Silicon users get the stock library.
// Only the pre-Catalina x86_64 slice trips the fallback and pulls in ghc.

#if defined(__APPLE__) && defined(__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__) \
    && __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ < 101500

  #include <hvoya/libs/ghc/filesystem.hpp>

  namespace hvoya { namespace fs = ghc::filesystem; }

#else

  #include <filesystem>

  namespace hvoya { namespace fs = std::filesystem; }

#endif
