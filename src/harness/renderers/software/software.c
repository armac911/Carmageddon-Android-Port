/* Initial indexed CPU rasterizer. No OpenGL, texture copies or model caches.
 * Known omissions: environment mapping, user clip planes, exact lighting,
 * BRender edge rules and wireframe modes. Device validation is still required.
 */
#include "software.h"
#include "harness/profiler.h"
#include "brender/brender.h"
#include <math.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

extern br_v1db_state v1db;
static br_pixelmap *colour, *depth;
static br_matrix4 view, projection;
static unsigned long triangles;
typedef struct { float p[4], u, v, light; unsigned mask; } Vertex;
static Vertex* vertex_cache;
static unsigned vertex_capacity;
static int reserve_vertices(unsigned needed) {
    if(needed<=vertex_capacity)return 1;
    unsigned capacity=vertex_capacity?vertex_capacity:256;
    while(capacity<needed)capacity*=2;
    Vertex* grown=realloc(vertex_cache,(size_t)capacity*sizeof(*grown));
    if(!grown){fputs("Software vertex scratch allocation failed\n",stderr);return 0;}
    vertex_cache=grown;vertex_capacity=capacity;return 1;
}
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
    const float reciprocal_area=1.0f/area;
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
    int textured=texture && texture->pixels && texture->width && texture->height;
    int shaded=textured && (mat->flags&BR_MATF_LIGHT) && shade && shade->pixels && shade->width>=256 && shade->height;
    int prelit=(mat->flags&BR_MATF_PRELIT)!=0;
    int tw=textured?texture->width:1,th=textured?texture->height:1;
    int texture_stride=textured?texture->row_bytes:0,shade_stride=shaded?shade->row_bytes:0;
    int shade_height=shaded?shade->height:1,blend_stride=blended?blend->row_bytes:0;
    const uint8_t* texture_pixels=textured?(uint8_t*)texture->pixels+texture->base_y*texture_stride+texture->base_x:NULL;
    const uint8_t* shade_pixels=shaded?(uint8_t*)shade->pixels+shade->base_y*shade_stride+shade->base_x:NULL;
    const uint8_t* blend_pixels=blended?(uint8_t*)blend->pixels+blend->base_y*blend_stride+blend->base_x:NULL;
    unsigned base_index=mat->index_base;
    float m00=mat->map_transform.m[0][0],m10=mat->map_transform.m[1][0],m20=mat->map_transform.m[2][0];
    float m01=mat->map_transform.m[0][1],m11=mat->map_transform.m[1][1],m21=mat->map_transform.m[2][1];
    float edge_reciprocal[3];
    for(int i=0;i<3;i++){float dy=s[(i+1)%3].y-s[i].y;edge_reciprocal[i]=dy?1.0f/dy:0;}
    for(int y=miny;y<=maxy;y++) {
        /* Conservative scanline bounds skip empty parts of the bounding box.
         * Padding and the original barycentric coverage tests preserve edges. */
        float left=INFINITY,right=-INFINITY,py=y+.5f;
        for(int i=0;i<3;i++){
            Screen a=s[i],b=s[(i+1)%3];
            if(a.y==b.y){
                if(py==a.y){left=fminf(left,fminf(a.x,b.x));right=fmaxf(right,fmaxf(a.x,b.x));}
            }else if(py>=fminf(a.y,b.y)&&py<=fmaxf(a.y,b.y)){
                float x=a.x+(py-a.y)*edge_reciprocal[i]*(b.x-a.x);
                left=fminf(left,x);right=fmaxf(right,x);
            }
        }
        if(left>right)continue;
        int row_min=(int)floorf(left)-2,row_max=(int)ceilf(right)+2;
        if(row_min<minx)row_min=minx;
        if(row_max>maxx)row_max=maxx;
        uint8_t* colour_row=(uint8_t*)colour->pixels+(y+colour->base_y)*colour->row_bytes+colour->base_x;
        uint16_t* depth_row=depth?(uint16_t*)((uint8_t*)depth->pixels+(y+depth->base_y)*depth->row_bytes)+depth->base_x:NULL;
        for(int x=row_min;x<=row_max;x++) {
            float a=edge(s[1],s[2],x+.5f,y+.5f)*reciprocal_area;
            float b=edge(s[2],s[0],x+.5f,y+.5f)*reciprocal_area,c=1-a-b;
            if(a<0||b<0||c<0)continue;
            float z=a*s[0].z+b*s[1].z+c*s[2].z;
            if(!isfinite(z)||z<0||z>1)continue;
            uint16_t iz=(uint16_t)(z*65534);
            uint16_t* dp=depth_row?depth_row+x:NULL;
            if(dp && iz>*dp)continue;
            unsigned index=base_index;
            if(textured) {
                float q=a*s[0].q+b*s[1].q+c*s[2].q;
                if(q<=0)continue;
                float reciprocal_q=1.0f/q;
                float u=(a*s[0].u+b*s[1].u+c*s[2].u)*reciprocal_q;
                float w=(a*s[0].v+b*s[1].v+c*s[2].v)*reciprocal_q;
                float tu=u*m00+w*m10+m20;
                float tv=u*m01+w*m11+m21;
                if(!isfinite(tu)||!isfinite(tv))continue;
                /* Wrap in float first to prevent out-of-range float->int casts. */
                int tx=(int)((tu-floorf(tu))*tw),ty=(int)((tv-floorf(tv))*th);
                if(tx>=tw)tx=0;
                if(ty>=th)ty=0;
                index=texture_pixels[ty*texture_stride+tx];
                if(!index)continue;
                if(shaded) {
                    int level=0;
                    if(prelit)level=(int)((a*s[0].light+b*s[1].light+c*s[2].light)*reciprocal_q)/shade_height;
                    if(level<0)level=0;
                    if(level>=shade_height)level=shade_height-1;
                    index=shade_pixels[level*shade_stride+index];
                }
            }
            uint8_t* dst=colour_row+x;
            if(blended)
                index=blend_pixels[*dst*blend_stride+index];
            *dst=(uint8_t)index;
            /* Like the GL backend: blend materials test depth but never write it.
             * The horizon's identity blend is invisible; writing its depth would
             * hide subsequently drawn track/car geometry behind a blank plane. */
            if(dp && !blended)*dp=iz;
        }
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
    uint64_t model_start=DRProfileClock();
    DRProfileCount(DRP_MODELS,1);
    br_matrix4 model4;
    BrMatrix4Copy34(&model4,&matrix);
    for(int g=0;g<prepared->ngroups;g++) {
        v11group* group=&prepared->groups[g];
        br_material* mat=group->stored?group->stored:material;
        if(!group->nfaces||!group->nvertices)continue;
        DRProfileCount(DRP_TRIANGLES,group->nfaces);
        uint64_t geom_start=DRProfileClock();
        if(!reserve_vertices(group->nvertices))break;
        /* Transform shared prepared vertices once per group, not per face. Keep
         * the original operation order to preserve clip-space results. */
        for(unsigned i=0;i<group->nvertices;i++) {
            fmt_vertex* vert=&group->vertices[i];Vertex* out=&vertex_cache[i];
            float p[4]={vert->p.v[0],vert->p.v[1],vert->p.v[2],1},world[4],eye[4];
            transform(world,p,&model4);transform(eye,world,&view);transform(out->p,eye,&projection);
            out->u=vert->map.v[0];out->v=vert->map.v[1];
            out->light=group->vertex_colours?(group->vertex_colours[i]>>24)&255:0;
            out->mask=0;
            for(int k=0;k<4;k++)if(!isfinite(out->p[k]))out->mask|=64;
            for(int k=0;k<6;k++)if(distance_plane(out,k)<0)out->mask|=1u<<k;
        }
        DRProfileCount(DRP_VERTICES,group->nvertices);
        DRProfileTime(DRP_GEOMETRY,geom_start);
        uint64_t raster_start=DRProfileClock();
        for(int f=0;f<group->nfaces;f++) {
            unsigned i0=group->faces[f].vertices[0],i1=group->faces[f].vertices[1],i2=group->faces[f].vertices[2];
            if(i0>=group->nvertices||i1>=group->nvertices||i2>=group->nvertices)continue;
            Vertex a[16],b[16];a[0]=vertex_cache[i0];a[1]=vertex_cache[i1];a[2]=vertex_cache[i2];
            unsigned outside=a[0].mask|a[1].mask|a[2].mask;
            if((outside&64)||(a[0].mask&a[1].mask&a[2].mask&63)){
                DRProfileCount(DRP_REJECTED,1);continue;
            }
            int n=3;
            if(outside){
                DRProfileCount(DRP_CLIPPED,1);
                for(int plane=0;plane<6&&n;plane++){
                    n=clip(b,a,n,plane);memcpy(a,b,n*sizeof(Vertex));
                }
            }
            for(int i=1;i+1<n;i++){Vertex tri[3]={a[0],a[i],a[i+1]};raster(tri,mat);}
        }
        DRProfileTime(DRP_RASTER,raster_start);
    }
    DRProfileTime(DRP_MODEL,model_start);
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
