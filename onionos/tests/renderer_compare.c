/* Frozen 0.1.4 vs current renderer: procedural scenes and equal-work timings.
 * Uses no commercial assets. Small rounding differences at texel boundaries
 * are counted, with a strict image-difference budget. */
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "brender/brender.h"
#include "software.h"
extern br_v1db_state v1db;
void SWRefBegin(br_actor*,br_pixelmap*,br_pixelmap*);
void SWRefModel(br_actor*,br_model*,br_material*,br_token,br_matrix34);
static uint8_t colour[2][112*80],texture_pixels[40*32],shade_pixels[256*16],blend_pixels[256*256];
static uint16_t depth[2][120*80];
static fmt_vertex vertices[96];static br_colour colours[96];static v11face faces[120];
static uint32_t seed=1234567;
static uint32_t random32(void){seed=seed*1664525u+1013904223u;return seed;}
static float random_float(float low,float high){return low+(random32()>>8)*(1.0f/16777216.0f)*(high-low);}
static uint64_t clock_ns(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000000000ull+t.tv_nsec;}
int main(int argc,char** argv){
    int cases=argc>1?atoi(argv[1]):32;if(cases<1||cases>256)cases=32;
    br_pixelmap c[2]={{0}},d[2]={{0}},tex={0},shade={0},blend={0};
    for(int i=0;i<2;i++){
        c[i].width=d[i].width=96;c[i].height=d[i].height=64;
        c[i].row_bytes=112;d[i].row_bytes=240;c[i].pixels=colour[i];d[i].pixels=depth[i];
        c[i].base_x=d[i].base_x=3;c[i].base_y=d[i].base_y=2;
    }
    tex.width=tex.height=32;tex.row_bytes=40;tex.pixels=texture_pixels;
    shade.width=256;shade.height=16;shade.row_bytes=256;shade.pixels=shade_pixels;
    blend.width=blend.height=256;blend.row_bytes=256;blend.pixels=blend_pixels;
    for(int y=0;y<32;y++)for(int x=0;x<32;x++)texture_pixels[y*40+x]=(x*13+y*7)%255;
    for(int y=0;y<16;y++)for(int x=0;x<256;x++)shade_pixels[y*256+x]=(x+y*3)%256;
    for(int y=0;y<256;y++)for(int x=0;x<256;x++)blend_pixels[y*256+x]=(x+y)/2;
    br_camera cam={0};cam.field_of_view=BR_ANGLE_DEG(60);cam.aspect=1.5;cam.hither_z=.1;cam.yon_z=100;
    br_actor actor={0};actor.type_data=&cam;BrMatrix34Identity(&v1db.camera_path[0].m);
    v11group group={0};group.vertices=vertices;group.vertex_colours=colours;group.faces=faces;group.nvertices=96;group.nfaces=120;
    v11model prepared={0};prepared.ngroups=1;prepared.groups=&group;br_model model={0};model.prepared=&prepared;
    br_material mat={0};BrMatrix23Identity(&mat.map_transform);mat.index_base=140;
    br_matrix34 matrix;BrMatrix34Identity(&matrix);
    uint64_t baseline=0,optimized=0;unsigned differences=0,depth_differences=0,total=0;
    for(int test=0;test<cases;test++){
        for(int v=0;v<96;v++){
            vertices[v].p=(br_vector3){{random_float(-4,4),random_float(-3,3),random_float(-9,2)}};
            vertices[v].map=(br_vector2){{random_float(-4,4),random_float(-4,4)}};
            colours[v]=(random32()&255)<<24;
        }
        if(test%17==0)vertices[0].p.v[0]=NAN;
        tex.width=test%2?30:32;tex.height=test%2?31:32;
        for(int f=0;f<120;f++)for(int k=0;k<3;k++)faces[f].vertices[k]=random32()%96;
        mat.colour_map=test%4?&tex:NULL;mat.index_shade=&shade;mat.index_blend=test%5==0?&blend:NULL;
        mat.flags=test%3==0?BR_MATF_LIGHT|BR_MATF_PRELIT:0;
        mat.map_transform.m[2][0]=(test%3)*.1f;
        for(int i=0;i<2;i++){memset(colour[i],7,sizeof(colour[i]));memset(depth[i],255,sizeof(depth[i]));}
        SWRefBegin(&actor,&c[0],&d[0]);SWBegin(&actor,&c[1],&d[1]);
        uint64_t start=clock_ns();SWRefModel(&actor,&model,&mat,BRT_TRIANGLE,matrix);baseline+=clock_ns()-start;
        start=clock_ns();SWModel(&actor,&model,&mat,BRT_TRIANGLE,matrix);optimized+=clock_ns()-start;
        unsigned diff=0;
        for(unsigned i=0;i<sizeof(colour[0]);i++)if(colour[0][i]!=colour[1][i])diff++;
        for(unsigned i=0;i<sizeof(depth[0])/sizeof(depth[0][0]);i++){
            int delta=abs((int)depth[0][i]-(int)depth[1][i]);if(delta>2)depth_differences++;
        }
        differences+=diff;total+=96*64;
        assert(diff<=12); /* <0.2% per scene, allowing isolated UV rounding boundaries. */
    }
    assert(depth_differences<=differences+4);
    printf("PASS renderer comparison scenes=%d colour_differences=%u/%u depth_differences=%u reference_ms=%.3f optimized_ms=%.3f speedup=%.2fx\n",
        cases,differences,total,depth_differences,baseline/1e6,optimized/1e6,(double)baseline/optimized);
}
