/* Procedural regression tests, no commercial assets. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "brender/brender.h"
#include "harness/hooks.h"
#include "../../src/harness/renderers/software/software.h"
extern br_v1db_state v1db;
static unsigned char colour[80*64], tex[4]={0,80,100,120};
static unsigned short depth[80*64];
int main(void){
    br_pixelmap c={0},d={0},texture={0};
    c.width=d.width=64;c.height=d.height=64;c.row_bytes=80;d.row_bytes=160;
    c.pixels=colour;d.pixels=depth;texture.width=texture.height=2;texture.row_bytes=2;texture.pixels=tex;
    br_camera cam={0};cam.field_of_view=BR_ANGLE_DEG(60);cam.aspect=1;cam.hither_z=.1;cam.yon_z=100;
    br_actor actor={0};actor.type_data=&cam;BrMatrix34Identity(&v1db.camera_path[0].m);
    fmt_vertex v[3]={0};v[0].p=(br_vector3){{-1,-1,-3}};v[1].p=(br_vector3){{1,-1,-3}};v[2].p=(br_vector3){{0,1,-3}};
    v11face f={0};f.vertices[1]=1;f.vertices[2]=2;
    v11group group={0};group.vertices=v;group.faces=&f;group.nvertices=3;group.nfaces=1;
    v11model p={0};p.ngroups=1;p.groups=&group;br_model m={0};m.prepared=&p;
    br_material mat={0};mat.index_base=220;BrMatrix23Identity(&mat.map_transform);
    br_matrix34 matrix;BrMatrix34Identity(&matrix);
    memset(colour,7,sizeof(colour));memset(depth,255,sizeof(depth));SWBegin(&actor,&c,&d);SWModel(&actor,&m,&mat,BRT_TRIANGLE,matrix);
    assert(colour[32*80+32]==220);assert(depth[32*80+32]<65535);
    mat.index_base=10;matrix.m[3][2]=-3;SWModel(&actor,&m,&mat,BRT_TRIANGLE,matrix);
    assert(colour[32*80+32]==220); /* farther triangle cannot overwrite */
    matrix.m[3][2]=1;mat.index_base=90;SWModel(&actor,&m,&mat,BRT_TRIANGLE,matrix);
    assert(colour[32*80+32]==90);
    for(int y=0;y<64;y++)for(int x=64;x<80;x++)assert(colour[y*80+x]==7);
    /* Transparent index zero preserves the background. */
    BrMatrix34Identity(&matrix);mat.colour_map=&texture;memset(depth,255,sizeof(depth));SWBegin(&actor,&c,&d);
    SWModel(&actor,&m,&mat,BRT_TRIANGLE,matrix);assert(colour[32*80+32]==90);
    tex[0]=80;SWModel(&actor,&m,&mat,BRT_TRIANGLE,matrix);assert(colour[32*80+32]==80);
    /* Entire triangle behind camera is rejected. */
    memset(colour,7,sizeof(colour));matrix.m[3][2]=6;memset(depth,255,sizeof(depth));SWBegin(&actor,&c,&d);
    SWModel(&actor,&m,&mat,BRT_TRIANGLE,matrix);assert(colour[32*80+32]==7);
    /* Near-plane crossing is clipped and cannot write beyond row bounds. */
    BrMatrix34Identity(&matrix);v[0].p.v[2]=.2;memset(depth,255,sizeof(depth));SWBegin(&actor,&c,&d);SWModel(&actor,&m,&mat,BRT_TRIANGLE,matrix);
    for(int y=0;y<64;y++)for(int x=64;x<80;x++)assert(colour[y*80+x]==7);
    /* Sub-pixelmap bases must address the correct backing pixels. */
    memset(colour,7,sizeof(colour));memset(depth,255,sizeof(depth));
    c.width=c.height=d.width=d.height=32;c.base_x=8;c.base_y=4;
    v[0].p.v[2]=-3;SWBegin(&actor,&c,&d);SWModel(&actor,&m,&mat,BRT_TRIANGLE,matrix);
    assert(colour[(4+16)*80+8+16]==80);assert(colour[0]==7);
    puts("PASS software raster: projection, depth, row stride, transparency, behind-camera and near clipping");
    return 0;
}
