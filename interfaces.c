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
#include <limits.h>
#include "interfaces.h"
#define LOG_LEVEL 3
#include "logs.h"
#include "timestamp.h"

#define TAG "Interfaces"


void print_milcan_frame(const char* tag, struct milcan_frame *frame, const char *format, ...) {
    FILE * fd = stdout;

    if(frame->frame.can_id & CAN_ERR_FLAG) {
        fd = stderr;
        fprintf(fd, "%lu: ERROR: %s: %s() line %i: %s: ID: ", nanos(), __FILE__, __FUNCTION__, __LINE__, tag);
    } else {
        fprintf(fd, "%lu:  INFO: %s: %s() line %i: %s: ID: ", nanos(), __FILE__, __FUNCTION__, __LINE__, tag);
    }

    if((frame->frame.can_id & CAN_EFF_FLAG) || (frame->frame.can_id & CAN_ERR_FLAG)) {
        fprintf(fd, "%08x", frame->frame.can_id & CAN_EFF_MASK);
    } else {
        fprintf(fd, "     %03x", frame->frame.can_id & CAN_SFF_MASK);
    }
    fprintf(fd, ", len: %2u", frame->frame.len);
    fprintf(fd, ", Data: ");
    for(int n = 0; n < CAN_MAX_DLC; n++) {
        if(n < frame->frame.len) {
            fprintf(fd, "%02x, ", frame->frame.data[n]);
        } else {
            fprintf(fd, "    ");
        }
    }

    if(frame->mortal > 0) {
        fprintf(fd, ", mortal: %10luns remaining", nanos() - frame->mortal);
    }

    va_list args;
    va_start(args, format);

    vfprintf(fd, format, args);
    va_end(args);

    if(frame->frame.can_id & CAN_ERR_FLAG) {
        fprintf(fd, ", ERROR FRAME");
    }

    fprintf(fd, "\n");
}

/// @brief Opens a CAN interface. We're trying to use the same interface functions for all the differnt types.
struct milcan_a* milcan_open(uint8_t speed, uint16_t sync_freq_hz, uint8_t sourceAddress, uint8_t can_interface_type, char* address, uint16_t port) {
    char rfifopath[PATH_MAX + 1] = {0x00};  // We will open the read FIFO here (if used).
    char wfifopath[PATH_MAX + 1] = {0x00};  // We will open the write FIFO here (if used).
    struct milcan_a* interface = calloc(1, sizeof(struct milcan_a));
    if(interface == NULL) {
        LOGE(TAG, "Memory shortage.");
    } else {
        interface->sourceAddress = sourceAddress;
        interface->can_interface_type = can_interface_type;
        interface->speed = speed;
        interface->sync = 0xFFFF;
        interface->syncTimer = nanos();
        interface->sync_freq_hz = sync_freq_hz;
        interface->sync_time_ns = (uint64_t) (1000000000L/sync_freq_hz);
        interface->rfdfifo = -1;
        interface->wfdfifo = -1;
        interface->mode = MILCAN_A_MODE_POWER_OFF;

        LOGI(TAG, "Sync Frame Frequency requested %u", interface->sync_freq_hz);
        LOGI(TAG, "Sync Frame period calculated %lu", interface->sync_time_ns);
 
        switch (can_interface_type)
        {
        case CAN_INTERFACE_GSUSB_FIFO:
            sprintf(rfifopath,"%sr", address);
            sprintf(wfifopath,"%sw", address);

            // Open the FIFOs
            LOGI(TAG, "Opening FIFO (%s)...", rfifopath);
            interface->rfdfifo = open(rfifopath, O_RDONLY | O_NONBLOCK);
            if(interface->rfdfifo < 0) {
                LOGE(TAG, "unable to open FIFO (%s) for Read. Error", rfifopath);
                interface = milcan_close(interface);
                break;
            }

            LOGI(TAG, "Opening FIFO (%s)...", wfifopath);
            interface->wfdfifo = open(wfifopath, O_WRONLY | O_NONBLOCK);
            if(interface->wfdfifo < 0) {
                LOGE(TAG, "unable to open FIFO (%s) for Write. Error", wfifopath);
                interface = milcan_close(interface);
                break;
            }

            // Connection is open so start the MILCAN process here.
            interface->mode = MILCAN_A_MODE_PRE_OPERATIONAL;
            break;
        
        default:
            LOGE(TAG, "CAN interface type is unrecognised or unsupported.");
            break;
        }
    }

    return interface;
}

struct milcan_a* milcan_close(struct milcan_a* interface) {
    LOGI(TAG, "Closing FIFOs...");
    if(interface->rfdfifo >= 0) {
        close(interface->rfdfifo);
    }
    if(interface->wfdfifo >= 0) {
        close(interface->wfdfifo);
    }
    LOGI(TAG, "Freeing memory...");
    free(interface);
    interface = NULL;
    LOGI(TAG, "Done.");
    return interface;
}

ssize_t milcan_send(struct milcan_a* interface, struct milcan_frame * frame) {
    ssize_t ret = -1;

    switch(interface->can_interface_type) {
    case CAN_INTERFACE_GSUSB_FIFO:
        print_milcan_frame(TAG, frame, "PIPE OUT");
        ret = write(interface->wfdfifo, &(frame->frame), sizeof(struct can_frame));
        break;
    
    default:
        LOGE(TAG, "CAN interface type is unrecognised or unsupported.");
        break;
    }
    return ret;
}

int milcan_recv(struct milcan_a* interface, int toRead) {
    int ret;
    int i = 0;
    struct milcan_frame frame;
    frame.mortal = 0;

    do {
        i++;
        ret = read(interface->rfdfifo, &(frame.frame), sizeof(struct can_frame));
        toRead -= ret;
        if(ret != sizeof(struct can_frame)) {
            LOGE(TAG, "Read %u bytes, expected %lu bytes!\n", ret, sizeof(struct can_frame));
        } else {
            print_milcan_frame(TAG, &frame, "PIPE IN");
        }
    } while (toRead >= sizeof(struct can_frame));

    return 0;
}

