#include <stdlib.h>
#include <stdio.h>
//__USE_GNU to get REG_RIP from /usr/include/x86_64-linux-gnu/sys/ucontext.h
// uc_mcontext is architecture-specific
#define __USE_GNU
#include <ucontext.h>

#include <signal.h>
#include <assert.h>
#include <stdbool.h>

#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE); \
                           } while (0)

#define doErrExit(cmd)  do { puts(#cmd); \
                             if((cmd) == -1){errExit(#cmd);} \
                           } while (0)

#ifndef __x86_64__
#error("Will likely not run on your arch")
#endif

void sa_sigsegv(int signum, siginfo_t *siginfo, void *ucontext_void){
    ucontext_t *uctx = ucontext_void;
    assert(signum == siginfo->si_signo);
    assert(signum == SIGSEGV);

    // Pointer to the place where all the cpu registers were saved when the exception occured
    gregset_t *saved_registers = &uctx->uc_mcontext.gregs;
    // Pointer to the place where the instruction pointer which triggered the SEGV is saved
    greg_t *rip = &(*saved_registers)[REG_RIP];
    // We need "Pointer to" so we can modify the struct which will later be used to restore the state of the cpu

    printf("Handling SIGSEGV. Invalid memory access to %p (Instruction pointer at %p)\n", siginfo->si_addr, (void *)*rip);

    // only for invalid_ptr exaple
    assert(siginfo->si_code == SEGV_MAPERR); // address not mapped
    assert(siginfo->si_addr == (void*)0x42);
    // end invalid_ptr example

    printf("%p\n", uctx->uc_link);

    //TODO investigate different contexts
    //ucontext_t ucp = {0};
    //doErrExit(getcontext(&ucp));
    //printf("%p %p | %d %d | %p %p \n", ucp.uc_link, uctx->uc_link, ucp.uc_sigmask, uctx->uc_sigmask, ucp.uc_stack.ss_sp, uctx->uc_stack.ss_sp);


    // handle SEGV: just increment instruction pointer. YOLO
    ++(*rip);
    // well, a simple increment may not even point to a valid instruction now.
    // For example movl $0xdeadbeef,(%rax) is c700efbeadde, so an increment will point to a 00ef instruction.
    // YOLO
}

bool has_signalhandler(const struct sigaction *const act){
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

typedef void (*sa_sigaction_t)(int, siginfo_t *, void *);

void install_signalhandler(int signum, sa_sigaction_t handler){
    const struct sigaction act = {
        .sa_sigaction = handler,
        //.sa_mask = ,
        .sa_flags = SA_SIGINFO,

        };
    struct sigaction oldact = {0};
    doErrExit(sigaction(signum, &act, &oldact));
    assert(!has_signalhandler(&oldact)); //no old signalhandler overwritten

    // check that our handler is now propperly installed
    if(sigaction(signum, NULL, &oldact) == -1){
        errExit("sigaction");
    }
    assert(has_signalhandler(&oldact));
    assert(oldact.sa_sigaction == handler);

}

int main(int argc, char** argv){
    puts("Hello World");

    install_signalhandler(SIGSEGV, &sa_sigsegv);

    asm volatile ("" : : : "memory"); // barrier (prevent reordering)

    int *invalid_ptr = (int*)0x42;
    // trigger invalid memor address
    *invalid_ptr = 0xdeadbeef;
    // the segfault handler should print the instruction which triggers the segfault
    // confirm by: $ objdump -d a.out | grep 0xdeadbeef

    asm volatile ("" : : : "memory"); // barrier (prevent reordering)

    puts("All went well. Bye.");
}
