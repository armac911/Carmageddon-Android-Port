"""Decode and mix synthetic OGG/MP3/FLAC/WAV with the production music player, no audio device."""
from pathlib import Path
import tempfile,subprocess,os
root=Path(__file__).resolve().parents[2]
fixture=r'''
#include "miniaudio/miniaudio.h"
#include <assert.h>
#include <math.h>
ma_engine miniaudio_engine;
#include "PLAYER"
static double mix(void){float samples[4096*2];double sum=0;for(int i=0;i<8;i++){assert(ma_engine_read_pcm_frames(&miniaudio_engine,samples,4096,NULL)==MA_SUCCESS);for(int j=0;j<8192;j++)sum+=fabs(samples[j]);}return sum;}
int main(void){
 ma_engine_config config=ma_engine_config_init();config.noDevice=MA_TRUE;config.channels=2;config.sampleRate=22050;
 assert(ma_engine_init(&config,&miniaudio_engine)==MA_SUCCESS);
 S3MusicOpen();assert(count==1);S3MusicService(6);assert(loaded&&wanted);assert(mix()>1);
 S3MusicService(0);mix();assert(mix()<0.001);
 S3MusicStop();assert(!loaded&&!wanted);S3MusicService(6);assert(!loaded);
 S3MusicStart();S3MusicService(3);assert(loaded);assert(mix()>1);
 S3MusicClose();assert(!ready&&!loaded);ma_engine_uninit(&miniaudio_engine);
 puts("PASS production music decode/mix, case-insensitive discovery, volume, stop/restart and cleanup");return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'harness').mkdir();(p/'harness/trace.h').write_text('#define LOG_INFO(...) ((void)0)\n#define LOG_WARN(...) ((void)0)\n')
 (p/'test.c').write_text(fixture.replace('PLAYER',str(root/'src/S3/stream_music.c')))
 subprocess.run(['gcc','-std=c99','-O1','-I'+str(p),'-I'+str(root/'lib/miniaudio/include'),'-I'+str(root/'lib/miniaudio/include/miniaudio'),str(p/'test.c'),str(root/'lib/miniaudio/miniaudio_with_vorbis.c'),'-lm','-ldl','-pthread','-o',str(p/'test')],check=True)
 for ext in ['ogg','mp3','flac','wav']:
  folder=p/ext/'music';folder.mkdir(parents=True)
  subprocess.run(['ffmpeg','-v','error','-f','lavfi','-i','sine=frequency=440:duration=30','-y',str(folder/('TrAcK02.'+ext.upper()))],check=True)
  subprocess.run([str(p/'test')],cwd=folder.parent,check=True)
