"""Exercise actual gear override code at bounds and across blocked car states."""
from pathlib import Path
import subprocess, tempfile
root=Path(__file__).resolve().parents[2]
s=(root/'src/DETHRACE/common/controls.c').read_text()
a=s.index('static void DRAndroidApplyGearChange('); b=s.index('\n#endif',a)
code='''#include <assert.h>
#include <stdio.h>
typedef struct {int gear,max_gear,knackered,just_changed_gear;} tCar_spec;
static int gRace_finished,gWait_for_it;
'''+s[a:b]+'''
int main(void) {
 tCar_spec c={2,6,0,0};DRAndroidApplyGearChange(&c,1);assert(c.gear==3&&c.just_changed_gear);
 DRAndroidApplyGearChange(&c,-1);assert(c.gear==2);
 c.gear=6;c.just_changed_gear=0;DRAndroidApplyGearChange(&c,1);assert(c.gear==6&&!c.just_changed_gear);
 c.gear=1;DRAndroidApplyGearChange(&c,-1);assert(c.gear==1&&!c.just_changed_gear);
 c.gear=0;DRAndroidApplyGearChange(&c,1);assert(c.gear==1);
 c.gear=-1;DRAndroidApplyGearChange(&c,1);assert(c.gear==-1);
 c.gear=2;c.knackered=1;DRAndroidApplyGearChange(&c,1);assert(c.gear==2);
 c.knackered=0;gRace_finished=1;DRAndroidApplyGearChange(&c,1);assert(c.gear==2);
 gRace_finished=0;gWait_for_it=1;DRAndroidApplyGearChange(&c,1);assert(c.gear==2);
 puts("PASS forward gear override bounds, neutral, reverse, wreck, finish and countdown guards");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(code)
 subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
