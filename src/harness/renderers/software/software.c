/* Initial indexed CPU rasterizer. No OpenGL, texture copies or model caches.
 * Known omissions: environment mapping, user clip planes, exact lighting,
 * BRender edge rules and wireframe modes. Device validation is still required.
 */
#include "software.h"
#include "brender/brender.h"
#include <math.h>
#include <stdint.h>
#include <string.h>

extern br_v1db_state v1db;
static br_pixelmap *colour, *depth;
static br_matrix4 view, projection;
static unsigned long triangles;
typedef struct { float p[4], u, v, light; } Vertex;
typedef struct { float x,y,z,q,u,v,light; } Screen;

static void transform(float out[4], const float in[4], const br_matrix4* m) {
    for (int c=0;c<4;c++) {
        out[c]=0;
        for (int r=0;r<4;r++) out[c]+=in[r]*m->m[r][c];
    }
}
/* Homogeneous clipping avoids huge projected coordinates at the camera. */
static float distance_plane(const Vertex* v, int p) {
    int axis=p/2;
    return v->p[3] + (p&1 ? -v->p[axis] : v->p[axis]);
}
static int clip(Vertex* dst, const Vertex* src, int n, int plane) {
    int out=0;
    for (int i=0;i<n;i++) {
        const Vertex *a=&src[i], *b=&src[(i+1)%n];
        float da=distance_plane(a,plane), db=distance_plane(b,plane);
        if (da>=0) dst[out++]=*a;
        if ((da<0)!=(db<0)) {
            float t=da/(da-db);
            Vertex* v=&dst[out++];
            for (int k=0;k<4;k++) v->p[k]=a->p[k]+t*(b->p[k]-a->p[k]);
            v->u=a->u+t*(b->u-a->u); v->v=a->v+t*(b->v-a->v);
            v->light=a->light+t*(b->light-a->light);
        }
    }
    return out;
}
static float edge(Screen a, Screen b, float x, float y) {
    return (b.x-a.x)*(y-a.y)-(b.y-a.y)*(x-a.x);
}
static int wrap(int x,int n) { int r=x%n; return r<0?r+n:r; }
static uint8_t texel(br_pixelmap* pm,int x,int y) {
    return ((uint8_t*)pm->pixels)[(y+pm->base_y)*pm->row_bytes+x+pm->base_x];
}
static void raster(Vertex* v, br_material* mat) {
    Screen s[3];
    for(int i=0;i<3;i++) {
        if(v[i].p[3]<=1e-6f) return;
        float q=1.0f/v[i].p[3];
        s[i]=(Screen){(v[i].p[0]*q+1)*colour->width*.5f,
            (1-v[i].p[1]*q)*colour->height*.5f,
            v[i].p[2]*q*.5f+.5f,q,v[i].u*q,v[i].v*q,v[i].light*q};
    }
    float area=edge(s[0],s[1],s[2].x,s[2].y);
    /* Both windings deliberately accepted until on-device geometry checks. */
    if(!isfinite(area)||fabsf(area)<1e-6f) return;
    int minx=(int)floorf(fminf(s[0].x,fminf(s[1].x,s[2].x)));
    int maxx=(int)ceilf(fmaxf(s[0].x,fmaxf(s[1].x,s[2].x)));
    int miny=(int)floorf(fminf(s[0].y,fminf(s[1].y,s[2].y)));
    int maxy=(int)ceilf(fmaxf(s[0].y,fmaxf(s[1].y,s[2].y)));
    if(minx<0)minx=0;
    if(miny<0)miny=0;
    if(maxx>=colour->width)maxx=colour->width-1;
    if(maxy>=colour->height)maxy=colour->height-1;
    br_pixelmap *texture=mat->colour_map,*shade=mat->index_shade,*blend=mat->index_blend;
    int blended=blend && blend->pixels && blend->width>=256 && blend->height>=256;
    for(int y=miny;y<=maxy;y++) for(int x=minx;x<=maxx;x++) {
        float a=edge(s[1],s[2],x+.5f,y+.5f)/area;
        float b=edge(s[2],s[0],x+.5f,y+.5f)/area,c=1-a-b;
        if(a<0||b<0||c<0)continue;
        float z=a*s[0].z+b*s[1].z+c*s[2].z;
        if(!isfinite(z)||z<0||z>1)continue;
        uint16_t iz=(uint16_t)(z*65534);
        uint16_t* dp=depth?(uint16_t*)((uint8_t*)depth->pixels+(y+depth->base_y)*depth->row_bytes)+x+depth->base_x:NULL;
        if(dp && iz>*dp)continue;
        unsigned index=mat->index_base;
        if(texture && texture->pixels && texture->width && texture->height) {
            float q=a*s[0].q+b*s[1].q+c*s[2].q;
            if(q<=0)continue;
            float u=(a*s[0].u+b*s[1].u+c*s[2].u)/q;
            float w=(a*s[0].v+b*s[1].v+c*s[2].v)/q;
            float tu=u*mat->map_transform.m[0][0]+w*mat->map_transform.m[1][0]+mat->map_transform.m[2][0];
            float tv=u*mat->map_transform.m[0][1]+w*mat->map_transform.m[1][1]+mat->map_transform.m[2][1];
            if(!isfinite(tu)||!isfinite(tv))continue;
            /* Wrap in float first to prevent out-of-range float->int casts. */
            int tx=wrap((int)floorf((tu-floorf(tu))*texture->width),texture->width);
            int ty=wrap((int)floorf((tv-floorf(tv))*texture->height),texture->height);
            index=texel(texture,tx,ty);
            if(!index)continue;
            if((mat->flags&BR_MATF_LIGHT) && shade && shade->pixels && shade->width>=256 && shade->height) {
                int level=0;
                if(mat->flags&BR_MATF_PRELIT)
                    level=(int)((a*s[0].light+b*s[1].light+c*s[2].light)/q)/shade->height;
                if(level<0)level=0;
                if(level>=shade->height)level=shade->height-1;
                index=texel(shade,index,level);
            }
        }
        uint8_t* dst=(uint8_t*)colour->pixels+(y+colour->base_y)*colour->row_bytes+x+colour->base_x;
        if(blended)
            index=texel(blend,index,*dst);
        *dst=(uint8_t)index;
        /* Like the GL backend: blend materials test depth but never write it.
         * The horizon's identity blend is invisible; writing its depth would
         * hide subsequently drawn track/car geometry behind a blank plane. */
        if(dp && !blended)*dp=iz;
    }
    triangles++;
}
void SWBegin(br_actor* camera,br_pixelmap* c,br_pixelmap* d) {
    colour=c;depth=d;
    br_camera* cam=camera->type_data;
    BrMatrix4Copy34(&view,&v1db.camera_path[0].m);
    BrMatrix4Inverse(&view,&view);
    BrMatrix4Perspective(&projection,cam->field_of_view,cam->aspect,cam->hither_z,cam->yon_z);
    projection.m[2][2]*=-1;
    /* Engine owns/clears both pixelmaps. Preserve depth across scene boundaries
     * and flushes so shadows and overlapping models share the same values. */
    if(depth && (depth->width<colour->width || depth->height<colour->height))depth=NULL;
}
void SWModel(br_actor* actor,br_model* model,br_material* material,br_token type,br_matrix34 matrix) {
    (void)actor;(void)type;
    v11model* prepared=model->prepared;
    if(!colour||!prepared||!material)return;
    br_matrix4 model4;
    BrMatrix4Copy34(&model4,&matrix);
    for(int g=0;g<prepared->ngroups;g++) {
        v11group* group=&prepared->groups[g];
        br_material* mat=group->stored?group->stored:material;
        for(int f=0;f<group->nfaces;f++) {
            Vertex a[16],b[16];int valid=1;
            for(int i=0;i<3;i++) {
                unsigned idx=group->faces[f].vertices[i];
                if(idx>=group->nvertices){valid=0;break;}
                fmt_vertex* vert=&group->vertices[idx];
                float p[4]={vert->p.v[0],vert->p.v[1],vert->p.v[2],1},world[4],eye[4];
                transform(world,p,&model4);transform(eye,world,&view);transform(a[i].p,eye,&projection);
                for(int k=0;k<4;k++)if(!isfinite(a[i].p[k]))valid=0;
                a[i].u=vert->map.v[0];a[i].v=vert->map.v[1];
                a[i].light=group->vertex_colours?(group->vertex_colours[idx]>>24)&255:0;
            }
            if(!valid)continue;
            int n=3;
            for(int p=0;p<6 && n;p++){n=clip(b,a,n,p);memcpy(a,b,n*sizeof(Vertex));}
            for(int i=1;i+1<n;i++){Vertex tri[3]={a[0],a[i],a[i+1]};raster(tri,mat);}
        }
    }
}
/* Groups hold the original material pointer. Model vertices remain engine-owned,
 * so deformation updates are visible without a second copy. */
void SWMaterial(br_material* m){m->stored=m;}
void SWTexture(br_pixelmap* p){(void)p;}
void SWBufferModel(br_model* m){(void)m;}
void SWFlush(void){}
void SWEnd(void){}
void SWClear(void){colour=NULL;depth=NULL;}
void SWViewport(int x,int y,int w,int h){(void)x;(void)y;(void)w;(void)h;}
unsigned long SWTriangleCount(void){return triangles;}
