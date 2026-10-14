/*
 ==============================================================================

 This file is part of the iPlug 2 library. Copyright (C) the iPlug 2 developers.

 See LICENSE.txt for  more info.

 ==============================================================================
*/

#pragma once

/** @file
 * @brief libdbus, loaded at run time: no link dependency, no headers needed, and nothing breaks where it is missing.
 */

#include <cstdint>

#include "IPlugPlatform.h"

BEGIN_IPLUG_NAMESPACE

namespace linux_dbus
{
struct Connection;
struct Message;

/** Caller-allocated iterator. libdbus' own is 72 bytes on 64-bit targets; this is larger and as aligned. */
struct MessageIter
{
  void* mOpaque[16];
};

using Bool = uint32_t;

enum EType : int
{
  kTypeInvalid = 0,
  kTypeByte = 'y',
  kTypeBoolean = 'b',
  kTypeUInt32 = 'u',
  kTypeString = 's',
  kTypeObjectPath = 'o',
  kTypeArray = 'a',
  kTypeVariant = 'v',
  kTypeStruct = 'r',
  kTypeDictEntry = 'e'
};

enum EMessageType : int
{
  kMessageMethodCall = 1,
  kMessageMethodReturn = 2,
  kMessageError = 3,
  kMessageSignal = 4
};

/** The libdbus entry points dialogs use, in libdbus' own signatures (DBusError* arguments are always passed null) */
struct Functions
{
  Connection* (*connection_open_private)(const char* address, void* error);
  void (*connection_close)(Connection*);
  void (*connection_unref)(Connection*);
  void (*connection_set_exit_on_disconnect)(Connection*, Bool);
  Bool (*connection_get_unix_fd)(Connection*, int* fd);
  Bool (*connection_send)(Connection*, Message*, uint32_t* serial);
  Bool (*connection_read_write)(Connection*, int timeoutMs);
  Message* (*connection_pop_message)(Connection*);
  void (*connection_flush)(Connection*);
  Bool (*connection_get_is_connected)(Connection*);
  Bool (*connection_get_is_authenticated)(Connection*);

  Message* (*message_new_method_call)(const char* destination, const char* path, const char* interface, const char* method);
  void (*message_unref)(Message*);
  void (*message_set_no_reply)(Message*, Bool);
  int (*message_get_type)(Message*);
  uint32_t (*message_get_reply_serial)(Message*);
  const char* (*message_get_path)(Message*);
  const char* (*message_get_interface)(Message*);
  const char* (*message_get_member)(Message*);
  const char* (*message_get_error_name)(Message*);

  Bool (*message_iter_init)(Message*, MessageIter*);
  int (*message_iter_get_arg_type)(MessageIter*);
  void (*message_iter_get_basic)(MessageIter*, void* value);
  Bool (*message_iter_next)(MessageIter*);
  void (*message_iter_recurse)(MessageIter*, MessageIter* sub);
  void (*message_iter_init_append)(Message*, MessageIter*);
  Bool (*message_iter_append_basic)(MessageIter*, int type, const void* value);
  Bool (*message_iter_append_fixed_array)(MessageIter*, int elementType, const void* value, int count);
  Bool (*message_iter_open_container)(MessageIter*, int type, const char* signature, MessageIter* sub);
  Bool (*message_iter_close_container)(MessageIter*, MessageIter* sub);

  /** libdbus loaded once per process and never unloaded. @return nullptr if it is missing or incomplete */
  static const Functions* Get();
};

/** The session bus address, without ever starting a bus: $DBUS_SESSION_BUS_ADDRESS, else $XDG_RUNTIME_DIR/bus
 * when that socket exists. @return false if there is none */
bool GetSessionBusAddress(char* address, int size);

} // namespace linux_dbus

END_IPLUG_NAMESPACE
