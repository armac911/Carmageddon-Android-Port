"""Production steal thresholds and eligibility, including out-of-range strengths."""
from pathlib import Path
import subprocess,tempfile,os
root=Path(__file__).resolve().parents[2]
s=(root/'src/DETHRACE/common/crush.c').read_text()
a=s.index('int gSteal_ranks[');table=s[a:s.index(';',a)+1]
a=s.index('                    int strength = gOpponents[victim->index].strength_rating;');b=s.index('                        StealCar(victim);',a);b=s.index('                    }',b)+len('                    }')
body=s[a:b]
code=r'''
#include <assert.h>
#include <stdio.h>
#define COUNT_OF(a) (sizeof(a)/sizeof((a)[0]))
typedef struct {int index,can_be_stolen;} Car;
static struct {int strength_rating,dead;} gOpponents[8];
static struct {int rank;} gProgram_state;
static int chance=1,stolen;
static int PercentageChance(int percent){assert(percent==50);return chance;}
static void StealCar(Car* victim){(void)victim;stolen++;}
'''+table+'\nstatic void check(Car* victim) {\n'+body+'\n}\n'+r'''
int main(void) {
 Car car={1,1};int ranks[]={89,72,55,38,21};
 for(int strength=1;strength<=5;strength++) {
  gOpponents[1].strength_rating=strength;gProgram_state.rank=ranks[strength-1];
  stolen=0;check(&car);assert(stolen==1);
  gProgram_state.rank++;stolen=0;check(&car);assert(stolen==0);
 }
 gProgram_state.rank=21;gOpponents[1].strength_rating=99;check(&car);assert(stolen==1);
 stolen=0;gOpponents[1].strength_rating=-9;gProgram_state.rank=89;check(&car);assert(stolen==1);
 stolen=0;chance=0;check(&car);assert(stolen==0);
 chance=1;gOpponents[1].dead=1;check(&car);assert(stolen==0);
 gOpponents[1].dead=0;car.can_be_stolen=0;check(&car);assert(stolen==0);
 car.index=4;car.can_be_stolen=1;chance=0;gProgram_state.rank=99;check(&car);assert(stolen==1);
 puts("PASS original strength 1..5 rank thresholds, strength bounds, chance, eligibility and special car rule");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(code)
 subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
