# Requires Linux Mesa EGL with GLES 3 software rendering. No game data needed.
import ctypes as C,os,pathlib,struct
os.environ['EGL_PLATFORM']='surfaceless';os.environ['LIBGL_ALWAYS_SOFTWARE']='1'
e=C.CDLL('libEGL.so.1');P=C.c_void_p;I=C.c_int;U=C.c_uint;B=C.c_ubyte;F=C.c_float

def egl(n,ret,args):
 f=getattr(e,n);f.restype=ret;f.argtypes=args;return f
get=egl('eglGetProcAddress',P,[C.c_char_p])
d=egl('eglGetDisplay',P,[P])(None);major=I();minor=I();assert egl('eglInitialize',U,[P,P,P])(d,C.byref(major),C.byref(minor))
assert egl('eglBindAPI',U,[U])(0x30a0)
a=(I*15)(0x3033,1,0x3040,64,0x3024,8,0x3023,8,0x3022,8,0x3021,8,0x3038,0x3038,0x3038);cfg=P();num=I();assert egl('eglChooseConfig',U,[P,P,P,I,P])(d,a,C.byref(cfg),1,C.byref(num)) and num.value
ctx=egl('eglCreateContext',P,[P,P,P,P])(d,cfg,None,(I*3)(0x3098,3,0x3038));assert ctx
surf=egl('eglCreatePbufferSurface',P,[P,P,P])(d,cfg,(I*5)(0x3057,8,0x3056,8,0x3038));assert surf
assert egl('eglMakeCurrent',U,[P,P,P,P])(d,surf,surf,ctx)
def gl(n,ret,*args):return C.CFUNCTYPE(ret,*args)(get(n.encode()))
cs=gl('glCreateShader',U,U);ss=gl('glShaderSource',None,U,I,P,P);co=gl('glCompileShader',None,U);si=gl('glGetShaderiv',None,U,U,P);sl=gl('glGetShaderInfoLog',None,U,I,P,P)
cp=gl('glCreateProgram',U);at=gl('glAttachShader',None,U,U);link=gl('glLinkProgram',None,U);pi=gl('glGetProgramiv',None,U,U,P);pl=gl('glGetProgramInfoLog',None,U,I,P,P)
r=pathlib.Path(__file__).resolve().parents[2]/'src/harness/resources';hdr=(r/'gles_header.glsl').read_text()
def shader(kind,s):
 sh=cs(kind);b=s.encode();arr=(C.c_char_p*1)(b);ss(sh,1,arr,None);co(sh);ok=I();si(sh,0x8b81,C.byref(ok));buf=C.create_string_buffer(8192);sl(sh,8192,None,buf);assert ok.value,buf.value;return sh
v=shader(0x8b31,hdr+(r/'framebuffer_vert.glsl').read_text());# Production compiles features in only when settings use them; exercise the
# full build here and compare the default (no defines) build further below.
ALL='#define PRESENT_SSAA\n#define PRESENT_UI_SCALER\n#define PRESENT_COLOR_ADJUST\n#define PRESENT_SCENE_MASK\n#define PRESENT_LEGACY_SMOOTH\n'
f=shader(0x8b30,hdr+ALL+(r/'framebuffer_frag.glsl').read_text())
p=cp();at(p,v);at(p,f);link(p);ok=I();pi(p,0x8b82,C.byref(ok));buf=C.create_string_buffer(8192);pl(p,8192,None,buf);assert ok.value,buf.value
# The same vertex shader is also used by the integer readback program.
fread=shader(0x8b30,hdr+(r/'readback_frag.glsl').read_text());pr=cp();at(pr,v);at(pr,fread);link(pr);pi(pr,0x8b82,C.byref(ok));pl(pr,8192,None,buf);assert ok.value,buf.value
use=gl('glUseProgram',None,U);use(p);loc=gl('glGetUniformLocation',I,U,C.c_char_p);ui=gl('glUniform1i',None,I,I)
gen=gl('glGenTextures',None,I,P);bind=gl('glBindTexture',None,U,U);active=gl('glActiveTexture',None,U);tex=gl('glTexImage2D',None,U,I,I,I,I,I,U,U,P);param=gl('glTexParameteri',None,U,U,I)
textures=[]
def texture(unit,w,h,values,integer=True):
 t=U();gen(1,C.byref(t));active(0x84c0+unit);bind(0xde1,t);param(0xde1,0x2801,0x2600);param(0xde1,0x2800,0x2600);param(0xde1,0x2802,0x812f);param(0xde1,0x2803,0x812f)
 b=(B*len(values))(*values);gl('glPixelStorei',None,U,I)(0xcf5,1);tex(0xde1,0,0x8232 if integer else 0x8058,w,h,0,0x8d94 if integer else 0x1908,0x1401,b);textures.append((unit,t,w,h))
texture(0,2,2,[1,2,3,4]);texture(2,2,2,[0]*4);texture(3,4,4,[0]*16)
# Non-monotonic palette detects accidental interpolation of palette indices.
colors=[(0,0,0,255),(0,0,255,255),(0,255,0,255),(255,0,0,255),(255,255,255,255)]
pal=[x for color in colors+[(255,0,255,255)]*251 for x in color];texture(1,256,1,pal,False)
for n,u in [('u_pixels',0),('u_palette',1),('u_snapshot',2),('u_super_3d',3)]:ui(loc(p,n.encode()),u)
out_size=gl('glUniform2f',None,I,F,F);out_size(loc(p,b'u_output_size'),8,8)
va=U();gl('glGenVertexArrays',None,I,P)(1,C.byref(va));gl('glBindVertexArray',None,U)(va)
bo=U();gl('glGenBuffers',None,I,P)(1,C.byref(bo));gl('glBindBuffer',None,U,U)(0x8892,bo)
verts=(F*20)(-1,-1,0,0,0,1,-1,0,1,0,-1,1,0,0,1,1,1,0,1,1)
gl('glBufferData',None,U,C.c_ssize_t,P,U)(0x8892,C.sizeof(verts),verts,0x88e4)
ptr=gl('glVertexAttribPointer',None,U,I,U,B,I,P);enable=gl('glEnableVertexAttribArray',None,U)
ptr(0,3,0x1406,0,20,None);enable(0);ptr(2,2,0x1406,0,20,P(12));enable(2)
gl('glViewport',None,I,I,I,I)(0,0,8,8);draw=gl('glDrawArrays',None,U,I,I);read=gl('glReadPixels',None,I,I,I,I,U,U,P)
def render(mode):
 ui(loc(p,b'u_smooth'),mode);draw(5,0,4);buf=(B*256)();read(0,0,8,8,0x1908,0x1401,buf);return list(buf)
sharp=render(0);smooth=render(1)
# CPU reference: bilinear mixing of decoded RGB, clamp at source borders.
rgb=[(c[2],c[1],c[0],c[3]) for c in colors]
for y in range(8):
 for x in range(8):
  idx=(y*8+x)*4;u=(x+.5)/8;v=(y+.5)/8
  expected=rgb[1+min(int(v*2),1)*2+min(int(u*2),1)]
  assert sharp[idx:idx+4]==list(expected),(x,y,sharp[idx:idx+4],expected)
  px=u*2-.5;py=v*2-.5;import math
  bx=math.floor(px);by=math.floor(py);wx=px-bx;wy=py-by
  def color(xx,yy):return rgb[1+max(0,min(1,yy))*2+max(0,min(1,xx))]
  ref=[round((color(bx,by)[c]*(1-wx)+color(bx+1,by)[c]*wx)*(1-wy)+(color(bx,by+1)[c]*(1-wx)+color(bx+1,by+1)[c]*wx)*wy) for c in range(4)]
  assert max(abs(a-b) for a,b in zip(smooth[idx:idx+4],ref))<=1,(x,y,smooth[idx:idx+4],ref)
assert sharp!=smooth
# A high-resolution 3D image must keep its own grid and bottom-up orientation.
def replace(unit,w,h,values):
 active(0x84c0+unit);bind(0xde1,textures[[t[0] for t in textures].index(unit)][1]);b=(B*len(values))(*values)
 tex(0xde1,0,0x8232,w,h,0,0x8d94,0x1401,b)
replace(0,2,2,[0]*4)
scene=[1,2,1,2,3,4,3,4,1,3,2,4,4,2,3,1]
replace(3,4,4,scene)
sharp3d=render(0);smooth3d=render(1)
for y in range(8):
 for x in range(8):
  i=(y*8+x)*4;expected=rgb[scene[(3-y//2)*4+x//2]]
  assert sharp3d[i:i+4]==list(expected)
assert sharp3d!=smooth3d
# Post-readback HUD wins over 3D; index-zero 3D falls back to the pixmap.
replace(0,2,2,[4,0,0,0]);hud=render(0);assert hud[:4]==list(rgb[4])
# Scene mask for FXAA: alpha 0 where the HUD covers, 255 on 3D pixels.
ui(loc(p,b'u_mark_scene'),1);masked=render(0);ui(loc(p,b'u_mark_scene'),0)
assert masked[3]==0 and masked[(7*8+7)*4+3]==255,(masked[3],masked[(7*8+7)*4+3])
replace(3,4,4,[0]*16);empty=render(0);assert empty[:4]==list(rgb[4]) and empty[-4:]==list(rgb[0])
# SSAA resolve: a 2x2 output over the 4x4 super grid averages each 2x2 block
# in gamma-2 light; a 3x3 output (fractional 4/3 footprint) weights by area.
replace(0,2,2,[0]*4);replace(3,4,4,scene)
def resolve(w,h):
 out_size(loc(p,b'u_output_size'),w,h);gl('glViewport',None,I,I,I,I)(0,0,w,h);ui(loc(p,b'u_smooth'),0);draw(5,0,4)
 buf=(B*(w*h*4))();read(0,0,w,h,0x1908,0x1401,buf);return list(buf)
def area_ref(w,h,x,y):
 # u_super_3d is stored bottom-up: output row y covers super rows 3-ty
 acc=[0.0]*4;total=0.0
 x0,x1,y0,y1=x*4/w,(x+1)*4/w,y*4/h,(y+1)*4/h
 for ty in range(4):
  for tx in range(4):
   wgt=max(0,min(x1,tx+1)-max(x0,tx))*max(0,min(y1,ty+1)-max(y0,ty))
   if wgt<=0:continue
   c=rgb[scene[(3-ty)*4+tx]];total+=wgt
   for k in range(3):acc[k]+=(c[k]/255)**2*wgt
   acc[3]+=c[3]/255*wgt
 return [round(math.sqrt(acc[k]/total)*255) for k in range(3)]+[round(acc[3]/total*255)]
for w,h in [(2,2),(3,3),(4,2)]:
 got=resolve(w,h)
 for y in range(h):
  for x in range(w):
   i=(y*w+x)*4;ref=area_ref(w,h,x,y)
   assert max(abs(a-b) for a,b in zip(got[i:i+4],ref))<=2,(w,h,x,y,got[i:i+4],ref)
# Output at the super size (the upscaler source pass) stays exact nearest.
exact=resolve(4,4)
for y in range(4):
 for x in range(4):assert exact[(y*4+x)*4:(y*4+x)*4+4]==list(rgb[scene[(3-y)*4+x]])
# UI scaling applies to HUD/menu pixels only (no 3D here).
replace(3,4,4,[0]*16);replace(0,2,2,[1,2,3,4])
uf=gl('glUniform1f',None,I,F)
ui(loc(p,b'u_ui_scaler'),1)
sharp_ui=resolve(8,8)  # integer 4x: area sampling equals nearest exactly
for y in range(8):
 for x in range(8):
  assert sharp_ui[(y*8+x)*4:(y*8+x)*4+3]==list(rgb[1+(y//4)*2+(x//4)])[:3],(x,y)
centre=resolve(3,3)[(1*3+1)*4:(1*3+1)*4+3]  # 1.5x: centre straddles all four
ref=[round(math.sqrt(sum((rgb[i][k]/255)**2 for i in (1,2,3,4))/4)*255) for k in range(3)]
assert max(abs(a-b) for a,b in zip(centre,ref))<=2,(centre,ref)
ui(loc(p,b'u_ui_scaler'),2);smooth_ui=resolve(8,8)
palette_rgb={tuple(c[:3]) for c in rgb}
assert all(tuple(smooth_ui[i:i+3]) in palette_rgb for i in range(0,len(smooth_ui),4))  # Scale2x never invents colours at integer scale
ui(loc(p,b'u_ui_scaler'),0)
# Colour adjustments: gamma brightens mid-tones; scanlines darken row edges.
replace(0,2,2,[5]*4);colors5=(128,64,32,255)
pal2=[x for color in colors+[colors5]+[(255,0,255,255)]*250 for x in color];texture(1,256,1,pal2,False)
uf(loc(p,b'u_gamma'),1.3);g=resolve(1,1)[:3];uf(loc(p,b'u_gamma'),1.0)
expect=[round((c/255)**(1/1.3)*255) for c in (colors5[2],colors5[1],colors5[0])]
assert max(abs(a-b) for a,b in zip(g,expect))<=2,(g,expect)
uf(loc(p,b'u_scanlines'),0.45);lines=resolve(1,8);uf(loc(p,b'u_scanlines'),0.0)
assert lines[0]<lines[8],(lines[0],lines[8])  # output row 0 sits near a game-row edge, row 2 near its centre
# FXAA: blends a hard diagonal edge on 3D pixels (alpha 255), leaves flat
# areas and HUD-masked pixels (alpha 0) exactly as they were.
# (v was reused as a loop variable above; compile the vertex shader again)
vf=shader(0x8b31,hdr+(r/'framebuffer_vert.glsl').read_text())
ffx=shader(0x8b30,hdr+(r/'fxaa_frag.glsl').read_text());pf=cp();at(pf,vf);at(pf,ffx);link(pf);pi(pf,0x8b82,C.byref(ok));pl(pf,8192,None,buf);assert ok.value,buf.value
def fxaa(alpha):
 src=[]
 for y in range(8):
  for x in range(8):src+= [255,255,255,alpha] if x>y else [0,0,0,alpha]
 t=U();gen(1,C.byref(t));active(0x84c0);bind(0xde1,t);param(0xde1,0x2801,0x2601);param(0xde1,0x2800,0x2601);param(0xde1,0x2802,0x812f);param(0xde1,0x2803,0x812f)
 sb=(B*256)(*src);tex(0xde1,0,0x8058,8,8,0,0x1908,0x1401,sb)
 use(pf);ui(loc(pf,b'u_source'),0);gl('glUniform2f',None,I,F,F)(loc(pf,b'u_source_size'),8,8);gl('glUniform2f',None,I,F,F)(loc(pf,b'u_offset'),0,0)
 gl('glViewport',None,I,I,I,I)(0,0,8,8);draw(5,0,4);out=(B*256)();read(0,0,8,8,0x1908,0x1401,out);use(p);return list(out),src
out,src=fxaa(255)
edge=[out[(y*8+x)*4] for y in range(8) for x in range(8) if abs(x-y)<=1 and 0<out[(y*8+x)*4]<255]
assert edge,'FXAA left the diagonal edge untouched'
assert out[(7*8+0)*4:(7*8+0)*4+3]==[0,0,0] and out[(0*8+7)*4:(0*8+7)*4+3]==[255,255,255]  # flat corners exact
masked,src=fxaa(0)
assert all(masked[i]==src[i] for i in range(256) if i%4!=3),'FXAA changed HUD-masked pixels'
# Default build (no features): identical output to the full build with all
# features off, using the same inputs (HUD + 3D frame, magnified).
fd=shader(0x8b30,hdr+(r/'framebuffer_frag.glsl').read_text());pd=cp();at(pd,vf);at(pd,fd);link(pd);pi(pd,0x8b82,C.byref(ok));pl(pd,8192,None,buf);assert ok.value,buf.value
texture(1,256,1,pal,False);replace(0,2,2,[4,0,0,0]);replace(3,4,4,scene)
def frame(prog):
 use(prog)
 for n,u in [('u_pixels',0),('u_palette',1),('u_snapshot',2),('u_super_3d',3)]:ui(loc(prog,n.encode()),u)
 gl('glViewport',None,I,I,I,I)(0,0,8,8);draw(5,0,4);out=(B*256)();read(0,0,8,8,0x1908,0x1401,out);return list(out)
use(p);out_size(loc(p,b'u_output_size'),8,8);ui(loc(p,b'u_smooth'),0);ui(loc(p,b'u_ui_scaler'),0);ui(loc(p,b'u_mark_scene'),0)
full_off=frame(p);default=frame(pd);use(p)
assert default==full_off,(default,full_off)
# The default build has none of the optional uniforms.
for name in [b'u_output_size',b'u_mark_scene',b'u_ui_scaler',b'u_gamma',b'u_saturation',b'u_scanlines',b'u_smooth']:
 assert loc(pd,name)==-1,name
err=gl('glGetError',U)();assert err==0,hex(err)
print('PASS GLES3 shader compile/link; shared readback link; exact Sharp output; palette-aware bilinear RGB; clamped borders; 3D grid and Y flip; HUD priority and empty-scene fallback; area-weighted SSAA resolve; FXAA scene mask; sharp/Scale2x UI scaling; gamma and scanlines; FXAA edge blend, flat and HUD passthrough; default build matches with no optional work')
