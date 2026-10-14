/*
 ==============================================================================

 This file is part of the iPlug 2 library. Copyright (C) the iPlug 2 developers.

 See LICENSE.txt for  more info.

 ==============================================================================
*/

#include "IPlugLinuxDBus.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>

#include <dlfcn.h>
#include <sys/stat.h>

using namespace iplug;
using namespace iplug::linux_dbus;

namespace
{
template <typename T>
bool Load(void* pLibrary, T& function, const char* name)
{
  function = reinterpret_cast<T>(dlsym(pLibrary, name));
  return function != nullptr;
}

bool LoadAll(void* pLibrary, Functions& f)
{
  return Load(pLibrary, f.connection_open_private, "dbus_connection_open_private")
      && Load(pLibrary, f.connection_close, "dbus_connection_close")
      && Load(pLibrary, f.connection_unref, "dbus_connection_unref")
      && Load(pLibrary, f.connection_set_exit_on_disconnect, "dbus_connection_set_exit_on_disconnect")
      && Load(pLibrary, f.connection_get_unix_fd, "dbus_connection_get_unix_fd")
      && Load(pLibrary, f.connection_send, "dbus_connection_send")
      && Load(pLibrary, f.connection_read_write, "dbus_connection_read_write")
      && Load(pLibrary, f.connection_pop_message, "dbus_connection_pop_message")
      && Load(pLibrary, f.connection_flush, "dbus_connection_flush")
      && Load(pLibrary, f.connection_get_is_connected, "dbus_connection_get_is_connected")
      && Load(pLibrary, f.connection_get_is_authenticated, "dbus_connection_get_is_authenticated")
      && Load(pLibrary, f.message_new_method_call, "dbus_message_new_method_call")
      && Load(pLibrary, f.message_unref, "dbus_message_unref")
      && Load(pLibrary, f.message_set_no_reply, "dbus_message_set_no_reply")
      && Load(pLibrary, f.message_get_type, "dbus_message_get_type")
      && Load(pLibrary, f.message_get_reply_serial, "dbus_message_get_reply_serial")
      && Load(pLibrary, f.message_get_path, "dbus_message_get_path")
      && Load(pLibrary, f.message_get_interface, "dbus_message_get_interface")
      && Load(pLibrary, f.message_get_member, "dbus_message_get_member")
      && Load(pLibrary, f.message_get_error_name, "dbus_message_get_error_name")
      && Load(pLibrary, f.message_iter_init, "dbus_message_iter_init")
      && Load(pLibrary, f.message_iter_get_arg_type, "dbus_message_iter_get_arg_type")
      && Load(pLibrary, f.message_iter_get_basic, "dbus_message_iter_get_basic")
      && Load(pLibrary, f.message_iter_next, "dbus_message_iter_next")
      && Load(pLibrary, f.message_iter_recurse, "dbus_message_iter_recurse")
      && Load(pLibrary, f.message_iter_init_append, "dbus_message_iter_init_append")
      && Load(pLibrary, f.message_iter_append_basic, "dbus_message_iter_append_basic")
      && Load(pLibrary, f.message_iter_append_fixed_array, "dbus_message_iter_append_fixed_array")
      && Load(pLibrary, f.message_iter_open_container, "dbus_message_iter_open_container")
      && Load(pLibrary, f.message_iter_close_container, "dbus_message_iter_close_container");
}
} // namespace

const Functions* Functions::Get()
{
  static Functions sFunctions;
  static const Functions* spLoaded = nullptr;
  static std::once_flag sOnce;
  std::call_once(sOnce, []() {
    // Never closed: the host or another plug-in may share the library, and libdbus keeps process-wide state.
    void* pLibrary = dlopen("libdbus-1.so.3", RTLD_NOW | RTLD_LOCAL);
    if (!pLibrary)
      return;
    if (LoadAll(pLibrary, sFunctions))
      spLoaded = &sFunctions;
    else
      fprintf(stderr, "iPlug2: libdbus-1.so.3 lacks functions the file dialogs need\n");
  });
  return spLoaded;
}

bool linux_dbus::GetSessionBusAddress(char* address, int size)
{
  const char* pAddress = getenv("DBUS_SESSION_BUS_ADDRESS");
  if (pAddress && pAddress[0])
  {
    // libdbus would start a private bus daemon for "autolaunch:"
    if (strncmp(pAddress, "autolaunch:", 11) == 0)
      return false;
    return snprintf(address, size, "%s", pAddress) < size;
  }

  const char* pRuntimeDir = getenv("XDG_RUNTIME_DIR");
  if (!pRuntimeDir || !pRuntimeDir[0])
    return false;

  char socketPath[512];
  if (snprintf(socketPath, sizeof(socketPath), "%s/bus", pRuntimeDir) >= static_cast<int>(sizeof(socketPath)))
    return false;
  struct stat info;
  if (stat(socketPath, &info) != 0 || !S_ISSOCK(info.st_mode))
    return false;

  int length = snprintf(address, size, "unix:path=");
  for (const char* p = socketPath; *p && length < size; p++)
  {
    const bool plain = isalnum(static_cast<unsigned char>(*p)) || strchr("-_/.\\*", *p);
    length += plain ? snprintf(address + length, size - length, "%c", *p)
                    : snprintf(address + length, size - length, "%%%02x", static_cast<unsigned char>(*p));
  }
  return length < size;
}
