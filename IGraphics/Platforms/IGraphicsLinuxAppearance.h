/*
 ==============================================================================

 This file is part of the iPlug 2 library. Copyright (C) the iPlug 2 developers.

 See LICENSE.txt for  more info.

 ==============================================================================
*/

#pragma once

/** @file
 * @brief The desktop's light/dark preference: xdg-desktop-portal's org.freedesktop.appearance color-scheme, read and
 * followed over a private session-bus connection. Main thread only; nothing here blocks.
 */

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "Linux/IPlugLinuxDBus.h"

BEGIN_IPLUG_NAMESPACE
BEGIN_IGRAPHICS_NAMESPACE

class LinuxAppearance
{
public:
  ~LinuxAppearance() { Stop(); }

  /** Opens the connection and asks the portal. Without a session bus, libdbus or a portal the scheme stays light,
   * as on Windows. */
  void Start()
  {
    namespace dbus = linux_dbus;
    if (mConnection || !(mDBus = dbus::Functions::Get()))
      return;
    char address[1024];
    if (!dbus::GetSessionBusAddress(address, sizeof(address)))
      return;
    if (!(mConnection = mDBus->connection_open_private(address, nullptr)))
      return;
    mDBus->connection_set_exit_on_disconnect(mConnection, 0);
    // libdbus' dbus_bus_register() would block the host's main thread on this Hello.
    mHelloSerial = Send(mDBus->message_new_method_call(kBus, kBusPath, kBus, "Hello"));
  }

  void Stop()
  {
    if (mConnection)
    {
      mDBus->connection_close(mConnection);
      mDBus->connection_unref(mConnection);
      mConnection = nullptr;
    }
    mHelloSerial = mReadSerial = 0;
  }

  bool IsDark() const { return mDark; }

  /** Handles whatever the bus has sent. @return true when the scheme changed */
  bool Poll()
  {
    namespace dbus = linux_dbus;
    if (!mConnection)
      return false;
    if (!mDBus->connection_get_is_connected(mConnection))
    {
      Stop();
      return false;
    }
    mDBus->connection_read_write(mConnection, 0);
    const bool wasDark = mDark;
    while (dbus::Message* pMessage = mDBus->connection_pop_message(mConnection))
    {
      const int type = mDBus->message_get_type(pMessage);
      const uint32_t replyTo = mDBus->message_get_reply_serial(pMessage);
      if (type == dbus::kMessageMethodReturn && replyTo == mHelloSerial)
      {
        mHelloSerial = 0;
        SubscribeAndRead();
      }
      else if (replyTo && replyTo == mReadSerial)
      {
        mReadSerial = 0;
        if (type == dbus::kMessageMethodReturn)
          ReadScheme(pMessage, false);
      }
      else if (type == dbus::kMessageSignal && !strcmp(Str(mDBus->message_get_member(pMessage)), "SettingChanged")
               && !strcmp(Str(mDBus->message_get_interface(pMessage)), kSettings))
      {
        ReadScheme(pMessage, true);
      }
      mDBus->message_unref(pMessage);
    }
    return mDark != wasDark;
  }

private:
  static constexpr const char* kBus = "org.freedesktop.DBus";
  static constexpr const char* kBusPath = "/org/freedesktop/DBus";
  static constexpr const char* kPortal = "org.freedesktop.portal.Desktop";
  static constexpr const char* kPortalPath = "/org/freedesktop/portal/desktop";
  static constexpr const char* kSettings = "org.freedesktop.portal.Settings";
  static constexpr const char* kNamespace = "org.freedesktop.appearance";
  static constexpr const char* kKey = "color-scheme";
  static constexpr uint32_t kPreferDark = 1; // 0: no preference, 2: prefer light

  static const char* Str(const char* str) { return str ? str : ""; }

  uint32_t Send(linux_dbus::Message* pMessage)
  {
    uint32_t serial = 0;
    if (!pMessage)
      return 0;
    mDBus->connection_send(mConnection, pMessage, &serial);
    mDBus->message_unref(pMessage);
    mDBus->connection_flush(mConnection);
    return serial;
  }

  void AppendString(linux_dbus::MessageIter* pIter, const char* value)
  {
    mDBus->message_iter_append_basic(pIter, linux_dbus::kTypeString, &value);
  }

  void SubscribeAndRead()
  {
    namespace dbus = linux_dbus;
    if (dbus::Message* pMatch = mDBus->message_new_method_call(kBus, kBusPath, kBus, "AddMatch"))
    {
      char rule[256];
      snprintf(rule, sizeof(rule), "type='signal',interface='%s',member='SettingChanged',arg0='%s'", kSettings, kNamespace);
      dbus::MessageIter args;
      mDBus->message_iter_init_append(pMatch, &args);
      AppendString(&args, rule);
      mDBus->message_set_no_reply(pMatch, 1);
      Send(pMatch);
    }
    // Read rather than the newer ReadOne: every portal version has it; it wraps the value in one more variant.
    if (dbus::Message* pRead = mDBus->message_new_method_call(kPortal, kPortalPath, kSettings, "Read"))
    {
      dbus::MessageIter args;
      mDBus->message_iter_init_append(pRead, &args);
      AppendString(&args, kNamespace);
      AppendString(&args, kKey);
      mReadSerial = Send(pRead);
    }
  }

  /** A Read reply (v) or a SettingChanged signal (s namespace, s key, v value): the value is a uint32, under however
   * many variants the portal put around it */
  void ReadScheme(linux_dbus::Message* pMessage, bool isSignal)
  {
    namespace dbus = linux_dbus;
    dbus::MessageIter it;
    if (!mDBus->message_iter_init(pMessage, &it))
      return;
    if (isSignal)
    {
      const char* names[2] = { nullptr, nullptr };
      for (const char*& name : names)
      {
        if (mDBus->message_iter_get_arg_type(&it) != dbus::kTypeString)
          return;
        mDBus->message_iter_get_basic(&it, &name);
        mDBus->message_iter_next(&it);
      }
      if (strcmp(Str(names[0]), kNamespace) || strcmp(Str(names[1]), kKey))
        return;
    }
    dbus::MessageIter inner;
    while (mDBus->message_iter_get_arg_type(&it) == dbus::kTypeVariant)
    {
      mDBus->message_iter_recurse(&it, &inner);
      it = inner;
    }
    if (mDBus->message_iter_get_arg_type(&it) != dbus::kTypeUInt32)
      return;
    uint32_t scheme = 0;
    mDBus->message_iter_get_basic(&it, &scheme);
    mDark = scheme == kPreferDark;
  }

  const linux_dbus::Functions* mDBus = nullptr;
  linux_dbus::Connection* mConnection = nullptr;
  uint32_t mHelloSerial = 0;
  uint32_t mReadSerial = 0;
  bool mDark = false;
};

END_IGRAPHICS_NAMESPACE
END_IPLUG_NAMESPACE
