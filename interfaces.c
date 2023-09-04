// interfaces.c
#include <stdio.h>      /* Standard input/output definitions */
#include <string.h>     /* String function definitions */
#include <unistd.h>     /* UNIX standard function definitions */
#include <fcntl.h>      /* File control definitions */
#include <errno.h>      /* Error number definitions */
#include <termios.h>    /* POSIX terminal control definitions */
#include <sys/socket.h> // Sockets
#include <netinet/in.h>
#include <sys/un.h>     // ?
#include <sys/event.h>  // Events
#include <assert.h>     // The assert function
#include <unistd.h>     // ?
#include <stdint.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <netdb.h>
#include <signal.h>
#include <stdarg.h>
#include <inttypes.h>
#include "interfaces.h"

struct milcan_a* milcan_open(uint8_t speed, uint16_t sync_freq_hz) {
    return NULL;
}

int milcan_close(struct milcan_a* milcan_a) {
    return 0;
}

int milcan_send(struct milcan_a* milcan_a) {
    return 0;
}

int milcan_recv(struct milcan_a* milcan_a) {
    return 0;
}

