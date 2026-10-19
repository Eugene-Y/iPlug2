/* T-020: a stand-in for xdg-desktop-portal's Settings interface, only org.freedesktop.appearance color-scheme.
 * Usage: fake-portal <initial scheme> <seconds> <next scheme> [<seconds> <next scheme> ...]
 * Serves Read (v(v(u))) and ReadOne (v(u)); after each delay switches the scheme and emits SettingChanged. */
#include <dbus/dbus.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const char* NS = "org.freedesktop.appearance";
static const char* KEY = "color-scheme";
static const char* IFACE = "org.freedesktop.portal.Settings";
static const char* PATH = "/org/freedesktop/portal/desktop";

static void append_value(DBusMessageIter* it, dbus_uint32_t v, int wraps)
{
  DBusMessageIter var;
  dbus_message_iter_open_container(it, DBUS_TYPE_VARIANT, wraps > 1 ? "v" : "u", &var);
  if (wraps > 1) append_value(&var, v, wraps - 1);
  else dbus_message_iter_append_basic(&var, DBUS_TYPE_UINT32, &v);
  dbus_message_iter_close_container(it, &var);
}

int main(int argc, char** argv)
{
  DBusConnection* c = dbus_bus_get(DBUS_BUS_SESSION, NULL);
  if (!c || dbus_bus_request_name(c, "org.freedesktop.portal.Desktop", DBUS_NAME_FLAG_DO_NOT_QUEUE, NULL) != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER)
    { fprintf(stderr, "fake-portal: cannot own the name\n"); return 1; }
  dbus_uint32_t scheme = (dbus_uint32_t) atoi(argv[1]);
  int next = 2;
  time_t switchAt = argc > next ? time(NULL) + atoi(argv[next]) : 0;
  printf("fake-portal: scheme %u\n", scheme); fflush(stdout);
  for (;;)
  {
    dbus_connection_read_write(c, 100);
    DBusMessage* m;
    while ((m = dbus_connection_pop_message(c)))
    {
      if (dbus_message_is_method_call(m, IFACE, "Read") || dbus_message_is_method_call(m, IFACE, "ReadOne"))
      {
        const char *ns = "", *key = "";
        dbus_message_get_args(m, NULL, DBUS_TYPE_STRING, &ns, DBUS_TYPE_STRING, &key, DBUS_TYPE_INVALID);
        DBusMessage* r;
        if (!strcmp(ns, NS) && !strcmp(key, KEY))
        {
          r = dbus_message_new_method_return(m);
          DBusMessageIter it;
          dbus_message_iter_init_append(r, &it);
          append_value(&it, scheme, dbus_message_is_method_call(m, IFACE, "Read") ? 2 : 1);
        }
        else
          r = dbus_message_new_error(m, "org.freedesktop.portal.Error.NotFound", "no such setting");
        printf("fake-portal: %s(%s, %s)\n", dbus_message_get_member(m), ns, key); fflush(stdout);
        dbus_connection_send(c, r, NULL);
        dbus_message_unref(r);
      }
      dbus_message_unref(m);
    }
    if (switchAt && time(NULL) >= switchAt)
    {
      scheme = (dbus_uint32_t) atoi(argv[next + 1]);
      DBusMessage* s = dbus_message_new_signal(PATH, IFACE, "SettingChanged");
      DBusMessageIter it;
      dbus_message_iter_init_append(s, &it);
      dbus_message_iter_append_basic(&it, DBUS_TYPE_STRING, &NS);
      dbus_message_iter_append_basic(&it, DBUS_TYPE_STRING, &KEY);
      append_value(&it, scheme, 1);
      dbus_connection_send(c, s, NULL);
      dbus_message_unref(s);
      dbus_connection_flush(c);
      printf("fake-portal: SettingChanged -> %u\n", scheme); fflush(stdout);
      next += 2;
      switchAt = argc > next ? time(NULL) + atoi(argv[next]) : 0;
    }
  }
}
