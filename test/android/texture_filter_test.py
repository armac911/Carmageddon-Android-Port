"""Numerical tests of the actual 3D fragment shader on Mesa GLES 3.
Requires a C compiler and Linux EGL. No copyrighted game data is used.
"""
import ctypes as C
import math
import pathlib
import runpy
import subprocess
import tempfile

root = pathlib.Path(__file__).resolve().parents[2]
g = runpy.run_path(str(root / 'test/android/presentation_filter_test.py'))
gl, shader, r, hdr = (g[k] for k in ('gl', 'shader', 'r', 'hdr'))
I, U, B, F, P = (g[k] for k in ('I', 'U', 'B', 'F', 'P'))

def program(vertex, fragment):
    v = shader(0x8b31, hdr + vertex)
    f = shader(0x8b30, hdr + fragment)
    p = g['cp'](); g['at'](p, v); g['at'](p, f); g['link'](p)
    ok = I(); g['pi'](p, 0x8b82, C.byref(ok))
    log = C.create_string_buffer(8192); g['pl'](p, 8192, None, log)
    assert ok.value, log.value
    return p

fragment = (r / '3d_frag.glsl').read_text()
program((r / '3d_vert.glsl').read_text(), fragment)
# Use a deterministic screen quad while testing the unmodified production
# fragment shader, including shade/alpha/blend branches.
vertex = '''layout(location=0) in vec3 position;
layout(location=2) in vec2 uv;
out vec3 v_frag_pos; out vec3 v_normal; out vec2 v_tex_coord; out float v_color;
void main(){gl_Position=vec4(position,1); v_frag_pos=vec3(0); v_normal=vec3(0,0,1); v_tex_coord=uv; v_color=2.0;}
'''
p = program(vertex, fragment); g['use'](p)
loc = lambda n: g['loc'](p, n.encode())
seti = lambda n, v: g['ui'](loc(n), v)
setu = lambda n, v: gl('glUniform1ui', None, I, U)(loc(n), v)
for name, unit in [('u_material_texture_pixelmap',0), ('u_material_palette',1),
                   ('u_material_shade_table',2), ('u_material_blend_table',3),
                   ('u_colour_buffer',4), ('u_filter_lut',7)]:
    seti(name, unit)
seti('u_clip_plane_count',0)
setu('u_material_texture_enabled',1); setu('u_material_flags',0)
setu('u_material_blend_enabled',0)
setu('u_material_shade_table_height',2)

# Synthetic non-monotonic palette: full 6x6x6 color cube plus duplicates.
colors=[(0,0,0,0)] + [(a*51,b*51,c*51,255) for c in range(6) for b in range(6) for a in range(6)]
colors += [(0,0,0,255)] * (256-len(colors))
palette=[v for color in colors for v in color]
with tempfile.TemporaryDirectory() as tmp:
    src=pathlib.Path(tmp)/'lut.c'; so=pathlib.Path(tmp)/'lut.so'
    src.write_text('#include "palette_filter.h"\nvoid build(unsigned char *out,const unsigned char *p){DRBuildPaletteLUT5(out,p);}\n')
    subprocess.run(['cc','-std=c99','-O2','-Wall','-Wextra','-Werror','-shared','-fPIC',
                    '-I'+str(root/'src/harness/renderers/gl'),str(src),'-o',str(so)],check=True)
    lib=C.CDLL(str(so)); lib.build.argtypes=[P,P]
    lut=(B*32768)(); pal=(B*1024)(*palette);lib.build(lut,pal)
    assert all(x!=0 for x in lut)
    # Check every LUT cell against an independent exhaustive nearest search in
    # a C reference with different iteration order, including tie behavior.
    src.write_text('''#include <stdint.h>
#include <limits.h>
int verify(const unsigned char *lut,const unsigned char *p){
 for(int cell=0;cell<32768;cell++){
  int c[3]={((cell%32)*255+15)/31,(((cell/32)%32)*255+15)/31,((cell/1024)*255+15)/31};
  int best=255,dist=INT_MAX;
  for(int i=255;i>=1;i--){int d=0;for(int k=0;k<3;k++){int e=p[4*i+k]-c[k];d+=e*e;}
   if(d<=dist){dist=d;best=i;}}
  if(lut[cell]!=best)return cell+1;
 }return 0;}
''')
    refso=pathlib.Path(tmp)/'ref.so'
    subprocess.run(['cc','-O2','-shared','-fPIC',str(src),'-o',str(refso)],check=True)
    ref=C.CDLL(str(refso));ref.verify.argtypes=[P,P];ref.verify.restype=I
    assert ref.verify(lut,pal)==0
    # Palette changes must produce an updated mapping, never a cached old LUT.
    reverse=list(palette)
    for i in range(1,256): reverse[i*4:i*4+3]=[255-v for v in reverse[i*4:i*4+3]]
    lut2=(B*32768)();pal2=(B*1024)(*reverse);lib.build(lut2,pal2)
    assert bytes(lut)!=bytes(lut2) and ref.verify(lut2,pal2)==0

textures={}
def upload(unit,w,h,values,integer=True):
    if unit not in textures:
        t=U();g['gen'](1,C.byref(t));textures[unit]=t
    g['active'](0x84c0+unit);g['bind'](0xde1,textures[unit])
    for pname,value in [(0x2801,0x2600),(0x2800,0x2600),(0x2802,0x2901),(0x2803,0x2901)]:
        g['param'](0xde1,pname,value)
    b=(B*len(values))(*values)
    g['tex'](0xde1,0,0x8232 if integer else 0x8058,w,h,0,0x8d94 if integer else 0x1908,0x1401,b)

indices=[6,181,31,211]
shade0=list(range(256));shade1=[0]+[(i*7)%215+1 for i in range(1,256)]
blend=[(a+b)%255+1 for b in range(256) for a in range(256)]
upload(0,2,2,indices);upload(1,256,1,palette,False)
upload(2,256,2,shade0+shade1);upload(3,256,256,blend)
upload(4,8,16,[7]*128);upload(7,256,128,list(lut))
# Integer render target: the same format used by the engine.
target=U();g['gen'](1,C.byref(target));g['active'](0x84c0+6);g['bind'](0xde1,target)
g['param'](0xde1,0x2801,0x2600);g['param'](0xde1,0x2800,0x2600)
g['tex'](0xde1,0,0x8232,8,8,0,0x8d94,0x1401,None)
fbo=U();gl('glGenFramebuffers',None,I,P)(1,C.byref(fbo))
gl('glBindFramebuffer',None,U,U)(0x8d40,fbo)
gl('glFramebufferTexture2D',None,U,U,U,U,I)(0x8d40,0x8ce0,0xde1,target,0)
assert gl('glCheckFramebufferStatus',U,U)(0x8d40)==0x8cd5

def transform(tx=0,ty=0):
    matrix=(F*6)(1,0,tx,0,1,ty)
    gl('glUniformMatrix2x3fv',None,I,I,B,P)(loc('u_material_uv_transform'),1,0,matrix)

def render(smooth,lit=False,blended=False):
    seti('u_texture_filtering',smooth);setu('u_material_flags',3 if lit else 0)
    setu('u_material_blend_enabled',int(blended))
    gl('glClearBufferuiv',None,U,I,P)(0x1800,0,(U*4)(0,0,0,0))
    g['draw'](5,0,4);buf=(B*64)()
    g['read'](0,0,8,8,0x8d94,0x1401,buf)
    return list(buf)

def expected(x,y,smooth,lit=False,blended=False,tx=0,ty=0):
    u=(x+.5)/8+tx;v=(y+.5)/8+ty
    tex=lambda a,b: indices[(b%2)*2+(a%2)]
    index=tex(math.floor(u*2),math.floor(v*2))
    if index==0:return 0
    shade=lambda i: shade1[i] if lit else i
    result=shade(index)
    if smooth:
        px=u*2-.5;py=v*2-.5;bx=math.floor(px);by=math.floor(py)
        fx=px-bx;fy=py-by;taps=[]
        for dy in range(2):
            for dx in range(2):
                t=tex(bx+dx,by+dy);w=(fx if dx else 1-fx)*(fy if dy else 1-fy)
                if t and w>0:taps.append((shade(t),w))
        if len({i for i,w in taps})==1:result=taps[0][0]
        else:
            total=sum(w for i,w in taps)
            color=[sum(colors[i][k]*w for i,w in taps)/total for k in range(3)]
            q=[math.floor(c/255*31+.5) for c in color]
            result=lut[q[0]+32*q[1]+1024*q[2]]
    if blended:result=blend[7*256+result]
    return result if result else 240

transform()
sharp=render(0);smooth=render(1)
assert sharp!=smooth and len(set(smooth))>4
for tx,ty in [(0,0),(-.3,1.2)]:
    transform(tx,ty)
    for lit in [False,True]:
        for blended in [False,True]:
            for mode in [0,1]:
                actual=render(mode,lit,blended)
                for y in range(8):
                    for x in range(8):
                        ref=expected(x,y,mode,lit,blended,tx,ty)
                        assert actual[y*8+x]==ref,(x,y,mode,lit,blended,actual[y*8+x],ref)
# Cutouts preserve nearest coverage and exclude transparent color from mixing.
indices=[0,181,31,211];upload(0,2,2,indices);transform()
for mode in [0,1]:
    actual=render(mode)
    for y in range(8):
        for x in range(8):assert actual[y*8+x]==expected(x,y,mode)
# Flat regions retain exact indices even where the palette has duplicate colors.
indices=[250]*4;upload(0,2,2,indices);assert set(render(1))=={250}
setu('u_material_texture_enabled',0);setu('u_material_index_base',11)
assert set(render(1))=={11}
assert gl('glGetError',U)()==0
print('PASS production 3D GLES shaders; 32768-cell LUT and palette refresh; Sharp/Smooth texture-space output; shading; repeating UVs; cutouts; indexed blends; exact flat/untextured indices')
