-- Run by REAPER at startup (`reaper -new reaper-open-fx.lua`): puts the plug-in named in IPLUG2_REAPER_FX
-- (e.g. "CLAP: IPlugEffect") on a new track and opens its editor, so an IDE's "Debug in REAPER" needs no clicks.
local fx_name = os.getenv("IPLUG2_REAPER_FX")
if not fx_name or fx_name == "" then
  reaper.ShowConsoleMsg("reaper-open-fx.lua: IPLUG2_REAPER_FX is not set\n")
  return
end
reaper.InsertTrackAtIndex(0, true)
local track = reaper.GetTrack(0, 0)
reaper.GetSetMediaTrackInfo_String(track, "P_NAME", fx_name, true)
local fx = reaper.TrackFX_AddByName(track, fx_name, false, -1)
if fx < 0 then
  reaper.ShowConsoleMsg("reaper-open-fx.lua: REAPER does not know \"" .. fx_name .. "\" (not deployed, or needs a rescan)\n")
  return
end
local SHOW_FLOATING = 3
reaper.TrackFX_Show(track, fx, SHOW_FLOATING)
