"""ntsim.py: duplicate.py's root computation and dirwalk()'s two root passes, replayed with ntpath (Windows path rules)
for the drive setups of notes/30 B; realpath is stubbed to resolve a subst drive, a mapped drive or a junction as
GetFinalPathNameByHandleW does. 'before' = hvoya-linux (newroot from relpath as is, roots computed after copytree),
'after' = linux/T-037 (newroot '/'-separated, roots computed first, ValueError -> error before the copy)."""
import ntpath, os

P = ntpath
FWD_LINE = "IPLUG2_ROOT = ../../.."                         # xcconfig, .mk, makedist-*.sh, prepare_resources-*.py
VS_LINE = "<IPLUG2_ROOT>$(ProjectDir)..\\..\\..</IPLUG2_ROOT>"  # .props, .vcxproj

def dirwalk_root_passes(line, oldroot, newroot):
  line = line.replace(oldroot, newroot)
  return line.replace(oldroot.replace('/', '\\'), newroot.replace('/', '\\'))

def roots(side, target_arg, inputproject="IPlugEffect", oldroot="../../.."):
  outputbasepath = realpath(target_arg)
  outputpath = P.join(outputbasepath, "NewPlug")
  configpath = P.join(inputproject, "config")
  iplug2folder = realpath(P.join(configpath, oldroot))
  try:
    newroot = P.relpath(iplug2folder, P.join(outputpath, "config"))
    if side == "after":
      newroot = newroot.replace(P.sep, '/')
    oldprojectroot = P.relpath(P.join(configpath, oldroot), inputproject).replace(P.sep, '/')
    newprojectroot = P.relpath(iplug2folder, outputpath).replace(P.sep, '/')
  except ValueError as e:
    if side == "after":
      return "error before copytree: output path must be on the same drive as iPlug2 (" + iplug2folder + ")"
    return "traceback after copytree (half-renamed project left): ValueError: " + str(e)
  return dict(newroot=newroot, oldprojectroot=oldprojectroot, newprojectroot=newprojectroot,
              fwd=dirwalk_root_passes(FWD_LINE, oldroot, newroot), vs=dirwalk_root_passes(VS_LINE, oldroot, newroot))

def run(label, cwd, target_arg, mapping):
  global realpath
  os.getcwd = lambda: cwd
  def realpath(p):
    a = P.abspath(p)
    for src, dst in mapping:
      if a.lower().startswith(src.lower()):
        a = dst + a[len(src):]
        break
    return P.normpath(a)
  print(f"{label}\n  cwd={cwd} target={target_arg}")
  for side in ("before", "after"):
    r = roots(side, target_arg)
    if isinstance(r, str):
      print(f"  {side:6s}: {r}")
    else:
      print(f"  {side:6s}: newroot={r['newroot']}  oldprojectroot={r['oldprojectroot']}  newprojectroot={r['newprojectroot']}")
      print(f"          '/' files: {r['fwd']}")
      print(f"          VS files : {r['vs']}")

SUBST = [("S:\\", "C:\\work\\")]
MAPPED = [("Z:\\", "\\\\srv\\share\\")]
JUNC = [("C:\\dev\\", "C:\\real\\")]

run("plain C:, iPlug2 a direct child of the target", r"C:\work\iPlug2\Examples", r"C:\work", [])
run("plain C:, sibling target (iPlug2 not a direct child)", r"C:\work\iPlug2\Examples", r"C:\work\out", [])
run("plain C:, deep target", r"C:\work\iPlug2\Examples", r"C:\work\a\b\c\d", [])
run("plain C:, target '.' (Examples)", r"C:\work\iPlug2\Examples", r".", [])
run("subst S:=C:\\work, target on S:", r"S:\iPlug2\Examples", r"S:\out", SUBST)
run("subst S:, relative target ..\\..\\out", r"S:\iPlug2\Examples", r"..\..\out", SUBST)
run("subst S:, target given on C:", r"S:\iPlug2\Examples", r"C:\work\out", SUBST)
run("subst S:, target '.'", r"S:\iPlug2\Examples", r".", SUBST)
run("mapped Z:=\\\\srv\\share, target on Z:", r"Z:\iPlug2\Examples", r"Z:\out", MAPPED)
run("mapped Z:, target on local D:", r"Z:\iPlug2\Examples", r"D:\out", MAPPED)
run("junction C:\\dev->C:\\real, target in C:\\dev", r"C:\dev\iPlug2\Examples", r"C:\dev\out", JUNC)
run("junction, target outside it", r"C:\dev\iPlug2\Examples", r"C:\other\out", JUNC)
run("different physical drive", r"C:\work\iPlug2\Examples", r"D:\out", [])
