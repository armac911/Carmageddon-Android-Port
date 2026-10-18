"""Production shader checks for snapshot addressing, 24-bit depth and world clipping."""
import ctypes as C
import pathlib
import runpy

root=pathlib.Path(__file__).resolve().parents[2]
x=runpy.run_path(str(root/'test/android/texture_filter_test.py'))
gl,I,U,B,F,P=(x[k] for k in ('gl','I','U','B','F','P'))
# A spatially varying GPU snapshot independently detects Y flips and the old
# out-of-range top-row fetch. Constant backgrounds cannot catch either bug.
x['setu']('u_material_texture_enabled',0);x['setu']('u_material_index_base',9)
for size in [8,16,24]:
    background=[1+(xx+8*y)%200 for y in range(size) for xx in range(size)]
    x['upload'](4,size,size,background)
    actual=x['render'](0,False,True)
    expected=[(9+background[y*size+xx])%255+1 for y in range(8) for xx in range(8)]
    assert actual==expected,(size,actual[:8],expected[:8])
# Attach the same depth format as the Android renderer and verify nearby
# surfaces remain distinguishable. This separation collapses in 16-bit depth.
g=x['g'];depth=U();g['gen'](1,C.byref(depth));g['active'](0x84c0+5);g['bind'](0xde1,depth)
g['param'](0xde1,0x2801,0x2600);g['param'](0xde1,0x2800,0x2600)
g['tex'](0xde1,0,0x81a6,8,8,0,0x1902,0x1405,None)
gl('glFramebufferTexture2D',None,U,U,U,U,I)(0x8d40,0x8d00,0xde1,depth,0)
assert gl('glCheckFramebufferStatus',U,U)(0x8d40)==0x8cd5
bits=I();gl('glGetIntegerv',None,U,P)(0xd56,C.byref(bits));assert bits.value>=24
x['seti']('u_texture_filtering',0);x['setu']('u_material_flags',0);x['setu']('u_material_blend_enabled',0)
gl('glEnable',None,U)(0xb71);gl('glDepthFunc',None,U)(0x0201);gl('glDepthMask',None,B)(1)
gl('glClearDepthf',None,F)(1);gl('glClear',None,U)(0x100)
verts=list(g['verts'])
for z,index in [(0.000008,40),(0.000007,80)]:
    for i in range(2,len(verts),5):verts[i]=z
    data=(F*len(verts))(*verts)
    gl('glBindBuffer',None,U,U)(0x8892,g['bo'])
    gl('glBufferData',None,U,C.c_ssize_t,P,U)(0x8892,C.sizeof(data),data,0x88e4)
    x['setu']('u_material_index_base',index);g['draw'](5,0,4)
buf=(B*64)();g['read'](0,0,8,8,0x8d94,0x1401,buf);assert set(buf)=={80}
gl('glDisable',None,U)(0xb71)
# Use the actual vertex shader, world transform, projection and clip plane.
# This exercises geometry at a translation large enough to expose half-float
# loss of the sub-unit camera and clipping offsets on hardware using it.
for i in range(2,len(verts),5):verts[i]=0
v=(F*len(verts))(*verts);gl('glBufferData',None,U,C.c_ssize_t,P,U)(0x8892,C.sizeof(v),v,0x88e4)
p=x['program']((x['r']/'3d_vert.glsl').read_text(),x['fragment']);g['use'](p)
loc=lambda name:g['loc'](p,name.encode())
ui=lambda name,value:g['ui'](loc(name),value)
uu=lambda name,value:gl('glUniform1ui',None,I,U)(loc(name),value)
for name,unit in [('u_material_texture_pixelmap',0),('u_material_palette',1),('u_material_shade_table',2),('u_material_blend_table',3),('u_colour_buffer',4),('u_filter_lut',7)]:ui(name,unit)
ui('u_texture_filtering',0);uu('u_material_texture_enabled',0);uu('u_material_flags',0);uu('u_material_blend_enabled',0);uu('u_material_index_base',91)
identity=[1.,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1]
for name,tx in [('u_model',1000.),('u_view',-999.75),('u_projection',0.)]:
    m=list(identity);m[12]=tx
    gl('glUniformMatrix4fv',None,I,I,B,P)(loc(name),1,0,(F*16)(*m))
ui('u_clip_plane_count',1)
gl('glUniform4fv',None,I,I,P)(loc('u_clip_planes[0]'),1,(F*4)(1,0,0,-1000.375))
gl('glClearBufferuiv',None,U,I,P)(0x1800,0,(U*4)(0,0,0,0));g['draw'](5,0,4)
g['read'](0,0,8,8,0x8d94,0x1401,buf)
for y in range(8):
    for xx in range(8):assert buf[y*8+xx]==(91 if xx>=6 else 0),(xx,y,buf[y*8+xx])
assert gl('glGetError',U)()==0
print('PASS snapshot rows/edges at 1x/2x/3x dimensions; D24 depth separates near surfaces; production world/camera/clip-plane precision')
