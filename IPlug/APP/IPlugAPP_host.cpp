/*
 ==============================================================================
 
 This file is part of the iPlug 2 library. Copyright (C) the iPlug 2 developers. 
 
 See LICENSE.txt for  more info.
 
 ==============================================================================
*/

#include "IPlugAPP_host.h"

#ifdef OS_WIN
#include <sys/stat.h>
#include "win32_utf8.h"
#endif

#ifdef OS_LINUX
#include <gdk/gdkx.h>
#include <algorithm>
#include <filesystem>
#include "IPlugPaths.h"
#endif

#include "IPlugLogger.h"

using namespace iplug;

#ifndef MAX_PATH_LEN
#define MAX_PATH_LEN 2048
#endif

#define STRBUFSZ 100

std::unique_ptr<IPlugAPPHost> IPlugAPPHost::sInstance;
UINT gSCROLLMSG;

IPlugAPPHost::IPlugAPPHost()
: mIPlug(MakePlug(InstanceInfo{this}))
{
}

IPlugAPPHost::~IPlugAPPHost()
{
  mExiting = true;
  
  CloseAudio();
  
  if (mMidiIn)
    mMidiIn->cancelCallback();

  if (mMidiOut)
    mMidiOut->closePort();
}

//static
IPlugAPPHost* IPlugAPPHost::Create()
{
  sInstance = std::make_unique<IPlugAPPHost>();
  return sInstance.get();
}

bool IPlugAPPHost::Init()
{
  mIPlug->SetHost("standalone", mIPlug->GetPluginVersion(false));
    
  if (!InitState())
    return false;
  
  TryToChangeAudioDriverType(); // will init RTAudio with an API type based on gState->mAudioDriverType
  ProbeAudioIO(); // find out what audio IO devs are available and put their IDs in the global variables gAudioInputDevs / gAudioOutputDevs
  InitMidi(); // creates RTMidiIn and RTMidiOut objects
  ProbeMidiIO(); // find out what midi IO devs are available and put their names in the global variables gMidiInputDevs / gMidiOutputDevs
  SelectMIDIDevice(ERoute::kInput, mState.mMidiInDev.Get());
  SelectMIDIDevice(ERoute::kOutput, mState.mMidiOutDev.Get());
  
  mIPlug->OnParamReset(kReset);
  mIPlug->OnActivate(true);
  
  return true;
}

bool IPlugAPPHost::OpenWindow(HWND pParent)
{
#ifdef OS_LINUX
  // pParent is a SWELL HWND (heap pointer, not an X11 Window).
  // Extract the underlying X11 window so IGraphicsLinux can embed into it.
  GdkWindow* gdkWnd = pParent ? static_cast<GdkWindow*>(SWELL_GetOSWindow(pParent, "GdkWindow")) : nullptr;
  void* x11Parent = gdkWnd ? (void*)(uintptr_t)GDK_WINDOW_XID(gdkWnd) : nullptr;
  void* pView = mIPlug->OpenWindow(x11Parent);
  if (pView && gdkWnd)
  {
    // SWELL draws the menu bar into the same X window, so the editor's child window goes below it.
    POINT clientOrigin { 0, 0 };
    ClientToScreen(pParent, &clientOrigin);
    RECT windowRect;
    GetWindowRect(pParent, &windowRect);
    Display* pDisplay = GDK_WINDOW_XDISPLAY(gdkWnd);
    XMoveWindow(pDisplay, (Window)(uintptr_t)pView, clientOrigin.x - windowRect.left, clientOrigin.y - windowRect.top);
    XFlush(pDisplay);
  }
  return pView != nullptr;
#else
  return mIPlug->OpenWindow(pParent) != nullptr;
#endif
}

void IPlugAPPHost::CloseWindow()
{
  mIPlug->CloseWindow();
}

bool IPlugAPPHost::InitState()
{
#if defined OS_WIN
  char strPath[MAX_PATH_LEN];
  SHGetSpecialFolderPathUTF8(NULL, strPath, MAX_PATH_LEN, CSIDL_LOCAL_APPDATA, FALSE);
  mINIPath.SetFormatted(MAX_PATH_LEN, "%s\\%s\\", strPath, BUNDLE_NAME);
#elif defined OS_MAC
  mINIPath.SetFormatted(MAX_PATH_LEN, "%s/Library/Application Support/%s/", getenv("HOME"), BUNDLE_NAME);
#elif defined OS_LINUX
  // Without a settings folder the APP still runs, on defaults that are not saved.
  INIPath(mINIPath, BUNDLE_NAME);
  std::error_code error;
  if (mINIPath.GetLength())
    std::filesystem::create_directories(mINIPath.Get(), error);
  else
    error = std::make_error_code(std::errc::no_such_file_or_directory);

  if (error)
  {
    fprintf(stderr, "%s: settings will not be saved, no settings folder '%s': %s\n", BUNDLE_NAME, mINIPath.Get(), error.message().c_str());
    mINIPath.Set("");
    return true;
  }

  mINIPath.Append("/");
#endif

  struct stat st;

  if (stat(mINIPath.Get(), &st) == 0) // if directory exists
  {
    mINIPath.Append("settings.ini"); // add file name to path

    char buf[STRBUFSZ];
    
    if (stat(mINIPath.Get(), &st) == 0) // if settings file exists read values into state
    {
      DBGMSG("Reading ini file from %s\n", mINIPath.Get());
      
      mState.mAudioDriverType = GetPrivateProfileInt("audio", "driver", mState.mAudioDriverType, mINIPath.Get());

      GetPrivateProfileString("audio", "indev", "Built-in Input", buf, STRBUFSZ, mINIPath.Get()); mState.mAudioInDev.Set(buf);
      GetPrivateProfileString("audio", "outdev", "Built-in Output", buf, STRBUFSZ, mINIPath.Get()); mState.mAudioOutDev.Set(buf);

      //audio
      mState.mAudioInChanL = GetPrivateProfileInt("audio", "in1", 1, mINIPath.Get()); // 1 is first audio input
      mState.mAudioInChanR = GetPrivateProfileInt("audio", "in2", 2, mINIPath.Get());
      mState.mAudioOutChanL = GetPrivateProfileInt("audio", "out1", 1, mINIPath.Get()); // 1 is first audio output
      mState.mAudioOutChanR = GetPrivateProfileInt("audio", "out2", 2, mINIPath.Get());
      //mState.mAudioInIsMono = GetPrivateProfileInt("audio", "monoinput", 0, mINIPath.Get());

      mState.mBufferSize = GetPrivateProfileInt("audio", "buffer", 512, mINIPath.Get());
      mState.mAudioSR = GetPrivateProfileInt("audio", "sr", 44100, mINIPath.Get());

      //midi
      GetPrivateProfileString("midi", "indev", "no input", buf, STRBUFSZ, mINIPath.Get()); mState.mMidiInDev.Set(buf);
      GetPrivateProfileString("midi", "outdev", "no output", buf, STRBUFSZ, mINIPath.Get()); mState.mMidiOutDev.Set(buf);

      mState.mMidiInChan = GetPrivateProfileInt("midi", "inchan", 0, mINIPath.Get()); // 0 is any
      mState.mMidiOutChan = GetPrivateProfileInt("midi", "outchan", 0, mINIPath.Get()); // 1 is first chan
    }

    // if settings file doesn't exist, populate with default values, otherwise overwrite
    UpdateINI();
  }
  else // folder doesn't exist - make folder and make file
  {
#if defined OS_WIN
    // folder doesn't exist - make folder and make file
    CreateDirectory(mINIPath.Get(), NULL);
    mINIPath.Append("settings.ini");
    UpdateINI(); // will write file if doesn't exist
#elif defined OS_MAC
    mode_t process_mask = umask(0);
    int result_code = mkdir(mINIPath.Get(), S_IRWXU | S_IRWXG | S_IRWXO);
    umask(process_mask);

    if (!result_code)
    {
      mINIPath.Append("settings.ini");
      UpdateINI(); // will write file if doesn't exist
    }
    else
    {
      return false;
    }
#elif defined OS_LINUX
    mINIPath.Set(""); // the folder created above has gone again: run on unsaved defaults
#else
  #error NOT IMPLEMENTED
#endif
  }

  return true;
}

void IPlugAPPHost::UpdateINI()
{
  if (!mINIPath.GetLength())
    return;

  char buf[STRBUFSZ]; // temp buffer for writing integers to profile strings
  const char* ini = mINIPath.Get();

  sprintf(buf, "%u", mState.mAudioDriverType);
  WritePrivateProfileString("audio", "driver", buf, ini);

  WritePrivateProfileString("audio", "indev", mState.mAudioInDev.Get(), ini);
  WritePrivateProfileString("audio", "outdev", mState.mAudioOutDev.Get(), ini);

  sprintf(buf, "%u", mState.mAudioInChanL);
  WritePrivateProfileString("audio", "in1", buf, ini);
  sprintf(buf, "%u", mState.mAudioInChanR);
  WritePrivateProfileString("audio", "in2", buf, ini);
  sprintf(buf, "%u", mState.mAudioOutChanL);
  WritePrivateProfileString("audio", "out1", buf, ini);
  sprintf(buf, "%u", mState.mAudioOutChanR);
  WritePrivateProfileString("audio", "out2", buf, ini);
  //sprintf(buf, "%u", mState.mAudioInIsMono);
  //WritePrivateProfileString("audio", "monoinput", buf, ini);

  WDL_String str;
  str.SetFormatted(32, "%i", mState.mBufferSize);
  WritePrivateProfileString("audio", "buffer", str.Get(), ini);

  str.SetFormatted(32, "%i", mState.mAudioSR);
  WritePrivateProfileString("audio", "sr", str.Get(), ini);

  WritePrivateProfileString("midi", "indev", mState.mMidiInDev.Get(), ini);
  WritePrivateProfileString("midi", "outdev", mState.mMidiOutDev.Get(), ini);

  sprintf(buf, "%u", mState.mMidiInChan);
  WritePrivateProfileString("midi", "inchan", buf, ini);
  sprintf(buf, "%u", mState.mMidiOutChan);
  WritePrivateProfileString("midi", "outchan", buf, ini);
}

std::string IPlugAPPHost::GetAudioDeviceName(uint32_t deviceID) const
{
  auto str = mDAC->getDeviceInfo(deviceID).name;
  std::size_t pos = str.find(':');

  if (pos != std::string::npos)
  {
    std::string subStr = str.substr(pos + 1);
    return subStr;
  }
  else
  {
    return str;
  }
}

std::optional<uint32_t> IPlugAPPHost::GetAudioDeviceID(const char* deviceNameToTest) const
{
  auto deviceIDs = mDAC->getDeviceIds();

  for (auto deviceID : deviceIDs)
  {
    auto name = GetAudioDeviceName(deviceID);

    if (std::string_view(deviceNameToTest) == name)
    {
      return deviceID;
    }
  }
  
  return std::nullopt;
}

int IPlugAPPHost::GetMIDIPortNumber(ERoute direction, const char* nameToTest) const
{
  int start = 1;
  
  auto nameStrView = std::string_view(nameToTest);
  
  if (direction == ERoute::kInput)
  {
    if (nameStrView == OFF_TEXT) return 0;
    
  #ifdef OS_MAC
    start = 2;
    if (nameStrView == "virtual input") return 1;
  #endif
    
    if (!mMidiIn) return -1;

    for (int i = 0; i < mMidiIn->getPortCount(); i++)
    {
      if (nameStrView == mMidiIn->getPortName(i).c_str())
        return (i + start);
    }
  }
  else
  {
    if (nameStrView == OFF_TEXT) return 0;
  
  #ifdef OS_MAC
    start = 2;
    if (nameStrView == "virtual output") return 1;
  #endif
  
    if (!mMidiOut) return -1;

    for (int i = 0; i < mMidiOut->getPortCount(); i++)
    {
      if (nameStrView == mMidiOut->getPortName(i).c_str())
        return (i + start);
    }
  }
  
  return -1;
}

void IPlugAPPHost::ProbeAudioIO()
{
  mAudioInputDevIDs.clear();
  mAudioOutputDevIDs.clear();
  mDefaultInputDev.reset();
  mDefaultOutputDev.reset();

  if (!mDAC)
    return;

  DBGMSG("\nRtAudio Version %s", RtAudio::getVersion().c_str());

  RtAudio::DeviceInfo info;

  auto deviceIDs = mDAC->getDeviceIds();

  for (auto deviceID : deviceIDs)
  {
    info = mDAC->getDeviceInfo(deviceID);

    if (info.inputChannels > 0)
    {
      mAudioInputDevIDs.push_back(deviceID);
    }
    
    if (info.outputChannels > 0)
    {
      mAudioOutputDevIDs.push_back(deviceID);
    }
    
    if (info.isDefaultInput)
    {
      mDefaultInputDev = deviceID;
    }
    
    if (info.isDefaultOutput)
    {
      mDefaultOutputDev = deviceID;
    }
  }
}

void IPlugAPPHost::ProbeMidiIO()
{
  mMidiInputDevNames.clear();
  mMidiOutputDevNames.clear();

  if (!mMidiIn || !mMidiOut)
    return;
  else
  {
    int nInputPorts = mMidiIn->getPortCount();

    mMidiInputDevNames.push_back(OFF_TEXT);

#ifdef OS_MAC
    mMidiInputDevNames.push_back("virtual input");
#endif

    for (int i=0; i<nInputPorts; i++)
    {
      mMidiInputDevNames.push_back(mMidiIn->getPortName(i));
    }

    int nOutputPorts = mMidiOut->getPortCount();

    mMidiOutputDevNames.push_back(OFF_TEXT);

#ifdef OS_MAC
    mMidiOutputDevNames.push_back("virtual output");
#endif

    for (int i=0; i<nOutputPorts; i++)
    {
      mMidiOutputDevNames.push_back(mMidiOut->getPortName(i));
      //This means the virtual output port wont be added as an input
    }
  }
}

bool IPlugAPPHost::AudioSettingsInStateAreEqual(AppState& os, AppState& ns)
{
  if (os.mAudioDriverType != ns.mAudioDriverType) return false;
  if (std::string_view(os.mAudioInDev.Get()) != ns.mAudioInDev.Get()) return false;
  if (std::string_view(os.mAudioOutDev.Get()) != ns.mAudioOutDev.Get()) return false;
  if (os.mAudioSR != ns.mAudioSR) return false;
  if (os.mBufferSize != ns.mBufferSize) return false;
  if (os.mAudioInChanL != ns.mAudioInChanL) return false;
  if (os.mAudioInChanR != ns.mAudioInChanR) return false;
  if (os.mAudioOutChanL != ns.mAudioOutChanL) return false;
  if (os.mAudioOutChanR != ns.mAudioOutChanR) return false;
//  if (os.mAudioInIsMono != ns.mAudioInIsMono) return false;

  return true;
}

bool IPlugAPPHost::MIDISettingsInStateAreEqual(AppState& os, AppState& ns)
{
  if (std::string_view(os.mMidiInDev.Get()) != ns.mMidiInDev.Get()) return false;
  if (std::string_view(os.mMidiOutDev.Get()) != ns.mMidiOutDev.Get()) return false;
  if (os.mMidiInChan != ns.mMidiInChan) return false;
  if (os.mMidiOutChan != ns.mMidiOutChan) return false;

  return true;
}

bool IPlugAPPHost::TryToChangeAudioDriverType()
{
  CloseAudio();

  if (mDAC)
  {
    mDAC = nullptr;
  }

  // Skip RtAudio initialization in no-I/O mode or screenshot mode
  if (mNoIO || IsScreenshotMode())
    return true;

#if defined OS_WIN
  if (mState.mAudioDriverType == kDeviceASIO)
    mDAC = std::make_unique<RtAudio>(RtAudio::WINDOWS_ASIO);
  else if (mState.mAudioDriverType == kDeviceDS)
    mDAC = std::make_unique<RtAudio>(RtAudio::WINDOWS_DS);
#elif defined OS_MAC
  if (mState.mAudioDriverType == kDeviceCoreAudio)
    mDAC = std::make_unique<RtAudio>(RtAudio::MACOSX_CORE);
  //else
  //mDAC = std::make_unique<RtAudio>(RtAudio::UNIX_JACK);
#elif defined OS_LINUX
  if (mState.mAudioDriverType == kDeviceJack)
    mDAC = std::make_unique<RtAudio>(RtAudio::UNIX_JACK);
  else if (mState.mAudioDriverType == kDevicePulse)
    mDAC = std::make_unique<RtAudio>(RtAudio::LINUX_PULSE);
  else
  {
    mState.mAudioDriverType = kDeviceAlsa;
    mDAC = std::make_unique<RtAudio>(RtAudio::LINUX_ALSA);
  }
#endif

  if (mDAC)
  {
    mDAC->setErrorCallback(ErrorCallback);
    return true;
  }

  return false;
}

bool IPlugAPPHost::TryToChangeAudio()
{
  // Skip audio initialization in no-I/O mode or screenshot mode
  if (mNoIO || IsScreenshotMode())
    return true;

  if (!mDAC) // Init() stopped before creating it, e.g. without a settings folder
    return false;

  const bool needsInputDevice = GetPlug()->MaxNChannels(ERoute::kInput) > 0;

#if defined OS_WIN
  // ASIO has one device, use the output for the input ID
  auto inputID = GetAudioDeviceID(mState.mAudioDriverType == kDeviceASIO ? mState.mAudioOutDev.Get() : mState.mAudioInDev.Get());
#elif defined OS_MAC
  auto inputID = GetAudioDeviceID(mState.mAudioInDev.Get());
#elif defined OS_LINUX
  auto inputID = GetAudioDeviceID(mState.mAudioInDev.Get());
#endif
  auto outputID = GetAudioDeviceID(mState.mAudioOutDev.Get());

  bool failedToFindDevice = false;
  bool resetToDefault = false;

  if (!inputID)
  {
    if (mDefaultInputDev)
    {
      resetToDefault = true;
      inputID = mDefaultInputDev;

      if (mAudioInputDevIDs.size())
        mState.mAudioInDev.Set(GetAudioDeviceName(inputID.value()).c_str());
    }
    else if (needsInputDevice)
      failedToFindDevice = true;
  }

  if (!outputID)
  {
    if (mDefaultOutputDev)
    {
      resetToDefault = true;
      outputID = mDefaultOutputDev;

      if (mAudioOutputDevIDs.size())
        mState.mAudioOutDev.Set(GetAudioDeviceName(outputID.value()).c_str());
    }
    else
      failedToFindDevice = true;
  }

  // Without input channels InitAudio opens no input stream, but it still takes an input device ID
  if (!inputID && !needsInputDevice)
    inputID = outputID;

  if (resetToDefault)
  {
    DBGMSG("Couldn't find previous audio device, reseting to default\n");
    UpdateINI();
  }

#ifdef OS_LINUX
  if (failedToFindDevice)
  {
    ReportAudioOff("no device found");
  }
  else if (inputID && outputID)
  {
    UseSampleRateTheDeviceSupports(outputID.value());

    if (!InitAudio(inputID.value(), outputID.value(), mState.mAudioSR, mState.mBufferSize))
    {
      ReportAudioOff("the device did not open");
      return false;
    }

    mAudioOffReason.Set("");
    return true;
  }
#else
  if (failedToFindDevice)
    MessageBox(gHWND, "Please check the audio settings", "Error", MB_OK);

  if (inputID && outputID)
  {
    return InitAudio(inputID.value(), outputID.value(), mState.mAudioSR, mState.mBufferSize);
  }
#endif

  return false;
}

#ifdef OS_LINUX
static std::string WithoutRtAudioPrefix(const std::string& errorText)
{
  const auto classEnd = errorText.find("::");
  const auto messageStart = classEnd == std::string::npos ? std::string::npos : errorText.find(": ", classEnd);
  return messageStart == std::string::npos ? errorText : errorText.substr(messageStart + 2);
}

// A JACK server runs at one rate, and the settings start at 44100
void IPlugAPPHost::UseSampleRateTheDeviceSupports(uint32_t outputID)
{
  const RtAudio::DeviceInfo info = mDAC->getDeviceInfo(outputID);
  const std::vector<unsigned int>& rates = info.sampleRates;
  if (rates.empty() || std::find(rates.begin(), rates.end(), mState.mAudioSR) != rates.end())
    return;

  mState.mAudioSR = info.preferredSampleRate ? info.preferredSampleRate : rates.front();
}

void IPlugAPPHost::ReportAudioOff(const char* reason)
{
  mAudioOffReason.Set(reason);

  WDL_String message;
  message.SetFormatted(1024, "Audio is off (%s): %s.", kAudioDriverNames[mState.mAudioDriverType], reason);
  if (!mDAC->getErrorText().empty())
    message.AppendFormatted(1024, "\n%s", WithoutRtAudioPrefix(mDAC->getErrorText()).c_str());
  message.Append("\n\nChoose an audio driver and device in File > Preferences.");
  fprintf(stderr, "%s: %s\n", BUNDLE_NAME, message.Get());
  MessageBox(gHWND, message.Get(), "Audio", MB_OK);
}
#endif

void IPlugAPPHost::HandleMissingMidiPort([[maybe_unused]] WDL_String& portName)
{
#ifdef OS_LINUX
  // The MIDI API follows the audio driver, and JACK's ports exist only while its server runs, so a saved port is
  // often missing just for now: it stays saved, closed, and Apply or OK connects it once it is back.
#else
  portName.Set(OFF_TEXT);
  UpdateINI();
#endif
}

bool IPlugAPPHost::SelectMIDIDevice(ERoute direction, const char* pPortName)
{
  int port = GetMIDIPortNumber(direction, pPortName);

  if (direction == ERoute::kInput)
  {
    if (port == -1)
    {
      HandleMissingMidiPort(mState.mMidiInDev);
      port = 0;
    }

    //TODO: send all notes off?
    if (mMidiIn)
    {
      mMidiIn->closePort();

      if (port == 0)
      {
        return true;
      }
  #if defined OS_WIN
      else
      {
        mMidiIn->openPort(port-1);
        return true;
      }
  #elif defined OS_MAC
      else if (port == 1)
      {
        std::string virtualMidiInputName = "To ";
        virtualMidiInputName += BUNDLE_NAME;
        mMidiIn->openVirtualPort(virtualMidiInputName);
        return true;
      }
      else
      {
        mMidiIn->openPort(port-2);
        return true;
      }
  #elif defined OS_LINUX
      else
      {
        mMidiIn->openPort(port-1);
        return true;
      }
  #endif
    }
  }
  else
  {
    if (port == -1)
    {
      HandleMissingMidiPort(mState.mMidiOutDev);
      port = 0;
    }
    
    if (mMidiOut)
    {
      //TODO: send all notes off?
#ifdef OS_LINUX
      mMidiOutPortOpen = false;
#endif
      mMidiOut->closePort();
      
      if (port == 0)
        return true;
#if defined OS_WIN
      else
      {
        mMidiOut->openPort(port-1);
        return true;
      }
#elif defined OS_MAC
      else if (port == 1)
      {
        std::string virtualMidiOutputName = "From ";
        virtualMidiOutputName += BUNDLE_NAME;
        mMidiOut->openVirtualPort(virtualMidiOutputName);
        return true;
      }
      else
      {
        mMidiOut->openPort(port-2);
        return true;
      }
#elif defined OS_LINUX
      else
      {
        mMidiOut->openPort(port-1);
        mMidiOutPortOpen = true;
        return true;
      }
#endif
    }
  }
  
  return false;
}

void IPlugAPPHost::CloseAudio()
{
  if (mDAC && mDAC->isStreamOpen())
  {
    if (mDAC->isStreamRunning())
    {
      mAudioEnding = true;
    
      while (!mAudioDone)
        Sleep(10);
      
      mDAC->abortStream();
    }
    
    mDAC->closeStream();
  }
}

bool IPlugAPPHost::InitAudio(uint32_t inID, uint32_t outID, uint32_t sr, uint32_t iovs)
{
  CloseAudio();

  RtAudio::StreamParameters iParams, oParams;
  iParams.deviceId = inID;
  iParams.nChannels = GetPlug()->MaxNChannels(ERoute::kInput); // TODO: flexible channel count
  iParams.firstChannel = 0; // TODO: flexible channel count

  oParams.deviceId = outID;
  oParams.nChannels = GetPlug()->MaxNChannels(ERoute::kOutput); // TODO: flexible channel count
  oParams.firstChannel = 0; // TODO: flexible channel count

  mBufferSize = iovs; // mBufferSize may get changed by stream

  DBGMSG("trying to start audio stream @ %i sr, buffer size %i\nindev = %s\noutdev = %s\ninputs = %i\noutputs = %i\n",
    sr, mBufferSize, GetAudioDeviceName(inID).c_str(), GetAudioDeviceName(outID).c_str(), iParams.nChannels, oParams.nChannels);

  RtAudio::StreamOptions options;
  options.flags = RTAUDIO_NONINTERLEAVED;
#ifdef OS_LINUX
  options.streamName = BUNDLE_NAME; // the client name in JACK, the stream name in PulseAudio
  // PulseAudio's output otherwise buffers up to the server default (about 100 ms on PipeWire); 4 is ALSA's default
  options.numberOfBuffers = 4;
#endif

  mBufIndex = 0;
  mSamplesElapsed = 0;
  mSampleRate = static_cast<double>(sr);
  mVecWait = 0;
  mAudioEnding = false;
  mAudioDone = false;
  
  mIPlug->SetBlockSize(APP_SIGNAL_VECTOR_SIZE);
  mIPlug->SetSampleRate(mSampleRate);
  mIPlug->OnReset();

  auto status = mDAC->openStream(&oParams, iParams.nChannels > 0 ? &iParams : nullptr, RTAUDIO_FLOAT64, sr, &mBufferSize, &AudioCallback, this, &options);

  if (status != RtAudioErrorType::RTAUDIO_NO_ERROR)
  {
    DBGMSG("%s", mDAC->getErrorText().c_str());
    return false;
  }

  for (int i = 0; i < iParams.nChannels; i++)
  {
    mInputBufPtrs.Add(nullptr); //will be set in callback
  }
    
  for (int i = 0; i < oParams.nChannels; i++)
  {
    mOutputBufPtrs.Add(nullptr); //will be set in callback
  }
    
  if (mDAC->startStream() != RTAUDIO_NO_ERROR)
  {
    DBGMSG("Error starting stream: %s\n", mDAC->getErrorText().c_str());
    return false;
  }

  mActiveState = mState;

  return true;
}

#ifdef OS_LINUX
static RtMidi::Api MidiApiForAudioDriver(int audioDriverType)
{
  return audioDriverType == kDeviceJack ? RtMidi::UNIX_JACK : RtMidi::LINUX_ALSA;
}

// The client name in aconnect and PipeWire. JACK gives every RtMidi object its own client; clients named BUNDLE_NAME
// would push the audio client, opened later, to a numbered name like "<app>-02".
static std::string MidiClientName(RtMidi::Api api, const char* pDirection)
{
  std::string name(BUNDLE_NAME);
  if (api == RtMidi::UNIX_JACK)
    name.append(" MIDI ").append(pDirection);
  return name;
}

void IPlugAPPHost::TryToChangeMidiApi()
{
  if (!mMidiIn || mMidiIn->getCurrentApi() != MidiApiForAudioDriver(mState.mAudioDriverType))
    RecreateMidi();
}

void IPlugAPPHost::RecreateJackMidiIfAudioStopped()
{
  if (mState.mAudioDriverType == kDeviceJack && !IsAudioRunning())
    RecreateMidi();
}

void IPlugAPPHost::RecreateMidi()
{
  // The old input's thread or JACK client must be gone before the new one feeds mMidiMsgsFromCallback
  mMidiIn = nullptr;
  mMidiOutPortOpen = false;
  mMidiOut = nullptr;
  InitMidi();
  ProbeMidiIO();
  SelectMIDIDevice(ERoute::kInput, mState.mMidiInDev.Get());
  SelectMIDIDevice(ERoute::kOutput, mState.mMidiOutDev.Get());
}

void IPlugAPPHost::ReselectSavedMidiPorts()
{
  ProbeMidiIO();
  ReselectSavedMidiPort(ERoute::kInput, mMidiIn.get(), mState.mMidiInDev);
  ReselectSavedMidiPort(ERoute::kOutput, mMidiOut.get(), mState.mMidiOutDev);
}

void IPlugAPPHost::ReselectSavedMidiPort(ERoute direction, RtMidi* pMidi, const WDL_String& portName)
{
  // Closing a JACK port unregisters it, which drops the connections made to it in a patchbay. RtMidi's JACK openPort
  // on an open port only connects it to the saved one again.
  const int port = GetMIDIPortNumber(direction, portName.Get());
  if (pMidi && pMidi->isPortOpen() && mState.mAudioDriverType == kDeviceJack && port != 0)
  {
    if (port > 0)
      pMidi->openPort(port - 1);
    return;
  }

  SelectMIDIDevice(direction, portName.Get());
}
#endif

bool IPlugAPPHost::InitMidi()
{
  // Skip MIDI initialization in no-I/O mode or screenshot mode
  if (mNoIO || IsScreenshotMode())
    return true;

#ifdef OS_LINUX
  const RtMidi::Api api = MidiApiForAudioDriver(mState.mAudioDriverType);
#endif

  try
  {
#ifdef OS_LINUX
    mMidiIn = std::make_unique<RtMidiIn>(api, MidiClientName(api, "in"));
#else
    mMidiIn = std::make_unique<RtMidiIn>();
#endif
  }
  catch (RtMidiError &error)
  {
    mMidiIn = nullptr;
    error.printMessage();
    return false;
  }

  try
  {
#ifdef OS_LINUX
    mMidiOut = std::make_unique<RtMidiOut>(api, MidiClientName(api, "out"));
#else
    mMidiOut = std::make_unique<RtMidiOut>();
#endif
  }
  catch (RtMidiError &error)
  {
    mMidiOut = nullptr;
    error.printMessage();
    return false;
  }

  mMidiIn->setCallback(&MIDICallback, this);
  mMidiIn->ignoreTypes(false, true, false );

  return true;
}

void ApplyFades(double *pBuffer, int nChans, int nFrames, bool down)
{
  for (int i = 0; i < nChans; i++)
  {
    double *pIO = pBuffer + (i * nFrames);
    
    if (down)
    {
      for (int j = 0; j < nFrames; j++)
        pIO[j] *= ((double) (nFrames - (j + 1)) / (double) nFrames);
    }
    else
    {
      for (int j = 0; j < nFrames; j++)
        pIO[j] *= ((double) j / (double) nFrames);
    }
  }
}

// static
int IPlugAPPHost::AudioCallback(void* pOutputBuffer, void* pInputBuffer, uint32_t nFrames, double streamTime, RtAudioStreamStatus status, void* pUserData)
{
  IPlugAPPHost* _this = (IPlugAPPHost*) pUserData;

  int nins = _this->GetPlug()->MaxNChannels(ERoute::kInput);
  int nouts = _this->GetPlug()->MaxNChannels(ERoute::kOutput);
  
  double* pInputBufferD = static_cast<double*>(pInputBuffer);
  double* pOutputBufferD = static_cast<double*>(pOutputBuffer);

  bool startWait = _this->mVecWait >= APP_N_VECTOR_WAIT; // wait APP_N_VECTOR_WAIT * iovs before processing audio, to avoid clicks
  bool doFade = _this->mVecWait == APP_N_VECTOR_WAIT || _this->mAudioEnding;
  
  if (startWait && !_this->mAudioDone)
  {
    if (doFade)
      ApplyFades(pInputBufferD, nins, nFrames, _this->mAudioEnding);
    
    for (int i = 0; i < nFrames; i++)
    {
      _this->mBufIndex %= APP_SIGNAL_VECTOR_SIZE;

      if (_this->mBufIndex == 0)
      {
        for (int c = 0; c < nins; c++)
        {
          _this->mInputBufPtrs.Set(c, (pInputBufferD + (c * nFrames)) + i);
        }
        
        for (int c = 0; c < nouts; c++)
        {
          _this->mOutputBufPtrs.Set(c, (pOutputBufferD + (c * nFrames)) + i);
        }
        
        _this->mIPlug->AppProcess(_this->mInputBufPtrs.GetList(), _this->mOutputBufPtrs.GetList(), APP_SIGNAL_VECTOR_SIZE);

        _this->mSamplesElapsed += APP_SIGNAL_VECTOR_SIZE;
      }
      
      for (int c = 0; c < nouts; c++)
      {
        pOutputBufferD[c * nFrames + i] *= APP_MULT;
      }

      _this->mBufIndex++;
    }
    
    if (doFade)
      ApplyFades(pOutputBufferD, nouts, nFrames, _this->mAudioEnding);
    
    if (_this->mAudioEnding)
      _this->mAudioDone = true;
  }
  else
  {
    memset(pOutputBufferD, 0, nFrames * nouts * sizeof(double));
  }
  
  _this->mVecWait = std::min(_this->mVecWait + 1, uint32_t(APP_N_VECTOR_WAIT + 1));

  return 0;
}

// static
void IPlugAPPHost::MIDICallback(double deltatime, std::vector<uint8_t>* pMsg, void* pUserData)
{
  IPlugAPPHost* _this = (IPlugAPPHost*) pUserData;
  
  if (pMsg->size() == 0 || _this->mExiting)
    return;
  
  if (pMsg->size() > 3)
  {
    if (pMsg->size() > MAX_SYSEX_SIZE)
    {
      DBGMSG("SysEx message exceeds MAX_SYSEX_SIZE\n");
      return;
    }
    
    SysExData data { 0, static_cast<int>(pMsg->size()), pMsg->data() };
    
    _this->mIPlug->mSysExMsgsFromCallback.Push(data);
    return;
  }
  else if (pMsg->size())
  {
    IMidiMsg msg;
    msg.mStatus = pMsg->at(0);
    pMsg->size() > 1 ? msg.mData1 = pMsg->at(1) : msg.mData1 = 0;
    pMsg->size() > 2 ? msg.mData2 = pMsg->at(2) : msg.mData2 = 0;

    _this->mIPlug->mMidiMsgsFromCallback.Push(msg);
  }
}

// static
void IPlugAPPHost::ErrorCallback(RtAudioErrorType type, const std::string &errorText)
{
  std::cerr << "\nerrorCallback: " << errorText << "\n\n";
}

