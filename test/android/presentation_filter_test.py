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
v=shader(0x8b31,hdr+(r/'framebuffer_vert.glsl').read_text());f=shader(0x8b30,hdr+(r/'framebuffer_frag.glsl').read_text())
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
replace(3,4,4,[0]*16);empty=render(0);assert empty[:4]==list(rgb[4]) and empty[-4:]==list(rgb[0])
err=gl('glGetError',U)();assert err==0,hex(err)
print('PASS GLES3 shader compile/link; shared readback link; exact Sharp output; palette-aware bilinear RGB; clamped borders; 3D grid and Y flip; HUD priority and empty-scene fallback')
