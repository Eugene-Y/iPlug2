"""dup-emul.py mac|win <duplicate.py args...>: run ./duplicate.py (cwd = Examples) with random.seed(11), so the unique ID
is stable. 'win' emulates Windows' path separator: os.path.relpath returns '\\' and os.sep is '\\'; the file system
calls stay POSIX. With SIM_OTHER_DRIVE=<prefix>, relpath between a path under <prefix> and one outside it raises
ntpath's ValueError, as for two Windows drives."""
import os, posixpath, random, runpy, sys

mode = sys.argv[1]
if mode == "win":
  posixrelpath = posixpath.relpath

  def relpath(path, start=os.curdir):
    other = os.environ.get("SIM_OTHER_DRIVE")
    if other and posixpath.abspath(path).startswith(other) != posixpath.abspath(start).startswith(other):
      raise ValueError("path is on mount 'C:', start on mount 'D:'")
    return posixrelpath(path, start).replace("/", "\\")
  os.path.relpath = relpath
  os.sep = "\\"

random.seed(11)
sys.argv = ["duplicate.py"] + sys.argv[2:]
runpy.run_path("duplicate.py", run_name="__main__")
