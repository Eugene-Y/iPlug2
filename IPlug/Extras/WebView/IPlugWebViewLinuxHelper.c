/*
 ==============================================================================

 This file is part of the iPlug 2 library. Copyright (C) the iPlug 2 developers.

 See LICENSE.txt for  more info.

 ==============================================================================
*/

/* The WebKitGTK side of a Linux IWebView. GTK must not run inside a host's process, so each web view gets
 * this process: a GtkPlug inside the plug-in's X window (XEmbed), holding one WebKitWebView. The plug-in talks
 * to it over a socket (IPlugWebViewLinuxIPC.h) that the plug-in starts it with, as fd 3. The process ends when
 * that socket closes, which also covers a host that quits or crashes. */

#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <glib-unix.h>
#include <gtk/gtk.h>
#include <gtk/gtkx.h>
#include <webkit2/webkit2.h>

#include "IPlugWebViewLinuxIPC.h"

typedef struct
{
  unsigned long parent;
  int width;
  int height;
  double zoom;
  gboolean opaque;
  gboolean devTools;
  const char* scheme;
  const char* dataDir;
} Options;

typedef struct
{
  Options options;
  GtkWidget* plug;
  WebKitWebView* view;
  int fd;
  IPlugWVBuffer in;
  IPlugWVBuffer out;
  guint outWatch;
  gint64 nextId;
  GHashTable* pendingDecisions; /* id -> WebKitPolicyDecision */
  GHashTable* pendingDownloads; /* id -> WebKitDownload */
  GPtrArray* schemeRoots; /* the folders the plug-in loaded pages from with the custom scheme, resolved */
  gboolean loadFailed;
} Helper;

static Helper sHelper;

static void Quit(void)
{
  static gboolean quitting = FALSE;
  if (quitting)
    return;
  quitting = TRUE;
  /* A script dialog runs a nested loop, which gtk_main_quit() would only leave. */
  if (gtk_main_level() > 1)
    _exit(0);
  /* WebKit's own processes end with their connection to us; the alarm ends us should their shutdown hang. */
  alarm(3);
  gtk_main_quit();
}

/* ---------------------------------------------------------------------------------------------- socket */

static gboolean OnSocketWritable(gint fd, GIOCondition condition, gpointer data);

static void Flush(void)
{
  Helper* h = &sHelper;
  while (h->out.length > 0)
  {
    const ssize_t written = write(h->fd, h->out.data, h->out.length);
    if (written > 0)
    {
      IPlugWVBufferConsume(&h->out, (size_t) written);
      continue;
    }
    if (written < 0 && errno == EINTR)
      continue;
    if (written < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
    {
      if (!h->outWatch)
        h->outWatch = g_unix_fd_add(h->fd, G_IO_OUT, OnSocketWritable, NULL);
      return;
    }
    Quit();
    return;
  }
}

static gboolean OnSocketWritable(gint fd, GIOCondition condition, gpointer data)
{
  sHelper.outWatch = 0;
  Flush();
  return G_SOURCE_REMOVE;
}

static void SendIntMessage(uint32_t type, int64_t value)
{
  const size_t frame = IPlugWVBeginFrame(&sHelper.out, type);
  IPlugWVAppendInt(&sHelper.out, value);
  IPlugWVEndFrame(&sHelper.out, frame);
  Flush();
}

static void SendIdAndString(uint32_t type, int64_t id, const char* str)
{
  const size_t frame = IPlugWVBeginFrame(&sHelper.out, type);
  IPlugWVAppendInt(&sHelper.out, id);
  IPlugWVAppendString(&sHelper.out, str);
  IPlugWVEndFrame(&sHelper.out, frame);
  Flush();
}

static gint64 Remember(GHashTable* pTable, gpointer object)
{
  const gint64 id = ++sHelper.nextId;
  g_hash_table_insert(pTable, g_memdup2(&id, sizeof(id)), g_object_ref(object));
  return id;
}

/** @return the object, with the reference Remember() took; NULL for an id that is not pending */
static gpointer Forget(GHashTable* pTable, gint64 id)
{
  gpointer key = NULL;
  gpointer object = NULL;
  if (!g_hash_table_steal_extended(pTable, &id, &key, &object))
    return NULL;
  g_free(key);
  return object;
}

static void OnEvaluated(GObject* source, GAsyncResult* result, gpointer data);

/** @return the file path a <scheme>:<path> URL names, resolved (no "..", no symlinks); NULL if it does not exist */
static char* ResolveSchemePath(const char* escapedPath)
{
  char* path = escapedPath ? g_uri_unescape_string(escapedPath, NULL) : NULL;
  char* resolved = path && path[0] == '/' ? realpath(path, NULL) : NULL;
  g_free(path);
  return resolved;
}

/* The custom scheme serves only the folders of the pages the plug-in itself loads with it (LoadFile, LoadURL), and
 * what is below them. Page content cannot add one: a page that is not the plug-in's, e.g. a sandboxed iframe of
 * remote content (its origin is "null", as the scheme's own pages'), must not read the user's files. */
static void AllowSchemeFolderOf(const char* url)
{
  const char* scheme = sHelper.options.scheme;
  const size_t schemeLength = scheme ? strlen(scheme) : 0;
  if (!schemeLength || strncmp(url, scheme, schemeLength) != 0 || url[schemeLength] != ':')
    return;
  char* escapedPath = g_strdup(url + schemeLength + 1);
  escapedPath[strcspn(escapedPath, "?#")] = '\0';
  char* folder = g_path_get_dirname(escapedPath);
  char* resolved = ResolveSchemePath(folder);
  g_free(folder);
  g_free(escapedPath);
  if (!resolved)
    return;
  for (guint i = 0; i < sHelper.schemeRoots->len; i++)
  {
    if (!strcmp(g_ptr_array_index(sHelper.schemeRoots, i), resolved))
    {
      free(resolved);
      return;
    }
  }
  g_ptr_array_add(sHelper.schemeRoots, resolved);
}

static gboolean IsInSchemeFolder(const char* path)
{
  for (guint i = 0; i < sHelper.schemeRoots->len; i++)
  {
    const char* root = g_ptr_array_index(sHelper.schemeRoots, i);
    const size_t length = strlen(root);
    if (!strncmp(path, root, length) && (path[length] == '/' || (length == 1 && root[0] == '/')))
      return TRUE;
  }
  return FALSE;
}

static void HandleMessage(uint32_t type, IPlugWVReader* r)
{
  Helper* h = &sHelper;
  switch (type)
  {
    case kIPlugWVLoadHTML:
    {
      char* html = IPlugWVReadString(r);
      if (html)
        webkit_web_view_load_html(h->view, html, NULL);
      free(html);
      break;
    }
    case kIPlugWVLoadURL:
    {
      char* url = IPlugWVReadString(r);
      if (url)
      {
        AllowSchemeFolderOf(url);
        webkit_web_view_load_uri(h->view, url);
      }
      free(url);
      break;
    }
    case kIPlugWVReload:
      webkit_web_view_reload(h->view);
      break;
    case kIPlugWVEvaluate:
    {
      const int64_t id = IPlugWVReadInt(r);
      char* script = IPlugWVReadString(r);
      if (script && !r->failed)
      {
        gint64* pId = g_new(gint64, 1);
        *pId = id;
        webkit_web_view_evaluate_javascript(h->view, script, -1, NULL, NULL, NULL, OnEvaluated, pId);
      }
      free(script);
      break;
    }
    case kIPlugWVSetZoom:
    {
      const int64_t zoom = IPlugWVReadInt(r);
      if (zoom > 0)
        webkit_web_view_set_zoom_level(h->view, (double) zoom / 1000.0);
      break;
    }
    case kIPlugWVEnableInteraction:
    {
      GdkWindow* pWindow = gtk_widget_get_window(h->plug);
      if (!pWindow)
        break;
      if (IPlugWVReadInt(r))
      {
        gdk_window_input_shape_combine_region(pWindow, NULL, 0, 0);
      }
      else
      {
        /* An empty input shape: clicks reach the plug-in's window underneath, as with mac's hitTest:. */
        cairo_region_t* pEmpty = cairo_region_create();
        gdk_window_input_shape_combine_region(pWindow, pEmpty, 0, 0);
        cairo_region_destroy(pEmpty);
      }
      break;
    }
    case kIPlugWVNavigationReply:
    case kIPlugWVMimeTypeReply:
    {
      const int64_t id = IPlugWVReadInt(r);
      const int64_t yes = IPlugWVReadInt(r);
      WebKitPolicyDecision* pDecision = Forget(h->pendingDecisions, id);
      if (!pDecision)
        break;
      if (type == kIPlugWVNavigationReply)
        yes ? webkit_policy_decision_use(pDecision) : webkit_policy_decision_ignore(pDecision);
      else
        yes ? webkit_policy_decision_download(pDecision) : webkit_policy_decision_use(pDecision);
      g_object_unref(pDecision);
      break;
    }
    case kIPlugWVDownloadPathReply:
    {
      const int64_t id = IPlugWVReadInt(r);
      char* path = IPlugWVReadString(r);
      WebKitDownload* pDownload = Forget(h->pendingDownloads, id);
      if (pDownload)
      {
        /* The 4.1 API takes a URI here; only WebKitGTK 6 takes a path. */
        char* uri = path && path[0] == '/' && !r->failed ? g_filename_to_uri(path, NULL, NULL) : NULL;
        if (uri)
          webkit_download_set_destination(pDownload, uri);
        else
          webkit_download_cancel(pDownload);
        g_free(uri);
        g_object_unref(pDownload);
      }
      free(path);
      break;
    }
    default:
      break;
  }
}

static gboolean OnSocketReadable(gint fd, GIOCondition condition, gpointer data)
{
  Helper* h = &sHelper;
  for (;;)
  {
    char chunk[65536];
    const ssize_t count = read(fd, chunk, sizeof(chunk));
    if (count > 0)
    {
      IPlugWVBufferAppend(&h->in, chunk, (size_t) count);
      continue;
    }
    if (count < 0 && errno == EINTR)
      continue;
    if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
      break;
    /* End of file: the plug-in closed the web view, or its host is gone. */
    Quit();
    return G_SOURCE_REMOVE;
  }
  if (h->in.failed)
  {
    Quit();
    return G_SOURCE_REMOVE;
  }

  size_t frameLength;
  while ((frameLength = IPlugWVCompleteFrameLength(h->in.data, h->in.length)) > 0)
  {
    IPlugWVReader reader;
    const uint32_t type = IPlugWVReadFrameType(&reader, h->in.data, frameLength);
    HandleMessage(type, &reader);
    IPlugWVBufferConsume(&h->in, frameLength);
  }
  return G_SOURCE_CONTINUE;
}

/* ------------------------------------------------------------------------------------------ JavaScript */

/** @return the value as JSON, g_free() it; "null" for values JSON has no form for (undefined, functions) */
static char* ToJSON(JSCValue* pValue)
{
  char* json = pValue ? jsc_value_to_json(pValue, 0) : NULL;
  return json ? json : g_strdup("null");
}

static void OnEvaluated(GObject* source, GAsyncResult* result, gpointer data)
{
  gint64* pId = data;
  GError* pError = NULL;
  JSCValue* pValue = webkit_web_view_evaluate_javascript_finish(WEBKIT_WEB_VIEW(source), result, &pError);
  if (*pId != 0)
  {
    char* json = pValue ? ToJSON(pValue) : NULL;
    const size_t frame = IPlugWVBeginFrame(&sHelper.out, kIPlugWVEvaluateResult);
    IPlugWVAppendInt(&sHelper.out, *pId);
    IPlugWVAppendInt(&sHelper.out, pValue != NULL);
    IPlugWVAppendString(&sHelper.out, json);
    IPlugWVEndFrame(&sHelper.out, frame);
    Flush();
    g_free(json);
  }
  if (pError)
    g_error_free(pError);
  if (pValue)
    g_object_unref(pValue);
  g_free(pId);
}

static void OnScriptMessage(WebKitUserContentManager* pManager, WebKitJavascriptResult* pResult, gpointer data)
{
  char* json = ToJSON(webkit_javascript_result_get_js_value(pResult));
  const size_t frame = IPlugWVBeginFrame(&sHelper.out, kIPlugWVScriptMessage);
  IPlugWVAppendString(&sHelper.out, json);
  IPlugWVEndFrame(&sHelper.out, frame);
  Flush();
  g_free(json);
}

/* ---------------------------------------------------------------------------------- loads and policies */

static void OnLoadChanged(WebKitWebView* pView, WebKitLoadEvent event, gpointer data)
{
  if (event == WEBKIT_LOAD_STARTED)
    sHelper.loadFailed = FALSE;
  else if (event == WEBKIT_LOAD_FINISHED && !sHelper.loadFailed)
    SendIntMessage(kIPlugWVContentLoaded, 0);
}

static gboolean OnLoadFailed(WebKitWebView* pView, WebKitLoadEvent event, char* uri, GError* pError, gpointer data)
{
  sHelper.loadFailed = TRUE;
  return FALSE;
}

/* Through xdg-open, as IGraphics' OpenURL() does: GIO's own default-handler lookup falls back to any app that takes
 * text/html (seen: a text editor) when no handler for https is registered, e.g. with a snap browser. */
static void OpenInBrowser(const char* uri)
{
  const char* argv[] = { "xdg-open", uri, NULL };
  GError* pError = NULL;
  if (!g_spawn_async(NULL, (char**) argv, NULL, G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL, NULL, NULL, NULL, &pError))
  {
    fprintf(stderr, "iplug2-webview-helper: cannot open %s: %s\n", uri, pError ? pError->message : "");
    g_clear_error(&pError);
  }
}

static gboolean OnDecidePolicy(WebKitWebView* pView, WebKitPolicyDecision* pDecision, WebKitPolicyDecisionType type, gpointer data)
{
  Helper* h = &sHelper;
  switch (type)
  {
    case WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION:
    {
      WebKitNavigationAction* pAction = webkit_navigation_policy_decision_get_navigation_action(WEBKIT_NAVIGATION_POLICY_DECISION(pDecision));
      const char* uri = webkit_uri_request_get_uri(webkit_navigation_action_get_request(pAction));
      SendIdAndString(kIPlugWVNavigationRequest, Remember(h->pendingDecisions, pDecision), uri);
      return TRUE;
    }
    case WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION:
    {
      /* As on mac and Windows: links to a new window open in the user's browser. */
      WebKitNavigationAction* pAction = webkit_navigation_policy_decision_get_navigation_action(WEBKIT_NAVIGATION_POLICY_DECISION(pDecision));
      OpenInBrowser(webkit_uri_request_get_uri(webkit_navigation_action_get_request(pAction)));
      webkit_policy_decision_ignore(pDecision);
      return TRUE;
    }
    case WEBKIT_POLICY_DECISION_TYPE_RESPONSE:
    {
      WebKitURIResponse* pResponse = webkit_response_policy_decision_get_response(WEBKIT_RESPONSE_POLICY_DECISION(pDecision));
      const char* mimeType = webkit_uri_response_get_mime_type(pResponse);
      SendIdAndString(kIPlugWVMimeTypeRequest, Remember(h->pendingDecisions, pDecision), mimeType ? mimeType : "");
      return TRUE;
    }
    default:
      return FALSE;
  }
}

static gboolean OnContextMenu(WebKitWebView* pView, WebKitContextMenu* pMenu, GdkEvent* pEvent, WebKitHitTestResult* pHit, gpointer data)
{
  /* As WebView2: the default menu (with Inspect Element) only with the developer tools. */
  return !sHelper.options.devTools;
}

static void OnWebProcessTerminated(WebKitWebView* pView, WebKitWebProcessTerminationReason reason, gpointer data)
{
  SendIntMessage(kIPlugWVWebProcessGone, (int64_t) reason);
}

/* Sees every event before GTK does, so also the ones WebKit's gestures take. GtkPlug asks its embedder for the
 * keyboard focus only when its focus widget changes, which a click into an already focused web view does not do,
 * and the host can have taken the X focus back meanwhile; without it, typing into the page goes nowhere. */
static void OnGdkEvent(GdkEvent* pEvent, gpointer data)
{
  /* Only the embedded page's: the page's own dialogs and the inspector are top-level windows of their own. */
  if (pEvent->type == GDK_BUTTON_RELEASE && sHelper.plug && gdk_event_get_window(pEvent)
      && gdk_window_get_toplevel(gdk_event_get_window(pEvent)) == gtk_widget_get_window(sHelper.plug))
    SendIntMessage(kIPlugWVFocusRequest, (int64_t) gdk_event_get_time(pEvent));
  gtk_main_do_event(pEvent);
}

/* -------------------------------------------------------------------------------------------- downloads */

static gboolean OnDecideDestination(WebKitDownload* pDownload, char* suggestedName, gpointer data)
{
  SendIdAndString(kIPlugWVDownloadPathRequest, Remember(sHelper.pendingDownloads, pDownload), suggestedName ? suggestedName : "download");
  return TRUE;
}

static void OnDownloadData(WebKitDownload* pDownload, guint64 length, gpointer data)
{
  WebKitURIResponse* pResponse = webkit_download_get_response(pDownload);
  const size_t frame = IPlugWVBeginFrame(&sHelper.out, kIPlugWVDownloadProgress);
  IPlugWVAppendInt(&sHelper.out, (int64_t) webkit_download_get_received_data_length(pDownload));
  IPlugWVAppendInt(&sHelper.out, pResponse ? (int64_t) webkit_uri_response_get_content_length(pResponse) : 0);
  IPlugWVEndFrame(&sHelper.out, frame);
  Flush();
}

static void OnDownloadFailed(WebKitDownload* pDownload, GError* pError, gpointer data)
{
  g_object_set_data(G_OBJECT(pDownload), "iplug-failed", GINT_TO_POINTER(1));
}

static void OnDownloadFinished(WebKitDownload* pDownload, gpointer data)
{
  const gboolean failed = g_object_get_data(G_OBJECT(pDownload), "iplug-failed") != NULL;
  const char* destination = webkit_download_get_destination(pDownload);
  char* path = destination ? g_filename_from_uri(destination, NULL, NULL) : NULL;
  const size_t frame = IPlugWVBeginFrame(&sHelper.out, kIPlugWVDownloadFinished);
  IPlugWVAppendInt(&sHelper.out, !failed);
  IPlugWVAppendString(&sHelper.out, path ? path : "");
  IPlugWVEndFrame(&sHelper.out, frame);
  Flush();
  g_free(path);
}

static void OnDownloadStarted(WebKitWebContext* pContext, WebKitDownload* pDownload, gpointer data)
{
  g_signal_connect(pDownload, "decide-destination", G_CALLBACK(OnDecideDestination), NULL);
  g_signal_connect(pDownload, "received-data", G_CALLBACK(OnDownloadData), NULL);
  g_signal_connect(pDownload, "failed", G_CALLBACK(OnDownloadFailed), NULL);
  g_signal_connect(pDownload, "finished", G_CALLBACK(OnDownloadFinished), NULL);
}

/* ------------------------------------------------------------------------------------ custom URL scheme */

static const char* MimeTypeForPath(const char* path)
{
  static const char* const kTypes[][2] = {
    { ".html", "text/html" }, { ".htm", "text/html" }, { ".css", "text/css" },
    { ".js", "text/javascript" }, { ".mjs", "text/javascript" }, { ".json", "application/json" },
    { ".svg", "image/svg+xml" }, { ".png", "image/png" }, { ".jpg", "image/jpeg" }, { ".jpeg", "image/jpeg" },
    { ".gif", "image/gif" }, { ".webp", "image/webp" }, { ".ico", "image/x-icon" },
    { ".wasm", "application/wasm" }, { ".woff", "font/woff" }, { ".woff2", "font/woff2" }, { ".ttf", "font/ttf" },
    { ".otf", "font/otf" }, { ".wav", "audio/wav" }, { ".mp3", "audio/mpeg" }, { ".txt", "text/plain" }
  };
  const char* dot = strrchr(path, '.');
  if (dot)
  {
    for (size_t i = 0; i < G_N_ELEMENTS(kTypes); i++)
    {
      if (g_ascii_strcasecmp(dot, kTypes[i][0]) == 0)
        return kTypes[i][1];
    }
  }
  return "application/octet-stream";
}

static void FinishWithStatus(WebKitURISchemeRequest* pRequest, guint status, const char* reason)
{
  GInputStream* pEmpty = g_memory_input_stream_new();
  WebKitURISchemeResponse* pResponse = webkit_uri_scheme_response_new(pEmpty, 0);
  webkit_uri_scheme_response_set_status(pResponse, status, reason);
  webkit_uri_scheme_request_finish_with_response(pRequest, pResponse);
  g_object_unref(pResponse);
  g_object_unref(pEmpty);
}

/* <scheme>:<absolute path> serves that file, as mac's WKURLSchemeHandler does, if it is in one of the plug-in's web
 * folders: LoadFile() names the page that way, and relative URLs in it resolve to the files beside it. */
static void OnSchemeRequest(WebKitURISchemeRequest* pRequest, gpointer data)
{
  char* path = ResolveSchemePath(webkit_uri_scheme_request_get_path(pRequest));
  char* contents = NULL;
  gsize length = 0;
  GError* pError = NULL;
  if (path && !IsInSchemeFolder(path))
  {
    FinishWithStatus(pRequest, 403, "Forbidden");
    free(path);
    return;
  }
  if (!path || !g_file_get_contents(path, &contents, &length, &pError))
  {
    if (!pError)
      pError = g_error_new_literal(G_IO_ERROR, G_IO_ERROR_NOT_FOUND, "not found");
    webkit_uri_scheme_request_finish_error(pRequest, pError);
    g_error_free(pError);
    free(path);
    return;
  }

  GInputStream* pStream = g_memory_input_stream_new_from_data(contents, (gssize) length, g_free);
  WebKitURISchemeResponse* pResponse = webkit_uri_scheme_response_new(pStream, (gint64) length);
  webkit_uri_scheme_response_set_content_type(pResponse, MimeTypeForPath(path));
  /* Such URLs have no host, so their origin is opaque ("null"): module scripts and fetch() count every load as
   * cross-origin. Only that origin may read the files; other opaque-origin documents (sandboxed iframes) share it,
   * which is why the files are only the plug-in's own (IsInSchemeFolder). */
  SoupMessageHeaders* pHeaders = soup_message_headers_new(SOUP_MESSAGE_HEADERS_RESPONSE);
  soup_message_headers_append(pHeaders, "Access-Control-Allow-Origin", "null");
  webkit_uri_scheme_response_set_http_headers(pResponse, pHeaders);
  webkit_uri_scheme_request_finish_with_response(pRequest, pResponse);
  g_object_unref(pResponse);
  g_object_unref(pStream);
  free(path);
}

/* ------------------------------------------------------------------------------------------------ setup */

static const char* const kSendMessageScript = "function IPlugSendMsg(m) { window.webkit.messageHandlers.callback.postMessage(m); }";

static const char* const kKeyScript =
  "document.addEventListener('keydown', function(e) { if(document.activeElement.type != \"text\") { IPlugSendMsg({'msg': 'SKPFUI', 'keyCode': e.keyCode, 'utf8': e.key, 'S': e.shiftKey, 'C': e.ctrlKey, 'A': e.altKey, 'isUp': false}); }});"
  "document.addEventListener('keyup', function(e) { if(document.activeElement.type != \"text\") { IPlugSendMsg({'msg': 'SKPFUI', 'keyCode': e.keyCode, 'utf8': e.key, 'S': e.shiftKey, 'C': e.ctrlKey, 'A': e.altKey, 'isUp': true}); }});";

static void AddUserScript(WebKitUserContentManager* pManager, const char* source)
{
  WebKitUserScript* pScript = webkit_user_script_new(source, WEBKIT_USER_CONTENT_INJECT_TOP_FRAME,
                                                     WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START, NULL, NULL);
  webkit_user_content_manager_add_script(pManager, pScript);
  webkit_user_script_unref(pScript);
}

static WebKitWebContext* MakeContext(const Options* pOptions)
{
  WebKitWebContext* pContext;
  if (pOptions->dataDir && pOptions->dataDir[0])
  {
    char* data = g_build_filename(pOptions->dataDir, "data", NULL);
    char* cache = g_build_filename(pOptions->dataDir, "cache", NULL);
    WebKitWebsiteDataManager* pData = webkit_website_data_manager_new("base-data-directory", data, "base-cache-directory", cache, NULL);
    pContext = webkit_web_context_new_with_website_data_manager(pData);
    g_object_unref(pData);
    g_free(data);
    g_free(cache);
  }
  else
  {
    pContext = webkit_web_context_new_ephemeral();
  }

  if (pOptions->scheme && pOptions->scheme[0])
  {
    webkit_web_context_register_uri_scheme(pContext, pOptions->scheme, OnSchemeRequest, NULL, NULL);
    WebKitSecurityManager* pSecurity = webkit_web_context_get_security_manager(pContext);
    webkit_security_manager_register_uri_scheme_as_secure(pSecurity, pOptions->scheme);
    webkit_security_manager_register_uri_scheme_as_cors_enabled(pSecurity, pOptions->scheme);
  }
  g_signal_connect(pContext, "download-started", G_CALLBACK(OnDownloadStarted), NULL);
  return pContext;
}

static WebKitWebView* MakeView(const Options* pOptions)
{
  WebKitUserContentManager* pManager = webkit_user_content_manager_new();
  AddUserScript(pManager, kSendMessageScript);
  AddUserScript(pManager, kKeyScript);
  g_signal_connect(pManager, "script-message-received::callback", G_CALLBACK(OnScriptMessage), NULL);
  webkit_user_content_manager_register_script_message_handler(pManager, "callback");

  WebKitWebContext* pContext = MakeContext(pOptions);
  WebKitWebView* pView = WEBKIT_WEB_VIEW(g_object_new(WEBKIT_TYPE_WEB_VIEW, "web-context", pContext, "user-content-manager", pManager, NULL));
  g_object_unref(pContext);
  g_object_unref(pManager);

  WebKitSettings* pSettings = webkit_web_view_get_settings(pView);
  webkit_settings_set_enable_developer_extras(pSettings, pOptions->devTools);
  /* With the developer tools on, the page's console also goes to the host's stdout, the only place a plug-in
   * developer on Linux sees it without opening the inspector. */
  webkit_settings_set_enable_write_console_messages_to_stdout(pSettings, pOptions->devTools);
  webkit_settings_set_javascript_can_access_clipboard(pSettings, TRUE);
  webkit_settings_set_allow_file_access_from_file_urls(pSettings, TRUE);
  webkit_settings_set_allow_universal_access_from_file_urls(pSettings, TRUE);
  if (!pOptions->opaque)
  {
    const GdkRGBA transparent = { 0.0, 0.0, 0.0, 0.0 };
    webkit_web_view_set_background_color(pView, &transparent);
  }
  if (pOptions->zoom > 0.0)
    webkit_web_view_set_zoom_level(pView, pOptions->zoom);

  g_signal_connect(pView, "load-changed", G_CALLBACK(OnLoadChanged), NULL);
  g_signal_connect(pView, "load-failed", G_CALLBACK(OnLoadFailed), NULL);
  g_signal_connect(pView, "decide-policy", G_CALLBACK(OnDecidePolicy), NULL);
  g_signal_connect(pView, "context-menu", G_CALLBACK(OnContextMenu), NULL);
  g_signal_connect(pView, "web-process-terminated", G_CALLBACK(OnWebProcessTerminated), NULL);
  return pView;
}

static gboolean ParseOptions(int argc, char** argv, Options* pOptions)
{
  memset(pOptions, 0, sizeof(*pOptions));
  pOptions->opaque = TRUE;
  for (int i = 1; i + 1 < argc; i += 2)
  {
    const char* name = argv[i];
    const char* value = argv[i + 1];
    if (!strcmp(name, "--parent"))
      pOptions->parent = strtoul(value, NULL, 0);
    else if (!strcmp(name, "--width"))
      pOptions->width = atoi(value);
    else if (!strcmp(name, "--height"))
      pOptions->height = atoi(value);
    else if (!strcmp(name, "--zoom-permille"))
      pOptions->zoom = atoi(value) / 1000.0;
    else if (!strcmp(name, "--opaque"))
      pOptions->opaque = atoi(value) != 0;
    else if (!strcmp(name, "--devtools"))
      pOptions->devTools = atoi(value) != 0;
    else if (!strcmp(name, "--scheme"))
      pOptions->scheme = value;
    else if (!strcmp(name, "--data-dir"))
      pOptions->dataDir = value;
  }
  return pOptions->parent != 0;
}

static void OnPlugDestroyed(GtkWidget* pWidget, gpointer data)
{
  /* Unembedded (the plug-in closed the web view) or its window went away with the host's. */
  sHelper.plug = NULL;
  Quit();
}

int main(int argc, char** argv)
{
  Helper* h = &sHelper;
  if (!ParseOptions(argc, argv, &h->options))
  {
    fprintf(stderr, "iplug2-webview-helper: started without --parent; it is started by an iPlug2 plug-in\n");
    return 2;
  }
  h->fd = IPLUG_WEBVIEW_HELPER_FD;
  if (fcntl(h->fd, F_SETFL, fcntl(h->fd, F_GETFL) | O_NONBLOCK) < 0)
    return 2;

  /* XEmbed exists only on X11; in a Wayland session the host's windows are Xwayland ones anyway. */
  setenv("GDK_BACKEND", "x11", 1);
  if (!gtk_init_check(&argc, &argv))
  {
    fprintf(stderr, "iplug2-webview-helper: cannot open the X display\n");
    return 3;
  }

  /* The plug-in scales the page with the zoom level; WebKit would scale its text by the desktop's font DPI
   * (Xft.dpi) on top of that. */
  g_object_set(gtk_settings_get_default(), "gtk-xft-dpi", 96 * 1024, NULL);

  h->pendingDecisions = g_hash_table_new_full(g_int64_hash, g_int64_equal, g_free, g_object_unref);
  h->pendingDownloads = g_hash_table_new_full(g_int64_hash, g_int64_equal, g_free, g_object_unref);
  h->schemeRoots = g_ptr_array_new_with_free_func(free);
  gdk_event_handler_set(OnGdkEvent, NULL, NULL);
  h->view = MakeView(&h->options);
  h->plug = gtk_plug_new((Window) h->options.parent);
  gtk_window_set_default_size(GTK_WINDOW(h->plug), MAX(1, h->options.width), MAX(1, h->options.height));
  gtk_container_add(GTK_CONTAINER(h->plug), GTK_WIDGET(h->view));
  g_signal_connect(h->plug, "destroy", G_CALLBACK(OnPlugDestroyed), NULL);
  gtk_widget_show_all(h->plug);

  g_unix_fd_add(h->fd, G_IO_IN | G_IO_HUP | G_IO_ERR, OnSocketReadable, NULL);
  const size_t frame = IPlugWVBeginFrame(&h->out, kIPlugWVReady);
  IPlugWVAppendInt(&h->out, IPLUG_WEBVIEW_PROTOCOL_VERSION);
  IPlugWVAppendInt(&h->out, (int64_t) gtk_plug_get_id(GTK_PLUG(h->plug)));
  IPlugWVEndFrame(&h->out, frame);
  Flush();

  gtk_main();

  if (h->plug)
    gtk_widget_destroy(h->plug);
  g_hash_table_destroy(h->pendingDecisions);
  g_hash_table_destroy(h->pendingDownloads);
  g_ptr_array_unref(h->schemeRoots);
  return 0;
}
