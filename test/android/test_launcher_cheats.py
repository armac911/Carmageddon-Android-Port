"""Exercise production race hooks: opt-in, expiry, cleanup and normal pickups."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / 'src/DETHRACE/common/powerup.c').read_text()


def function(name):
    # These top-level C functions have their closing brace at column zero.
    start = source.index(name + '(')
    start = source.rfind('\n', 0, start) + 1
    end = source.index('\n}', start) + 2
    return source[start:end]


fixture = r'''
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#define LOG_TRACE(...) ((void)0)
typedef unsigned int tU32;
typedef struct powerup {
    int got_time, current_value, type;
    tU32 lose_time;
    void (*periodic_proc)(struct powerup*, tU32);
} tPowerup;
static struct { struct { int invulnerable; } current_car; } gProgram_state;
static int gFree_repairs, gFreeze_timer, gNet_mode;
enum { eNet_mode_none, eNet_mode_host, ePowerup_timed = 5 };
static unsigned int gLauncher_cheats;
static tPowerup pickups[1], *gPowerup_array = pickups;
static int gNumber_of_powerups = 1;
static tU32 GetTotalTime(void) { return 100; }
static void LosePowerup(tPowerup* p) {
    p->got_time = 0;
    gProgram_state.current_car.invulnerable = gFree_repairs = gFreeze_timer = 0;
}
'''
for name in ['LauncherCheatEnabled', 'CloseLauncherCheats', 'ApplyLauncherCheats',
             'InitLauncherCheats', 'InitPowerups', 'CloseDownPowerUps', 'DoPowerupPeriodics']:
    fixture += '\n' + function(name)

fixture += r'''
static void expect(int invulnerable, int repairs, int timer) {
    assert(gProgram_state.current_car.invulnerable == invulnerable);
    assert(gFree_repairs == repairs);
    assert(gFreeze_timer == timer);
}
static void configure(unsigned int mask) {
    setenv("DETHRACE_CHEAT_INVULNERABLE", mask & 1 ? "1" : "0", 1);
    setenv("DETHRACE_CHEAT_FREE_REPAIRS", mask & 2 ? "1" : "0", 1);
    setenv("DETHRACE_CHEAT_FREEZE_TIMER", mask & 4 ? "1" : "0", 1);
}
int main(void) {
    // A clean installation has no environment/settings: normal gameplay.
    InitPowerups(); DoPowerupPeriodics(16); expect(0, 0, 0);
    // All selections are inert without an explicitly enabled master switch.
    configure(7);
    InitPowerups(); DoPowerupPeriodics(16); expect(0, 0, 0);
    setenv("DETHRACE_CHEAT_ENABLED", "true", 1);
    InitPowerups(); expect(0, 0, 0); // Reject unexpected environment values.
    setenv("DETHRACE_CHEAT_ENABLED", "1", 1);
    // Every independent combination, repeated races, and native pickup expiry.
    for (unsigned int mask = 0; mask < 8; ++mask) {
        configure(mask);
        for (int race = 0; race < 2; ++race) {
            InitPowerups(); expect(!!(mask & 1), !!(mask & 2), !!(mask & 4));
            pickups[0].got_time = 1; pickups[0].type = ePowerup_timed;
            pickups[0].lose_time = 99; pickups[0].current_value = 1;
            DoPowerupPeriodics(16);
            expect(!!(mask & 1), !!(mask & 2), !!(mask & 4));
            CloseDownPowerUps(); expect(0, 0, 0);
        }
    }
    // Remembered choices cannot leak into network games or master-off races.
    configure(7); gNet_mode = eNet_mode_host;
    InitPowerups(); DoPowerupPeriodics(16); expect(0, 0, 0);
    CloseDownPowerUps(); gNet_mode = eNet_mode_none;
    setenv("DETHRACE_CHEAT_ENABLED", "0", 1);
    InitPowerups(); DoPowerupPeriodics(16); expect(0, 0, 0);
    // With cheats off, real pickups and their natural expiry are untouched.
    gProgram_state.current_car.invulnerable = gFree_repairs = gFreeze_timer = 1;
    DoPowerupPeriodics(16); expect(1, 1, 1);
    pickups[0].got_time = 1; pickups[0].type = ePowerup_timed;
    pickups[0].lose_time = 99; pickups[0].current_value = 1;
    DoPowerupPeriodics(16); expect(0, 0, 0);
    puts("Launcher cheat race lifecycle checks passed");
}
'''
with tempfile.TemporaryDirectory() as directory:
    c_file = Path(directory) / 'cheats.c'
    binary = Path(directory) / 'cheats'
    c_file.write_text(fixture)
    subprocess.run(['gcc', '-std=c99', '-D__ANDROID__', '-Wall', '-Wextra', '-Werror',
                    str(c_file), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
