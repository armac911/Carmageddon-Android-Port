"""Exercise production engine force with proportional throttle/brake and reverse."""
from pathlib import Path
import subprocess,tempfile,os
root=Path(__file__).resolve().parents[2]
s=(root/'src/DETHRACE/common/car.c').read_text()
a=s.index('void CalcEngineForce(');b=s.index('\n// IDA:',a)
code=r'''
#include <math.h>
#include <assert.h>
#include <stdio.h>
#define LOG_TRACE(...) ((void)0)
typedef float br_scalar;
typedef int tS32;
typedef struct {int gear,traction_control; struct {float v[3];} velocity_car_space;
 struct {int backwards,acc,dec;} keys;
 struct {int acc,dec;} joystick;
 float revs,torque,curvature,maxcurve,engine_power_multiplier;
 int power_up_levels[3];struct {int damage_level;} damage_units[1];
 float brake_force,brake_increase,initial_brake,acc_force,force_torque_ratio;
 float target_revs,speed_revs_ratio,M;
} tCar_spec;
static float gEngine_powerup_factor[4]={1,1,1,1};
'''+s[a:b]+r'''
static tCar_spec car(void) {
 tCar_spec c={0};c.gear=1;c.revs=c.target_revs=1000;c.maxcurve=1;
 c.engine_power_multiplier=c.M=c.force_torque_ratio=c.speed_revs_ratio=1;
 c.initial_brake=10;c.brake_increase=90;c.joystick.acc=c.joystick.dec=-1;
 return c;
}
int main(void) {
 tCar_spec c=car();CalcEngineForce(&c,.04f);float idle=c.torque;
 c=car();c.joystick.acc=32768;CalcEngineForce(&c,.04f);float half=c.torque;
 c=car();c.joystick.acc=65535;CalcEngineForce(&c,.04f);float full=c.torque;
 assert(half>idle&&full>half&&fabsf((half-idle)/(full-idle)-.5f)<.001f);
 c=car();c.joystick.dec=32768;CalcEngineForce(&c,.04f);assert(fabsf(c.brake_force-50)<.01f);
 for(int i=0;i<100;i++)CalcEngineForce(&c,.04f);
 assert(fabsf(c.brake_force-50)<.01f); // partial brake never accumulates to full
 c.joystick.dec=-1;CalcEngineForce(&c,.04f);assert(c.brake_force==0);
 c=car();c.joystick.dec=65535;CalcEngineForce(&c,.04f);assert(c.brake_force==100);
 c=car();c.keys.dec=1;CalcEngineForce(&c,.04f);assert(c.brake_force==10);
 c=car();c.curvature=1;c.traction_control=1;c.joystick.acc=32768;CalcEngineForce(&c,.04f);
 assert(fabsf(c.torque-idle-.35f)<.001f); // traction limiter preserves pedal position
 c=car();c.gear=0;c.joystick.dec=32768;CalcEngineForce(&c,.04f);
 assert(c.keys.backwards&&c.joystick.acc==32768&&c.joystick.dec==-1&&c.torque>idle);
 puts("PASS production proportional torque, brake pressure/hold/release, digital fallback, traction limiter and brake-to-reverse");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(code)
 subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-fsanitize=undefined,address',str(p/'test.c'),'-lm','-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
