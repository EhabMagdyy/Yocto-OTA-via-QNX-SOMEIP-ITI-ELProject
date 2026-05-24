// For QNX, eventfd is not available, so we provide a simple stub implementation using a pipe.

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>

#define EFD_NONBLOCK  O_NONBLOCK
#define EFD_CLOEXEC   O_CLOEXEC
#define EFD_SEMAPHORE 1

int eventfd(unsigned int initval, int flags) {
    int fds[2];
    if (pipe(fds) < 0) return -1;
    if (flags & EFD_NONBLOCK) {
        fcntl(fds[0], F_SETFL, O_NONBLOCK);
        fcntl(fds[1], F_SETFL, O_NONBLOCK);
    }
    if (flags & EFD_CLOEXEC) {
        fcntl(fds[0], F_SETFD, FD_CLOEXEC);
        fcntl(fds[1], F_SETFD, FD_CLOEXEC);
    }
    close(fds[1]);
    return fds[0];
}
