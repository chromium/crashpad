import os
import sys
import subprocess
import shutil
import platform

SYSTEM = {
    'Linux': 'linux',
    'Windows': 'windows',
    'Darwin': 'mac'
}[platform.system()]

ROOT = os.path.abspath(os.path.dirname(os.path.realpath(__file__)))
SRCDIR = os.path.join(ROOT, 'src')

GCLIENT = 'gclient'
GN = 'gn'
NINJA = 'ninja'

if SYSTEM == 'windows':
  GCLIENT += '.bat'
  GN += '.bat'
  NINJA += '.exe'

def prepare_src():
  if os.path.exists(SRCDIR):
    shutil.rmtree(SRCDIR)
  os.mkdir(SRCDIR)
  subprocess.check_call([GCLIENT, 'config', '--name', 'crashpad', 'ssh://git@github.com/manuvideogamemaker/crashpad.git'], cwd=SRCDIR)
  subprocess.check_call([GCLIENT, 'sync', '-v'], cwd=SRCDIR)

def build():
  outdir = os.path.join('out', 'Default')
  crashpaddir = os.path.join(SRCDIR, 'crashpad')
  if not os.path.exists(os.path.join(SRCDIR, outdir)):
    subprocess.check_call([GN, 'gen', outdir], cwd=crashpaddir)
  subprocess.check_call([NINJA, '-C', outdir], cwd=crashpaddir)

def main():
  prepare_src();
  build();
  print("Binaries are in: " + os.path.join(SRCDIR, 'out', 'Default'))
if __name__ == '__main__':
    main()
