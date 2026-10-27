#include <assert.h>
#include <stdio.h>
#include "globvars.h"
#include "pratcam.h"
extern tPrat_sequence* gPratcam_sequences;
extern int gNumber_of_prat_sequences;
extern int gCurrent_pratcam_precedence;
extern int gCurrent_pratcam_index;
int main(void){
    gRace_finished=0;gInterface_within_race_mode=0;gProgram_state.prat_cam_on=1;
    gCurrent_pratcam_index=-1;gCurrent_pratcam_precedence=123;
    gAusterity_mode=1;gPratcam_sequences=NULL;gNumber_of_prat_sequences=0;
    PratcamEvent(29);NewPratcamSequence(29,0);
    assert(gCurrent_pratcam_precedence==123&&gCurrent_pratcam_index==-1);
    gAusterity_mode=0;PratcamEvent(29);NewPratcamSequence(29,0);
    tPrat_sequence sequences[1]={0};gPratcam_sequences=sequences;gNumber_of_prat_sequences=1;
    PratcamEvent(-1);PratcamEvent(1);NewPratcamSequence(-1,0);NewPratcamSequence(1,0);
    PratcamEvent(0); /* Valid, but lower precedence: no sequence change. */
    assert(gCurrent_pratcam_precedence==123&&gCurrent_pratcam_index==-1);
    puts("PASS pratcam: low-memory/unloaded and out-of-range events return before sequence lookup");
}
