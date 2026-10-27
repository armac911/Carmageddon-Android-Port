#include <signal.h>
#include "harness/os.h"
int main(void){OS_InstallSignalHandler("miyoo-crash-test");raise(SIGSEGV);return 1;}
