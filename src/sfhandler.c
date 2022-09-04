#include <sys/mman.h>
#include <signal.h>

#include "sfhandler.h"
#include "macros.h"

extern void handle_memory_update(int sig, siginfo_t *si, void *unused);

int register_sigsegv_handler()
{
    struct sigaction sa;

    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = handle_memory_update;

    if (sigaction(SIGSEGV, &sa, NULL) == -1)
        handle_error("failed to register handle_memory_update");

    return 0;
}
