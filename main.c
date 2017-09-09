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

void * get_si_addr(const int signum, const siginfo_t *const siginfo){
    // si_addr only defined for certain signals
    assert(signum == SIGILL || signum == SIGFPE || signum == SIGSEGV || signum == SIGBUS || signum == SIGTRAP);
    return siginfo->si_addr;
}

// Return a pointer to the RIP stored in the machine context which will be restored on sigreturn.
greg_t* get_pointer_to_saved_rip(const ucontext_t *const uctx){
    // Pointer to the place where all the cpu registers were saved when the exception occured
    const gregset_t *const saved_registers = &uctx->uc_mcontext.gregs;
    // Pointer to the place where the instruction pointer which triggered the SEGV is saved
    const greg_t *const rip = &(*saved_registers)[REG_RIP];
    // We need "Pointer to" so we can modify the struct which will later be used to restore the state of the cpu

    return (greg_t *) rip; //discards const qualifier
}

void sa_sigsegv(int signum, siginfo_t *siginfo, void *ucontext){
    assert(signum == siginfo->si_signo);
    assert(signum == SIGSEGV);
    greg_t *rip = get_pointer_to_saved_rip(ucontext);
    printf("Handling SIGSEGV. Invalid memory access to %p (Instruction pointer at %p)\n", get_si_addr(signum, siginfo), (void *)*rip);

    if(siginfo->si_code == SEGV_MAPERR){
        printf("\tAddress not mapped to object.\n");
    }else{
        assert(siginfo->si_code == SEGV_ACCERR);
        printf("\tInvalid permissions for mapped object.\n");
    }

    //TODO investigate different contexts
    //ucontext_t ucp = {0};
    //doErrExit(getcontext(&ucp));
    //printf("%p %p | %d %d | %p %p \n", ucp.uc_link, uctx->uc_link, ucp.uc_sigmask, uctx->uc_sigmask, ucp.uc_stack.ss_sp, uctx->uc_stack.ss_sp);


    // handle SEGV: just increment instruction pointer. YOLO
    ++(*rip);
    // well, a simple increment may not even point to a valid instruction now.
    // For example movl $0xdeadbeef,(%rax) is c700efbeadde, so an increment will point to a 00ef instruction.
    // YOLO

    printf("\tContinuing execution at %p\n", *rip);
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
    //*invalid_ptr = 0xdeadbeef;
    asm ("cli" : : :);
    // the segfault handler should print the instruction which triggers the segfault
    // confirm by: $ objdump -d a.out | grep 0xdeadbeef

    asm volatile ("" : : : "memory"); // barrier (prevent reordering)

    puts("All went well. Bye.");
}
