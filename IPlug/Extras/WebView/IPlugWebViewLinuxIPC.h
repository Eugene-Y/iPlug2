/*
 ==============================================================================

 This file is part of the iPlug 2 library. Copyright (C) the iPlug 2 developers.

 See LICENSE.txt for  more info.

 ==============================================================================
*/

#pragma once

/** @file
 * @brief The messages between a Linux IWebView (in the plug-in) and its WebKitGTK helper process.
 *
 * C, because the helper is a C program: it links only WebKitGTK and GTK, no C++ runtime of its own.
 * Both ends come from the same build, so integers travel in the machine's own byte order.
 *
 * A frame is a uint32 body length, then the body: a uint32 message type and its fields in order,
 * each field an int64 or a string (uint32 length, then the bytes, no terminator).
 */

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/** The socket's fd number in the helper, set up by the plug-in when it starts the helper */
#define IPLUG_WEBVIEW_HELPER_FD 3

/** The helper's file name in the plug-in's resource folder */
#define IPLUG_WEBVIEW_HELPER_NAME "iplug2-webview-helper"

/** Bump on every change below: the plug-in refuses a helper that reports another version. */
#define IPLUG_WEBVIEW_PROTOCOL_VERSION 1

enum IPlugWebViewMessage
{
  /* plug-in -> helper */
  kIPlugWVLoadHTML = 1,        /* html */
  kIPlugWVLoadURL,             /* url */
  kIPlugWVReload,
  kIPlugWVEvaluate,            /* id (0: no result wanted), script */
  kIPlugWVSetZoom,             /* zoom × 1000 */
  kIPlugWVEnableInteraction,   /* 0/1 */
  kIPlugWVNavigationReply,     /* id, allow */
  kIPlugWVMimeTypeReply,       /* id, download */
  kIPlugWVDownloadPathReply,   /* id, path */

  /* helper -> plug-in */
  kIPlugWVReady = 100,         /* protocol version, plug window id */
  kIPlugWVContentLoaded,
  kIPlugWVScriptMessage,       /* json */
  kIPlugWVEvaluateResult,      /* id, succeeded, json */
  kIPlugWVNavigationRequest,   /* id, url */
  kIPlugWVMimeTypeRequest,     /* id, mime type */
  kIPlugWVDownloadPathRequest, /* id, suggested file name */
  kIPlugWVDownloadProgress,    /* bytes received, total bytes (0: unknown) */
  kIPlugWVDownloadFinished,    /* succeeded, path */
  kIPlugWVFocusRequest,        /* X server time of the button release */
  kIPlugWVWebProcessGone
};

/** A growing byte buffer; zero-initialize it, release it with IPlugWVBufferFree() */
typedef struct
{
  char* data;
  size_t length;
  size_t capacity;
  int failed;
} IPlugWVBuffer;

static inline void IPlugWVBufferFree(IPlugWVBuffer* pBuffer)
{
  free(pBuffer->data);
  memset(pBuffer, 0, sizeof(*pBuffer));
}

static inline void IPlugWVBufferAppend(IPlugWVBuffer* pBuffer, const void* pData, size_t size)
{
  if (pBuffer->failed)
    return;
  if (pBuffer->length + size > pBuffer->capacity)
  {
    size_t capacity = pBuffer->capacity ? pBuffer->capacity : 256;
    while (capacity < pBuffer->length + size)
      capacity *= 2;
    char* pData2 = (char*) realloc(pBuffer->data, capacity);
    if (!pData2)
    {
      pBuffer->failed = 1;
      return;
    }
    pBuffer->data = pData2;
    pBuffer->capacity = capacity;
  }
  memcpy(pBuffer->data + pBuffer->length, pData, size);
  pBuffer->length += size;
}

/** Drops the first count bytes */
static inline void IPlugWVBufferConsume(IPlugWVBuffer* pBuffer, size_t count)
{
  if (count >= pBuffer->length)
  {
    pBuffer->length = 0;
    return;
  }
  memmove(pBuffer->data, pBuffer->data + count, pBuffer->length - count);
  pBuffer->length -= count;
}

/** Starts a frame; returns where it starts, for IPlugWVEndFrame() */
static inline size_t IPlugWVBeginFrame(IPlugWVBuffer* pBuffer, uint32_t type)
{
  const size_t start = pBuffer->length;
  const uint32_t placeholder = 0;
  IPlugWVBufferAppend(pBuffer, &placeholder, sizeof(placeholder));
  IPlugWVBufferAppend(pBuffer, &type, sizeof(type));
  return start;
}

static inline void IPlugWVAppendInt(IPlugWVBuffer* pBuffer, int64_t value)
{
  IPlugWVBufferAppend(pBuffer, &value, sizeof(value));
}

static inline void IPlugWVAppendString(IPlugWVBuffer* pBuffer, const char* str)
{
  const uint32_t length = str ? (uint32_t) strlen(str) : 0;
  IPlugWVBufferAppend(pBuffer, &length, sizeof(length));
  IPlugWVBufferAppend(pBuffer, str, length);
}

static inline void IPlugWVEndFrame(IPlugWVBuffer* pBuffer, size_t start)
{
  if (pBuffer->failed)
    return;
  const uint32_t bodyLength = (uint32_t) (pBuffer->length - start - sizeof(uint32_t));
  memcpy(pBuffer->data + start, &bodyLength, sizeof(bodyLength));
}

/** Reads one frame's fields in order. A short or malformed frame sets failed; reads after that return 0 / "". */
typedef struct
{
  const char* pos;
  size_t left;
  int failed;
} IPlugWVReader;

/** @return the length of the first complete frame in data (header included), or 0 if it is not complete yet */
static inline size_t IPlugWVCompleteFrameLength(const char* data, size_t length)
{
  uint32_t bodyLength;
  if (length < sizeof(bodyLength))
    return 0;
  memcpy(&bodyLength, data, sizeof(bodyLength));
  return length - sizeof(bodyLength) >= bodyLength ? sizeof(bodyLength) + bodyLength : 0;
}

/** @param frame a complete frame, as IPlugWVCompleteFrameLength() measured it */
static inline uint32_t IPlugWVReadFrameType(IPlugWVReader* pReader, const char* frame, size_t frameLength)
{
  uint32_t type = 0;
  pReader->pos = frame + sizeof(uint32_t);
  pReader->left = frameLength - sizeof(uint32_t);
  pReader->failed = pReader->left < sizeof(type);
  if (pReader->failed)
    return 0;
  memcpy(&type, pReader->pos, sizeof(type));
  pReader->pos += sizeof(type);
  pReader->left -= sizeof(type);
  return type;
}

static inline int64_t IPlugWVReadInt(IPlugWVReader* pReader)
{
  int64_t value = 0;
  if (pReader->failed || pReader->left < sizeof(value))
  {
    pReader->failed = 1;
    return 0;
  }
  memcpy(&value, pReader->pos, sizeof(value));
  pReader->pos += sizeof(value);
  pReader->left -= sizeof(value);
  return value;
}

/** @return a malloc'ed, terminated copy (never NULL unless out of memory); free() it */
static inline char* IPlugWVReadString(IPlugWVReader* pReader)
{
  uint32_t length = 0;
  if (!pReader->failed && pReader->left >= sizeof(length))
  {
    memcpy(&length, pReader->pos, sizeof(length));
    if (pReader->left - sizeof(length) < length)
      pReader->failed = 1;
  }
  else
  {
    pReader->failed = 1;
  }
  if (pReader->failed)
    length = 0;
  char* str = (char*) malloc(length + 1);
  if (!str)
    return NULL;
  if (length)
  {
    memcpy(str, pReader->pos + sizeof(length), length);
    pReader->pos += sizeof(length) + length;
    pReader->left -= sizeof(length) + length;
  }
  else if (!pReader->failed)
  {
    pReader->pos += sizeof(length);
    pReader->left -= sizeof(length);
  }
  str[length] = '\0';
  return str;
}
