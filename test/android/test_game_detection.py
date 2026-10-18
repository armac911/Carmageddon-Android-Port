"""Exercise the inherited production game detector with synthetic marker files."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[2]
s=(root/'src/harness/harness.c').read_text();a=s.index('static void Harness_DetectGameMode(');b=s.index('void Harness_Init(',a)
fixture=r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "harness/config.h"
#include "ascii_tables.h"
#define LOG_INFO(...) ((void)0)
#define LOG_PANIC(...) abort()
tHarness_game_info harness_game_info;
/* DETECTOR */
static void file(const char* p){FILE* f=fopen(p,"w");assert(f);fclose(f);}
int main(void){
 file("DATA/RACES/CITYA1.TXT");file("DATA/RACES/CITYB3.TXT");Harness_DetectGameMode();assert(harness_game_info.mode==eGame_carmageddon);
 memset(&harness_game_info,0,sizeof(harness_game_info));
 file("DATA/RACES/CASTLE.TXT");file("DATA/RACES/CASTLE2.TXT");Harness_DetectGameMode();assert(harness_game_info.mode==eGame_splatpack);assert(!strcmp(harness_game_info.defines.INTRO_SMK_FILE,"SPLINTRO.SMK"));
 puts("PASS production native detection: full Carmageddon and Splat Pack markers/intro");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'DATA/RACES').mkdir(parents=True);(p/'test.c').write_text(fixture.replace('/* DETECTOR */',s[a:b]))
 subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-I'+str(root/'src/harness/include'),'-I'+str(root/'src/harness'),str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],cwd=p,check=True)
