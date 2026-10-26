#include "miniaudio/miniaudio.h"
#include "harness/trace.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

extern ma_engine miniaudio_engine;
#define MAX_TRACKS 128
#define TRACK_PATH 1024
static char tracks[MAX_TRACKS][TRACK_PATH];
static int count, next_track, loaded, ready, wanted;
static ma_sound music;
static float volume;

static int audio_file(const char* name) {
    const char* ext = strrchr(name, '.');
    return ext && (!strcasecmp(ext,".ogg") || !strcasecmp(ext,".mp3")
        || !strcasecmp(ext,".flac") || !strcasecmp(ext,".wav"));
}
static int compare_tracks(const void* a,const void* b) {return strcasecmp(a,b);}
static void stop_track(void) {
    if(loaded){ma_sound_uninit(&music);loaded=0;}
}
void S3MusicClose(void) {stop_track();ready=wanted=count=next_track=0;}
void S3MusicOpen(void) {
    S3MusicClose();ready=1;wanted=1;volume=0;
    // The engine runs with the selected game's root as its working directory.
    // Scan case-insensitively too, so earlier 0.9 imports need no reimport.
    char folder[TRACK_PATH]="";
    DIR* root=opendir(".");struct dirent* e;
    if(root){while((e=readdir(root)))if(!strcasecmp(e->d_name,"MUSIC")){
        struct stat st;if(stat(e->d_name,&st)==0&&S_ISDIR(st.st_mode)){
            snprintf(folder,sizeof(folder),"%s",e->d_name);break;}
    }closedir(root);}
    DIR* dir=folder[0]?opendir(folder):NULL;
    if(dir){while((e=readdir(dir))&&count<MAX_TRACKS)if(audio_file(e->d_name)){
        int n=snprintf(tracks[count],TRACK_PATH,"%s/%s",folder,e->d_name);
        struct stat st;if(n>0&&n<TRACK_PATH&&stat(tracks[count],&st)==0&&S_ISREG(st.st_mode))count++;
    }closedir(dir);}
    qsort(tracks,count,sizeof(tracks[0]),compare_tracks);
    LOG_INFO("Music: found %d tracks",count);
}
void S3MusicMute(void) {if(loaded)ma_sound_set_volume(&music,0);}
void S3MusicStart(void) {if(ready)wanted=1;}
void S3MusicStop(void) {wanted=0;stop_track();}
void S3MusicService(int level) {
    if(!ready)return;
    if(level<0)level=0;if(level>6)level=6;volume=level/6.0f;
    if(loaded)ma_sound_set_volume(&music,volume);
    if(!wanted||!count||!volume)return;
    if(loaded&&ma_sound_is_playing(&music))return;
    stop_track();
    // Try each track at most once. Bad/unsupported files never retry every frame.
    int attempts=count;
    while(attempts--){
        const char* path=tracks[next_track];next_track=(next_track+1)%count;
        ma_result result=ma_sound_init_from_file(&miniaudio_engine,path,
            MA_SOUND_FLAG_STREAM|MA_SOUND_FLAG_NO_SPATIALIZATION,NULL,NULL,&music);
        if(result!=MA_SUCCESS){LOG_WARN("Music: cannot decode %s (%d)",path,result);continue;}
        loaded=1;ma_sound_set_volume(&music,volume);
        result=ma_sound_start(&music);
        if(result==MA_SUCCESS){LOG_INFO("Music: playing %s",path);return;}
        LOG_WARN("Music: cannot start %s (%d)",path,result);stop_track();
    }
    wanted=0;
}
