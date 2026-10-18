-- T-026 F2 on mac REAPER, AU: CC1 sweeps from a MIDI item into Sand (CC1 bound to comp in the state),
-- host value sampled every defer tick, two passes (automation read, then write).
local OUT="<scratch>/reaper-au/step2.out"
local function out(s) local f=io.open(OUT,"a"); f:write(s.."\n"); f:close() end
reaper.Main_openProject("<scratch>/reaper-au/f2au-cc.RPP")
local t=reaper.GetTrack(0,0)
local comp=-1
for p=0,reaper.TrackFX_GetNumParams(t,0)-1 do local _,n=reaper.TrackFX_GetParamName(t,0,p,""); if n=="compression amnt" then comp=p end end
out("comp param "..comp.." start "..reaper.TrackFX_GetParamNormalized(t,0,comp))
-- ramps: 0.5 s 127->0 in 200 ms, 1.0 s 0->127 in 100 ms, 1.5 s 127->0 in 50 ms, 2.0 s 0->100 in 20 ms, then hold to 3.5 s
local it=reaper.CreateNewMIDIItemInProj(t,0,3.5); local tk=reaper.GetActiveTake(it); local n=0
local function ramp(t0,len,a,b) local steps=math.max(2,math.floor(len/0.002)); for k=0,steps do local v=math.floor(a+(b-a)*k/steps+0.5); reaper.MIDI_InsertCC(tk,false,false,reaper.MIDI_GetPPQPosFromProjTime(tk,t0+len*k/steps),0xB0,0,1,v); n=n+1 end end
ramp(0.5,0.2,127,0); ramp(1.0,0.1,0,127); ramp(1.5,0.05,127,0); ramp(2.0,0.02,0,100); reaper.MIDI_Sort(tk)
out("ccs "..n)
local passes={0,3}; local pi=1; local started=false
local function step()
  local playing=reaper.GetPlayState()&1==1
  if not started then
    reaper.SetMediaTrackInfo_Value(t,"I_AUTOMODE",passes[pi]); reaper.SetEditCurPos(0,false,false); reaper.OnPlayButton(); started=true
    out("pass mode "..passes[pi])
  elseif playing then
    local pos=reaper.GetPlayPosition()
    out(string.format("s %d %.3f %.4f", passes[pi], pos, reaper.TrackFX_GetParamNormalized(t,0,comp)))
    if pos>=3.4 then reaper.OnStopButton() end
  else
    local e=reaper.GetFXEnvelope(t,0,comp,false)
    out("pass-end mode "..passes[pi].." host "..reaper.TrackFX_GetParamNormalized(t,0,comp).." env-points "..(e and reaper.CountEnvelopePoints(e) or "none"))
    pi=pi+1; started=false
    if pi>#passes then reaper.Main_SaveProjectEx(0,"<scratch>/reaper-au/f2au-after.RPP",0); out("done"); return end
  end
  reaper.defer(step)
end
reaper.defer(step)
