// AU smoke test without installing: load a .component, register its factory in-process,
// instantiate, initialize, render, check output is finite, count parameters, query the Cocoa UI.
#include <AudioToolbox/AudioToolbox.h>
#include <CoreFoundation/CoreFoundation.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(x, what) do { OSStatus e_ = (x); if (e_) { printf("FAIL %s: %d\n", what, (int)e_); return 1; } else printf("ok   %s\n", what); } while (0)

static float gIn[2][512];

static OSStatus InputCb(void* ref, AudioUnitRenderActionFlags* f, const AudioTimeStamp* t, UInt32 bus, UInt32 n, AudioBufferList* io)
{
  for (UInt32 c = 0; c < io->mNumberBuffers; c++)
    memcpy(io->mBuffers[c].mData, gIn[c], n * sizeof(float));
  return noErr;
}

int main(int argc, char** argv)
{
  if (argc < 2) { fprintf(stderr, "usage: au_smoke <bundle.component>\n"); return 2; }
  CFURLRef url = CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8*)argv[1], strlen(argv[1]), true);
  CFBundleRef bundle = CFBundleCreate(NULL, url);
  if (!bundle || !CFBundleLoadExecutable(bundle)) { printf("FAIL load bundle\n"); return 1; }
  printf("ok   load bundle\n");

  CFArrayRef comps = CFBundleGetValueForInfoDictionaryKey(bundle, CFSTR("AudioComponents"));
  CFDictionaryRef d = CFArrayGetValueAtIndex(comps, 0);
  char fname[256], type[8], sub[8], mfr[8];
  CFStringGetCString(CFDictionaryGetValue(d, CFSTR("factoryFunction")), fname, sizeof fname, kCFStringEncodingUTF8);
  CFStringGetCString(CFDictionaryGetValue(d, CFSTR("type")), type, sizeof type, kCFStringEncodingUTF8);
  CFStringGetCString(CFDictionaryGetValue(d, CFSTR("subtype")), sub, sizeof sub, kCFStringEncodingUTF8);
  CFStringGetCString(CFDictionaryGetValue(d, CFSTR("manufacturer")), mfr, sizeof mfr, kCFStringEncodingUTF8);
  SInt32 version = 0;
  CFNumberGetValue(CFDictionaryGetValue(d, CFSTR("version")), kCFNumberSInt32Type, &version);
  CFStringRef fn = CFStringCreateWithCString(NULL, fname, kCFStringEncodingUTF8);
  AudioComponentFactoryFunction factory = CFBundleGetFunctionPointerForName(bundle, fn);
  if (!factory) { printf("FAIL factory %s\n", fname); return 1; }
  printf("ok   factory %s (%s %s %s, version 0x%08x)\n", fname, type, sub, mfr, (unsigned)version);

  AudioComponentDescription desc = {
    .componentType = (OSType)((type[0] << 24) | (type[1] << 16) | (type[2] << 8) | type[3]),
    .componentSubType = (OSType)((sub[0] << 24) | (sub[1] << 16) | (sub[2] << 8) | sub[3]),
    .componentManufacturer = (OSType)((mfr[0] << 24) | (mfr[1] << 16) | (mfr[2] << 8) | mfr[3]),
  };
  AudioComponent comp = AudioComponentRegister(&desc, CFSTR("smoke: Sand"), (UInt32)version, factory);
  if (!comp) { printf("FAIL AudioComponentRegister\n"); return 1; }
  AudioUnit au;
  CHECK(AudioComponentInstanceNew(comp, &au), "AudioComponentInstanceNew");

  AudioStreamBasicDescription fmt = {
    .mSampleRate = 48000, .mFormatID = kAudioFormatLinearPCM,
    .mFormatFlags = kAudioFormatFlagsNativeFloatPacked | kAudioFormatFlagIsNonInterleaved,
    .mBytesPerPacket = 4, .mFramesPerPacket = 1, .mBytesPerFrame = 4, .mChannelsPerFrame = 2, .mBitsPerChannel = 32,
  };
  CHECK(AudioUnitSetProperty(au, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &fmt, sizeof fmt), "set input format 2ch 48k");
  CHECK(AudioUnitSetProperty(au, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0, &fmt, sizeof fmt), "set output format 2ch 48k");
  UInt32 maxFrames = 512;
  CHECK(AudioUnitSetProperty(au, kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global, 0, &maxFrames, sizeof maxFrames), "set max frames");
  AURenderCallbackStruct cb = { InputCb, NULL };
  CHECK(AudioUnitSetProperty(au, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &cb, sizeof cb), "set input callback");
  CHECK(AudioUnitInitialize(au), "AudioUnitInitialize");

  UInt32 sz = 0;
  CHECK(AudioUnitGetPropertyInfo(au, kAudioUnitProperty_ParameterList, kAudioUnitScope_Global, 0, &sz, NULL), "parameter list");
  printf("     %u parameters\n", (unsigned)(sz / sizeof(AudioUnitParameterID)));

  Float64 latency = 0; sz = sizeof latency;
  CHECK(AudioUnitGetProperty(au, kAudioUnitProperty_Latency, kAudioUnitScope_Global, 0, &latency, &sz), "latency");

  sz = 0; Boolean w;
  OSStatus uiErr = AudioUnitGetPropertyInfo(au, kAudioUnitProperty_CocoaUI, kAudioUnitScope_Global, 0, &sz, &w);
  printf("%s Cocoa UI property (%u bytes)\n", uiErr ? "FAIL" : "ok  ", (unsigned)sz);

  float out[2][512];
  AudioBufferList* abl = malloc(sizeof(AudioBufferList) + sizeof(AudioBuffer));
  double peak = 0; int bad = 0;
  AudioTimeStamp ts = { .mFlags = kAudioTimeStampSampleTimeValid };
  for (int block = 0; block < 200; block++) {
    for (int i = 0; i < 512; i++)
      gIn[0][i] = gIn[1][i] = 0.5f * sinf(2.f * (float)M_PI * 440.f * (float)(block * 512 + i) / 48000.f);
    abl->mNumberBuffers = 2;
    for (int c = 0; c < 2; c++) {
      abl->mBuffers[c].mNumberChannels = 1;
      abl->mBuffers[c].mDataByteSize = 512 * 4;
      abl->mBuffers[c].mData = out[c];
    }
    AudioUnitRenderActionFlags flags = 0;
    ts.mSampleTime = block * 512;
    OSStatus e = AudioUnitRender(au, &flags, &ts, 0, 512, abl);
    if (e) { printf("FAIL AudioUnitRender block %d: %d\n", block, (int)e); return 1; }
    for (int c = 0; c < 2; c++)
      for (int i = 0; i < 512; i++) {
        if (!isfinite(out[c][i])) bad++;
        if (fabs(out[c][i]) > peak) peak = fabs(out[c][i]);
      }
  }
  printf("%s render 200 x 512 frames: peak %.3f, non-finite %d\n", bad ? "FAIL" : "ok  ", peak, bad);

  CHECK(AudioUnitUninitialize(au), "AudioUnitUninitialize");
  CHECK(AudioComponentInstanceDispose(au), "AudioComponentInstanceDispose");
  return bad || uiErr ? 1 : 0;
}
