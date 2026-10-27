#!/usr/bin/env python3
"""Allowlist packaging: binaries, runtime SDL/json-c, licenses and docs only."""
import argparse, hashlib, json, shutil, subprocess, zipfile
from pathlib import Path
p=argparse.ArgumentParser()
p.add_argument('--build',required=True,type=Path)
p.add_argument('--sdl-prefix',required=True,type=Path)
p.add_argument('--sysroot',required=True,type=Path)
p.add_argument('--sdl-source',required=True,type=Path)
a=p.parse_args()
root=Path(__file__).resolve().parent.parent
out=root/'onionos/dist/Carmageddon-OnionOS-experimental-0.1'
if out.exists():shutil.rmtree(out)
shutil.copytree(root/'onionos/App',out/'App',ignore=shutil.ignore_patterns('game','logs','lib'))
app=out/'App/Carmageddon';(app/'lib').mkdir()
for name in ('dethrace','miyoo-probe'):shutil.copy2(a.build/name,app/name)
shutil.copy2(a.sdl_prefix/'lib/libSDL2-2.0.so.0',app/'lib/libSDL2-2.0.so.0')
shutil.copy2(a.sysroot/'usr/lib/libjson-c.so.5',app/'lib/libjson-c.so.5')
for name in ('README.md','FEASIBILITY.md','DEPENDENCIES.md','VALIDATION.md'):shutil.copy2(root/'onionos'/name,out/name)
licenses=out/'licenses';licenses.mkdir()
for name in ('LICENSE','THIRD-PARTY-NOTICES.md'):shutil.copy2(root/name,licenses/name)
shutil.copy2(a.sdl_source/'LICENSE',licenses/'SDL-Mini-LGPL-2.1.txt')
shutil.copy2(a.sdl_source/'sdl2/LICENSE.txt',licenses/'SDL2-zlib.txt')
shutil.copy2(root/'onionos/JSON-C-LICENSE.txt',licenses/'json-c-MIT.txt')
source=subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip()
sdl_revision=subprocess.check_output(['git','rev-parse','HEAD'],cwd=a.sdl_source,text=True).strip()
(app/'BUILD-INFO.txt').write_text(f'Experimental OnionOS 0.1\nEngine commit: {source}\nSDL base commit: {sdl_revision}\nCPU-only Mini patch: onionos/prepare-sdl.py\nHardware test: pending\n')
# Include corresponding engine source and the exact modified LGPL SDL sources.
# This makes the test ZIP independently auditable; neither tree has game data.
subprocess.run(['git','archive','--format=tar','--output='+str(out/'engine-source.tar'),'HEAD'],cwd=root,check=True)
with zipfile.ZipFile(out/'sdl-corresponding-source.zip','w',zipfile.ZIP_DEFLATED) as z:
    tracked=subprocess.check_output(['git','ls-files','-z','sdl2','mini/inc','mini/lib','LICENSE'],cwd=a.sdl_source).decode().split('\0')
    # Do not redistribute proprietary vendor SDK .so files. Firmware supplies them.
    for rel in tracked:
        if not rel or rel.startswith(('mini/lib/', 'sdl2/dependency/', 'sdl2/android-project/', 'sdl2/test/')):continue
        file=a.sdl_source/rel
        if file.is_file():z.write(file,rel)
manifest={str(f.relative_to(out)):hashlib.sha256(f.read_bytes()).hexdigest() for f in sorted(out.rglob('*')) if f.is_file()}
(out/'SHA256SUMS.json').write_text(json.dumps(manifest,indent=2)+'\n')
zipname=out.with_suffix('.zip')
with zipfile.ZipFile(zipname,'w',zipfile.ZIP_DEFLATED) as z:
    for file in sorted(out.rglob('*')):
        if file.is_file():z.write(file,file.relative_to(out))
print(zipname)
