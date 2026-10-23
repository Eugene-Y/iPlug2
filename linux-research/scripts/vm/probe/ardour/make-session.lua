-- T-013: build an Ardour session with one track per plug-in (VST3), headless (ardour8-lua).
-- Usage: ardour8-lua make-session.lua   (env T013_DIR = session folder, T013_PLUGINS = "Gneiss Sand IPlugControls")
local dir = os.getenv("T013_DIR")
local names = os.getenv("T013_PLUGINS") or "Gneiss Sand IPlugControls"
AudioEngine:set_backend("None (Dummy)", "", "")
local s = create_session(dir, "t013", 48000)
assert(s, "create_session failed")
for name in names:gmatch("%S+") do
  local tl = s:new_audio_track(2, 2, nil, 1, name, ARDOUR.PresentationInfo.max_order, ARDOUR.TrackMode.Normal, true)
  local track = tl:front()
  local p = ARDOUR.LuaAPI.new_plugin(s, name, ARDOUR.PluginType.VST3, "")
  assert(not p:isnil(), "no plugin " .. name)
  track:add_processor_by_index(p, 0, nil, true)
  print("added", name, p:display_name())
end
s:save_state("", false, false, false, false, false)
print("saved", dir)
close_session()
