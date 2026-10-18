local function out(s) local f=io.open("<scratch>/reaper-au/step1.out","a"); f:write(s.."\n"); f:close() end
reaper.InsertTrackAtIndex(0,true)
local t=reaper.GetTrack(0,0)
local i=reaper.TrackFX_AddByName(t,"AU:HvoyaAudio: Sand",false,-1)
local _,name=reaper.TrackFX_GetFXName(t,0,"")
out("fx "..i.." "..name.." params "..reaper.TrackFX_GetNumParams(t,0))
for p=0,reaper.TrackFX_GetNumParams(t,0)-1 do local _,n=reaper.TrackFX_GetParamName(t,0,p,""); if n=="compression amnt" then out("comp idx "..p.." val "..reaper.TrackFX_GetParamNormalized(t,0,p)) end end
reaper.Main_SaveProjectEx(0,"<scratch>/reaper-au/f2au.RPP",0)
out("saved")
