#define PLUG_NAME "IPlugEffect"
#define PLUG_MFR "HvoyaAudio"
#define PLUG_VERSION_HEX 0x00000100
typedef decltype (PLUG_VERSION_HEX) version_hex_t;
#define PLUG_VERSION_STR "0.1.0"
#define PLUG_UNIQUE_ID 'Ipef'
#define PLUG_MFR_ID 'HvyA'
#define PLUG_URL_STR "https://hvoya.audio"
#define PLUG_EMAIL_STR "gene@hvoya.audio"
#define PLUG_COPYRIGHT_STR "Copyright 2025 Hvoya Audio"
#define PLUG_CLASS_NAME IPlugEffect

#define BUNDLE_NAME "IPlugEffect"
#define BUNDLE_MFR "HvoyaAudio"
#define BUNDLE_DOMAIN "io"

#define SHARED_RESOURCES_SUBPATH "IPlugEffect"

#define PLUG_CHANNEL_IO "1-1 2-2"

#define PLUG_LATENCY 0
#define PLUG_TYPE 0
#define PLUG_DOES_MIDI_IN 1
#define PLUG_DOES_MIDI_OUT 0
#define PLUG_DOES_MPE 0
#define PLUG_DOES_STATE_CHUNKS 1

#define PLUG_HAS_UI 1

#define PLUG_HAS_INFO_HEADER 1

#if PLUG_HAS_INFO_HEADER
  #define PLUG_INFO_HEADER_HEIGHT 105
#else
  #define PLUG_INFO_HEADER_HEIGHT 0
#endif

#define PLUG_HEIGHT_NO_HEADER 200
// NB: do not hide inside other defines
#define PLUG_HEIGHT 305

#define PLUG_WIDTH 450

#define PLUG_FPS 60
#define PLUG_SHARED_RESOURCES 0
#define PLUG_HOST_RESIZE 0

#define AUV2_ENTRY IPlugEffect_Entry
#define AUV2_ENTRY_STR "IPlugEffect_Entry"
#define AUV2_FACTORY IPlugEffect_Factory
#define AUV2_VIEW_CLASS IPlugEffect_View
#define AUV2_VIEW_CLASS_STR "IPlugEffect_View"

#define AAX_TYPE_IDS 'IEF1', 'IEF2'
#define AAX_TYPE_IDS_AUDIOSUITE 'IEA1', 'IEA2'
#define AAX_PLUG_MFR_STR "HvyA"
#define AAX_PLUG_NAME_STR "IPlugEffect\nIPEF"
#define AAX_PLUG_CATEGORY_STR "Effect"
#define AAX_DOES_AUDIOSUITE 1

#define VST3_SUBCATEGORY "Fx"

#define CLAP_MANUAL_URL "https://..."
#define CLAP_SUPPORT_URL "https://..."
#define CLAP_DESCRIPTION "A simple audio effect"
#define CLAP_FEATURES "audio-effect"//, "utility"

#define APP_NUM_CHANNELS 2
#define APP_N_VECTOR_WAIT 0
#define APP_MULT 1
#define APP_COPY_AUV3 0
#define APP_SIGNAL_VECTOR_SIZE 64

#define ROBOTO_FN "Roboto-Regular.ttf"
#define ROBOTO_MONO_FN "RobotoMono-Regular.ttf"
#define HVOYA_LOGO_HANDLE_FN "hvoya_logo_white_single.svg"
