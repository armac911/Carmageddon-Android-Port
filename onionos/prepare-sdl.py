#!/usr/bin/env python3
"""Remove Mini's unconditional SwiftShader/EGL route for a CPU-only build.
Run against steward-fu/sdl2 at the commit recorded in DEPENDENCIES.md.
Re-run autogen.sh + configure afterwards. LGPL source changes are in this file.
"""
from pathlib import Path
import sys
root = Path(sys.argv[1]) / 'sdl2'
p = root / 'configure.ac'
s = p.read_text()
start = s.index('CheckMiniVideo()')
end = s.index('CheckA30Video()', start)
block = s[start:end].replace('EXTRA_LDFLAGS="$EXTRA_LDFLAGS -L. -lEGL -lGLESv2"', 'EXTRA_LDFLAGS="$EXTRA_LDFLAGS"')
s = s[:start] + block + s[end:]
p.write_text(s)
p = root / 'src/video/mini/SDL_gles_mini.c'
p.write_text('''/* LGPL-2.1: software-only adapter replacing Mini EGL functions.
 * Original Mini backend: (C) 2025 Steward Fu. */
#include "../../SDL_internal.h"
#if SDL_VIDEO_DRIVER_MINI
#include "SDL_gles_mini.h"
int glLoadLibrary(_THIS,const char* name){return SDL_SetError("GL disabled in CPU-only build");}
void* glGetProcAddress(_THIS,const char* proc){return NULL;}
void glUnloadLibrary(_THIS){}
SDL_GLContext glCreateContext(_THIS,SDL_Window* w){SDL_SetError("GL disabled");return NULL;}
int glSetSwapInterval(_THIS,int i){return -1;}
int glUpdateBufferSettings(void* cb){return 0;}
int glSwapWindow(_THIS,SDL_Window* w){return -1;}
int glMakeCurrent(_THIS,SDL_Window* w,SDL_GLContext c){return -1;}
void glDeleteContext(_THIS,SDL_GLContext c){}
#endif
''')

# Upstream Mini fork forces acceleration even when software is requested.
p = root / 'src/render/SDL_render.c'
s = p.read_text().replace('    flags|= SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC;', '')
p.write_text(s)
