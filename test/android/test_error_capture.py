"""Production native capture: opt-out, bounded writes and fatal-signal chaining."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[2]
fixture=r'''
#define _GNU_SOURCE
#include "error_log.h"
#include <assert.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
static int chain_pipe;
static void prior(int signal,siginfo_t* info,void* context){
 assert(signal==SIGABRT&&info&&context);ssize_t result=write(chain_pipe,"p",1);(void)result;_exit(77);
}
static long length(const char* name){struct stat st;return stat(name,&st)==0?st.st_size:-1;}
int main(int argc,char** argv){
 assert(argc==2);assert(chdir(argv[1])==0);struct rlimit no_core={0,0};setrlimit(RLIMIT_CORE,&no_core);
 unsetenv("DETHRACE_ERROR_DIRECTORY");DRErrorLogStart();DRErrorLogMessage(6,"panic ","disabled");assert(length("engine.log")==-1);
 pid_t child=fork();assert(child>=0);if(child==0){raise(SIGSEGV);_exit(99);}int status;waitpid(child,&status,0);assert(WIFSIGNALED(status)&&WTERMSIG(status)==SIGSEGV);
 setenv("DETHRACE_ERROR_DIRECTORY",".",1);DRErrorLogStart();assert(length("native-maps.txt")>0);
 DRErrorLogMessage(3,"debug ","omit");assert(length("engine.log")==0);
 char body[1024];memset(body,'x',sizeof(body)-1);body[sizeof(body)-1]=0;
 for(int i=0;i<2600;i++)DRErrorLogMessage(4,"engine ",body);
 DRErrorLogMessage(6,"panic ","tail-marker");
 assert(length("engine.log")>0&&length("engine.log")<=2*1024*1024);
 assert(length("engine.log.previous")>0&&length("engine.log.previous")<=2*1024*1024);
 child=fork();assert(child>=0);if(child==0){raise(SIGABRT);_exit(99);}waitpid(child,&status,0);assert(WIFSIGNALED(status)&&WTERMSIG(status)==SIGABRT);
 FILE* f=fopen("native-signal.txt","r");assert(f);char line[200];assert(fgets(line,sizeof(line),f));fclose(f);assert(strstr(line,"signal=0x6")&&strstr(line," pc=0x"));
 DRErrorLogStop();long before=length("engine.log");DRErrorLogMessage(6,"panic ","after-stop");assert(length("engine.log")==before);
 int pipefd[2];assert(pipe(pipefd)==0);child=fork();assert(child>=0);
 if(child==0){close(pipefd[0]);chain_pipe=pipefd[1];struct sigaction action={0};action.sa_sigaction=prior;action.sa_flags=SA_SIGINFO;sigemptyset(&action.sa_mask);sigaction(SIGABRT,&action,NULL);DRErrorLogStart();raise(SIGABRT);_exit(99);}
 close(pipefd[1]);char got=0;assert(read(pipefd[0],&got,1)==1&&got=='p');close(pipefd[0]);waitpid(child,&status,0);assert(WIFEXITED(status)&&WEXITSTATUS(status)==77);
 puts("PASS native errors: disabled capture, bounded rotation, terminal signal status, signal context and previous-handler chaining, clean stop");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'fixture.c').write_text(fixture)
 subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pthread','-I'+str(root/'src/harness/platforms'),str(p/'fixture.c'),str(root/'src/harness/platforms/error_log.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test'),str(p)],check=True)
