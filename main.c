#include<stdlib.h>
#include<stdio.h>
#include<signal.h>
#include<assert.h>
#include<stdbool.h>

#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE); \
                           } while (0)

#define doErrExit(cmd)  do { puts(#cmd); \
                             if((cmd) == -1){errExit(#cmd);} \
                           } while (0)


void sa_sigsegv(int signum, siginfo_t *siginfo, void *ucontext_void){
    ucontext_t *ucontext = ucontext_void;
    assert(signum == siginfo->si_signo);
    assert(signum == SIGSEGV);
    printf("Handling SIGSEGV\n");
}

bool has_signalhandler(const struct sigaction * const act){
    bool ret = 0;
    if((act->sa_flags & SA_SIGINFO) && (act->sa_sigaction != NULL)){
        printf("sa_sigaction at %p\n", act->sa_sigaction);
        ret = 1;
    }
    if(!(act->sa_flags & SA_SIGINFO) && (act->sa_handler != NULL)){
        printf("sa_sighandler at %p\n", act->sa_handler);
        ret = 1;
    }
    return ret;
}

int main(int argc, char** argv){
    puts("Hello World");

    const struct sigaction act = {
        .sa_sigaction = &sa_sigsegv,
        //.sa_mask = ,
        .sa_flags = SA_SIGINFO,

        };
    struct sigaction oldact = {0};
    doErrExit(sigaction(SIGSEGV, &act, &oldact));
    assert(!has_signalhandler(&oldact)); //no old signalhandler overwritten

    doErrExit(sigaction(SIGSEGV, NULL, &oldact));
    assert(has_signalhandler(&oldact)); // check that our handler is now propperly installed

    asm volatile ("" : : : "memory"); // barrier
    int * invalid_ptr = 0x42;
    *invalid_ptr = 0; // invalid memory access
}
