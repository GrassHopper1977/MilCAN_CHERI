// milcan.c

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
#include <sys/rtprio.h>


#include "../BSD-USB-to-CAN/usb2can.h"
#define LOG_LEVEL 3
#include "utils/logs.h"
#include "utils/timestamp.h"
#include "utils/priorities.h"
#include "milcan.h"
#include "interfaces.h"

#define BUFSIZE 1024

// function prototypes
// int tcpopen(const char *host, int port);
// void sendbuftosck(int sckfd, const char *buf, int len);
int sendcantosck(int fd, struct milcan_frame * frame);

// #define KQUEUE_CH_SIZE 	1
// #define KQUEUE_EV_SIZE	10
#define TIMER_FD  1234

#define SLEEP_TIME  1000  // sleep time in us e.g. 1000 = 1ms

#define TAG "MilCAN"

struct milcan_a* interface = NULL;
char* pathOrAddress = NULL;

void print_can_frame(const char* tag, struct can_frame *frame, uint8_t err, const char *format, ...) {
  FILE * fd = stdout;

  if(err || (frame->can_id & CAN_ERR_FLAG)) {
    fd = stderr;
  }
  fprintf(fd, "%lu:  INFO: %s: %s() line %i: %s: ID: ", nanos(), __FILE__, __FUNCTION__, __LINE__, tag);

  if((frame->can_id & CAN_EFF_FLAG) || (frame->can_id & CAN_ERR_FLAG)) {
    fprintf(fd, "%08x", frame->can_id & CAN_EFF_MASK);
  } else {
    fprintf(fd, "     %03x", frame->can_id & CAN_SFF_MASK);
  }
  fprintf(fd, ", len: %2u", frame->len);
  fprintf(fd, ", Data: ");
  for(int n = 0; n < CAN_MAX_DLC; n++) {
    if(n < frame->len) {
      fprintf(fd, "%02x, ", frame->data[n]);
    } else {
      fprintf(fd, "    ");
    }
  }

  va_list args;
  va_start(args, format);

  vfprintf(fd, format, args);
  va_end(args);

  if(frame->can_id & CAN_ERR_FLAG) {
    fprintf(fd, ", ERROR FRAME");
  }

  fprintf(fd, "\n");
}

void tidyBeforeExit() {
  milcan_close(interface);
  if(pathOrAddress != NULL) {
    free(pathOrAddress);
  }
}

void diep(const char *s) {
  perror(s); 
  tidyBeforeExit();
  exit(EXIT_FAILURE);
}



void sigint_handler(int sig) {
  printf("\nSignal received (%i).\n", sig);
  fflush(stdout);
  fflush(stderr);
  if(sig == SIGINT) {
    tidyBeforeExit();
    // Make sure the signal is passed down the line correctly.
    signal(SIGINT, SIG_DFL);
    kill(getpid(), SIGINT);
  }
}

#define SYNC_PERIOD_NS  7812500L
#define SYNC_PERIOD_NS_1PC (uint64_t)(SYNC_PERIOD_NS * 0.01)

void checkSync(struct milcan_a* interface) {
  uint64_t now = nanos();

  // LOGI(TAG, "                 now: %lu", now);
  // LOGI(TAG, "interface->syncTimer: %lu", interface->syncTimer);
  
  if((interface->current_sync_master == interface->sourceAddress) // We are Sync Master
    || (((interface->options & MILCAN_A_OPTION_SYNC_MASTER) == MILCAN_A_OPTION_SYNC_MASTER) && 
    ((interface->current_sync_master == 0) || (interface->mode == MILCAN_A_MODE_PRE_OPERATIONAL)))) { // Or we want to be sync Master
    // if(now >= interface->syncTimer) {
    if(now >= (interface->syncTimer - SYNC_PERIOD_1PC(interface->sync_time_ns))) {
      interface->syncTimer = now + interface->sync_time_ns;  // Next period from now.
      // interface->syncTimer += interface->sync_time_ns; // Next period from when we should've been.

      // MilCAN Sync Frame
      interface->sync++;
      interface->sync &= 0x000003FF;
      struct milcan_frame frame = MILCAN_MAKE_SYNC(interface->sourceAddress, interface->sync);

      milcan_send(interface, &frame);
    }
  } else if(((interface->options & MILCAN_A_OPTION_SYNC_MASTER) == MILCAN_A_OPTION_SYNC_MASTER) && (interface->sourceAddress < interface->current_sync_master)) {  // We are not Sync Master but we have higher priority than the current Sync Master.
    if(now >= (interface->syncTimer - SYNC_PERIOD_20PC(interface->sync_time_ns))) { // We can interrupt at 0.8 of PTU if we have a higher priority.
      interface->syncTimer = now + interface->sync_time_ns;  // Next period from now.
      // interface->syncTimer += interface->sync_time_ns; // Next period from when we should've been.

      // MilCAN Sync Frame
      interface->sync++;
      interface->sync &= 0x000003FF;
      struct milcan_frame frame = MILCAN_MAKE_SYNC(interface->sourceAddress, interface->sync);

      milcan_send(interface, &frame);
    }
  } else {
    // We are not the Sync Master but we haven't had a Sync Message for 8 PTUs
    if(now >= (interface->syncTimer + (interface->sync_time_ns * 7))) {
      interface->current_sync_master = 0;
      interface->mode = MILCAN_A_MODE_PRE_OPERATIONAL;
    }
  }
}


int main(int argc, char *argv[])
{
  // Create the signal handler here - ensures that Ctrl-C gets passed back up to 
  signal(SIGINT, sigint_handler);

  // struct kevent chlist[KQUEUE_CH_SIZE]; // events we want to monitor
  // struct kevent evlist[KQUEUE_EV_SIZE]; // events that were triggered
  // int sckfd;
  // int kq, nev, i; 
  int result;
  uint8_t localAddress = 0;
  uint8_t can_interface_type = CAN_INTERFACE_NONE;
  uint16_t portNumber = 0;
  
  printf("MilCAN Implementation V0.0.2\n\n");

  displayRTpriority();
  setRTpriority(0);

  // check argument count
  if (argc != 3) {
    fprintf(stderr, "usage: %s a<address> f<FIFO> (e.g. /tmp/can0.0 Note: Omit the \"r\" or \"w\" on the end of the filename)\n", argv[0]);
    fprintf(stderr, "   <address> - The MilCAN device's ID in the range 1-255\n");
    fprintf(stderr, "   <FIFO> - e.g. /tmp/can0.0 Note: Omit the \"r\" or \"w\" on the end of the filename)\n");
    tidyBeforeExit();
    exit(EXIT_FAILURE);
  }
  
  unsigned long tempLong;
  size_t tempLen;
  for(int i = 1; i < argc; i++) {
    switch(argv[i][0]) {
    case 'a':
      tempLong = strtoul(&argv[i][1], NULL, 10);
      if((tempLong > 255) || (tempLong < 1)) {
        LOGE(TAG, "The local address is invalid. You must define a device address between 0 and 255.");
        tidyBeforeExit();
        exit(EXIT_FAILURE);
      }
      localAddress = (uint8_t)tempLong;
      break;
    case 'f': // A FIFO address.
      tempLen = strnlen(&argv[i][1], PATH_MAX);
      pathOrAddress = calloc(1, tempLen + 1);
      if(pathOrAddress == NULL) {
        LOGE(TAG, "Memory error.");
        tidyBeforeExit();
        exit(EXIT_FAILURE);
      }
      // LOGI(TAG, "tempLen: %zu", tempLen);
      snprintf(pathOrAddress, tempLen + 1, "%s", &argv[i][1]);
      LOGI(TAG, "pathOrAddress: %s", pathOrAddress);
      can_interface_type = CAN_INTERFACE_GSUSB_FIFO;
      break;
    case 'c': // A CANdo modeule's number.
      tempLong = strtoul(&argv[i][1], NULL, 10);
      if((tempLong > 9) || (tempLong < 0)) {
        LOGE(TAG, "The CANdo number is invalid. You must choose a device number between 0 and 9.");
        tidyBeforeExit();
        exit(EXIT_FAILURE);
      }
      portNumber = (uint16_t)tempLong;
      LOGI(TAG, "Use CANdo device: %u", portNumber);
      can_interface_type = CAN_INTERFACE_CANDO;
      break;
    default:
      LOGE(TAG, "Unrecognised option '%c'!", argv[i][0]);
      tidyBeforeExit();
      exit(EXIT_FAILURE);
      break;
    }
  }

  if(localAddress == 0) {
    LOGE(TAG, "The local address is invalid. You must define a device address between 0 and 255.");
    tidyBeforeExit();
    exit(EXIT_FAILURE);
  }
  if((can_interface_type == CAN_INTERFACE_NONE) || ((can_interface_type == CAN_INTERFACE_GSUSB_FIFO) && (pathOrAddress == NULL))) {
    LOGE(TAG, "The CAN connection point has not been defined. You must choose a method to talk to a CAN device.");
    tidyBeforeExit();
    exit(EXIT_FAILURE);
  }
  LOGI(TAG, "starting...");

  // // open a connection to a host:port pair
  // sckfd = tcpopen(argv[1], atoi(argv[2]));

  interface = milcan_open(MILCAN_A_500K, MILCAN_A_500K_DEFAULT_SYNC_HZ, localAddress, can_interface_type, pathOrAddress, portNumber, MILCAN_A_OPTION_SYNC_MASTER);
  if(interface == NULL) {
    tidyBeforeExit();
    exit(EXIT_FAILURE);
  }

  // if(can_interface_type == CAN_INTERFACE_GSUSB_FIFO) {
  //   // create a new kernel event queue
  //   if ((kq = kqueue()) == -1) {
  //     LOGE(TAG, "Unable to create kqueue.");
  //     tidyBeforeExit();
  //     exit(EXIT_FAILURE);
  //   }

  //   // initialise kevent structures
  //   // EV_SET(&chlist[0], sckfd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, 0);
  //   EV_SET(&chlist[0], interface->rfdfifo, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, 0);
  //   nev = kevent(kq, chlist, KQUEUE_CH_SIZE, NULL, 0, NULL);
  //   if (nev < 0) {
  //     LOGE(TAG, "Unable to listen to kqueue.");
  //     tidyBeforeExit();
  //     exit(EXIT_FAILURE);
  //   }
  // }

  // struct timespec zero_ts = {
  //   .tv_sec = 0,
  //   .tv_nsec = 0
  // };
  
  printf("Starting loop...\n");
  // loop forever
  for (;;) {
    checkSync(interface);
    result = milcan_recv(interface);
    if(result < MILCAN_ERROR_FATAL) {
      exit(EXIT_FAILURE);
    }

    // switch(can_interface_type) {
    //   case CAN_INTERFACE_GSUSB_FIFO:
    //     // checkTimer(&timer, interface->wfdfifo, localAddress, &syncCounter);
    //     nev = kevent(kq, NULL, 0, evlist, KQUEUE_EV_SIZE, &zero_ts);  // Non-blocking

    //     if (nev < 0) {
    //       LOGE(TAG, "%s() Unable to listen to kqueue.", __FUNCTION__);
    //       tidyBeforeExit();
    //       exit(EXIT_FAILURE);
    //     }
    //     else if (nev > 0) {
    //       for (i = 0; i < nev; i++) {
    //         if (evlist[i].flags & EV_EOF) {
    //           LOGE(TAG, "Read direction of socket has shutdown.");
    //           tidyBeforeExit();
    //           exit(EXIT_FAILURE);
    //         }

    //         if (evlist[i].flags & EV_ERROR) {                /* report errors */
    //           LOGE(TAG, "EV_ERROR: %s.", strerror(evlist[i].data));
    //           tidyBeforeExit();
    //           exit(EXIT_FAILURE);
    //         }
      
    //         if (evlist[i].ident == interface->rfdfifo) {                  /* we have data from the host */
    //           milcan_recv(interface, (int)(evlist[i].data));
    //         }
    //       }
    //     }
    //     break;
    //   case CAN_INTERFACE_CANDO:
    //     result = milcan_recv(interface, -1);
    //     if(result < MILCAN_ERROR_FATAL) {
    //       exit(EXIT_FAILURE);
    //     }
    //     break;
    // }
    usleep(SLEEP_TIME);
  }

  // close(kq);
  tidyBeforeExit();
  return EXIT_SUCCESS;
}

int sendcantosck(int fd, struct milcan_frame * frame) {
  print_can_frame(TAG, &(frame->frame), 0, "PIPE OUT");
  return write(fd, &(frame->frame), sizeof(struct can_frame));
}
