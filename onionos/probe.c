/* No game assets needed. Uses the SAME presenter and rasterizer as the engine. */
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "harness/hooks.h"
#include "harness/config.h"
#include "brender/brender.h"
#include "globvars.h"
extern void Harness_Platform_Init(tHarness_platform*);
extern br_v1db_state v1db;
int main(int argc,char** argv){
    int seconds=argc>1?atoi(argv[1]):30;if(seconds<1||seconds>300)seconds=30;
    setvbuf(stdout,NULL,_IONBF,0);
    Harness_Platform_Init(&gHarness_platform);
    void* w=gHarness_platform.CreateWindowAndRenderer("Carmageddon Miyoo probe",0,0,320,200);
    uint8_t* pixels=calloc(320*200,1);uint16_t* z=malloc(320*200*2);
    if(!pixels||!z)return 2;
    PALETTEENTRY_ palette[256];
    for(int i=0;i<256;i++)palette[i]=(PALETTEENTRY_){i,i,255-i,0};
    gHarness_platform.Renderer_SetPalette(palette);
    br_pixelmap c={0},d={0};c.width=d.width=320;c.height=d.height=200;
    c.row_bytes=320;d.row_bytes=640;c.pixels=pixels;d.pixels=z;
    br_camera camera={0};camera.field_of_view=BR_ANGLE_DEG(60);camera.aspect=1.6f;camera.hither_z=.1f;camera.yon_z=100;
    br_actor actor={0};actor.type_data=&camera;
    BrMatrix34Identity(&v1db.camera_path[0].m);
    fmt_vertex vertices[3]={0};vertices[0].p=(br_vector3){{-1,-1,-3}};
    vertices[1].p=(br_vector3){{1,-1,-3}};vertices[2].p=(br_vector3){{0,1,-3}};
    v11face face={0};face.vertices[1]=1;face.vertices[2]=2;
    v11group group={0};group.faces=&face;group.vertices=vertices;group.nfaces=1;group.nvertices=3;
    v11model prepared={0};prepared.ngroups=1;prepared.groups=&group;
    br_model model={0};model.prepared=&prepared;
    br_material mat={0};mat.index_base=220;
    br_matrix34 matrix;BrMatrix34Identity(&matrix);
    unsigned start=SDL_GetTicks();unsigned frames=0;MSG_ msg={0};
    while(SDL_GetTicks()-start<(unsigned)seconds*1000){
        if(gHarness_platform.ProcessWindowMessages(&msg))break;
        uint8_t keys[256];gHarness_platform.GetKeyboardState(sizeof(keys),keys);
        for(int y=0;y<200;y++)for(int x=0;x<320;x++)pixels[y*320+x]=(x+y+frames)%180;
        memset(z,255,320*200*2);
        gHarness_platform.Renderer_BeginScene(&actor,&c,&d);
        gHarness_platform.Renderer_Model(&actor,&model,&mat,BRT_TRIANGLE,matrix);
        if(!frames && pixels[100*320+160]!=220){fprintf(stderr,"FAIL triangle center not drawn\n");return 3;}
        gHarness_platform.Renderer_FullScreenQuad(pixels);gHarness_platform.SwapWindow();
        gHarness_platform.Renderer_ClearBuffers();SDL_Delay(33);frames++;
    }
    printf("probe done frames=%u elapsed_ms=%u; center triangle check passed\n",frames,SDL_GetTicks()-start);
    free(pixels);free(z);gHarness_platform.DestroyWindow(w);return 0;
}
