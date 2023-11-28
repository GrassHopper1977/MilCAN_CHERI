// test.c
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

#define LOG_LEVEL 3
#include "utils/logs.h"
#include "utils/timestamp.h"
#include "utils/priorities.h"
#include "milcan.h"
// #include "interfaces.h"

#define TAG "test"
#define SLEEP_TIME_US 100
#define HEARTBEAT_PERIOD_MS 100


struct milcan_a* interface = NULL;

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
}

void diep(const char *s) {
  perror(s); 
  tidyBeforeExit();
  exit(EXIT_FAILURE);
}

void sigint_handler(int sig) {
  printf("\n%s: Signal received (%i).\n", __FILE__, sig);
  fflush(stdout);
  fflush(stderr);
  if(sig == SIGINT) {
    tidyBeforeExit();
    // Make sure the signal is passed down the line correctly.
    signal(SIGINT, SIG_DFL);
    kill(getpid(), SIGINT);
  }
}


int main(int argc, char *argv[])
{
  // Create the signal handler here - ensures that Ctrl-C gets passed back up to 
  signal(SIGINT, sigint_handler);

  int result;
  uint8_t localAddress = 0;
  uint8_t can_interface_type = CAN_INTERFACE_NONE;
  uint16_t moduleNumber = 0;
  
  printf("MilCAN Test\n\n");

  displayRTpriority();
  setRTpriority(0);

  // check argument count
  if (argc != 3) {
    fprintf(stderr, "usage: %s a<address> c<CANdo> | g<GSUSB>\n", argv[0]);
    fprintf(stderr, "   <address> - The MilCAN device's ID in the range 1-255\n");
    fprintf(stderr, "   <CANdo> - The module number of the CANdo module. They are normally numbered in the order that you plugged them in.\n");
    fprintf(stderr, "   <GSUSB> - The module number of the GSUSB compatible module. They are normally numbered in the order that you plugged them in.\n");
    fprintf(stderr, "Note: You should only attempt to connect to one module at a time. Including both options 'c' and 'g' may cause unexpected results.\n");
    tidyBeforeExit();
    exit(EXIT_FAILURE);
  }
  
  unsigned long tempLong;
  // size_t tempLen;
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
    case 'c': // A CANdo module's number.
      tempLong = strtoul(&argv[i][1], NULL, 10);
      if((tempLong > 9) || (tempLong < 0)) {
        LOGE(TAG, "The CANdo number is invalid. You must choose a device number between 0 and 9.");
        tidyBeforeExit();
        exit(EXIT_FAILURE);
      }
      moduleNumber = (uint16_t)tempLong;
      LOGI(TAG, "Use CANdo device: %u", moduleNumber);
      can_interface_type = CAN_INTERFACE_CANDO;
      break;
    case 'g': // A GSUSB module's number.
      tempLong = strtoul(&argv[i][1], NULL, 10);
      moduleNumber = (uint16_t)tempLong;
      LOGI(TAG, "Use GSUSB device: %u", moduleNumber);
      can_interface_type = CAN_INTERFACE_GSUSB_SO;
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
  if(can_interface_type == CAN_INTERFACE_NONE) {
    LOGE(TAG, "The CAN connection point has not been defined. You must choose a method to talk to a CAN device.");
    tidyBeforeExit();
    exit(EXIT_FAILURE);
  }
  LOGI(TAG, "starting...");

  interface = milcan_open(MILCAN_A_500K, MILCAN_A_500K_DEFAULT_SYNC_HZ, localAddress, can_interface_type, moduleNumber, 0);
  if(interface == NULL) {
    LOGE(TAG, "Unable to open interface.");
    tidyBeforeExit();
    exit(EXIT_FAILURE);
  }
  
  LOGI(TAG, "Starting loop...");
  struct milcan_frame frame;
  // uint64_t heartbeat_time = nanos() + MS_TO_NS(HEARTBEAT_PERIOD_MS); // Send the heartbeat signal every HEARTBEAT_PERIOD_MS ms.
  // struct milcan_frame heartbeat_frame;
  // heartbeat_frame.mortal = 0;
  // heartbeat_frame.frame.can_id = MILCAN_MAKE_ID(1, 0, 11, 12, localAddress);
  // heartbeat_frame.frame.len = 8;
  // heartbeat_frame.frame.data[0] = 0x00;
  // heartbeat_frame.frame.data[1] = 0x5A;
  // heartbeat_frame.frame.data[2] = 0x01;
  // heartbeat_frame.frame.data[3] = 0x23;
  // heartbeat_frame.frame.data[4] = 0x45;
  // heartbeat_frame.frame.data[5] = 0x67;
  // heartbeat_frame.frame.data[6] = 0x89;
  // heartbeat_frame.frame.data[7] = 0xab;
  
  uint64_t config_mode_time = nanos() + SECS_TO_NS(2);
  
  // loop forever
  for (;;) {
    result = milcan_recv(interface, &frame);
    if(result < MILCAN_ERROR_FATAL) {
      LOGE(TAG, "MILCAN_ERROR_FATAL...");
      exit(EXIT_FAILURE);
    } else if(result > 0) {
      switch(frame.frame_type) {
        case MILCAN_FRAME_TYPE_MESSAGE:
          print_can_frame(TAG, &(frame.frame), 0, "Mesage In");
          break;
        case MILCAN_FRAME_TYPE_CHANGE_MODE:
          milcan_display_mode(interface);
          break;
        case MILCAN_FRAME_TYPE_NEW_FRAME:
          // printf("Frame: %000x\r", frame.frame.can_id);
          break;
        case MILCAN_FRAME_TYPE_CHANGE_SYNC_MASTER:
          LOGI(TAG, "Sync Master is %02x.", frame.frame.can_id);
          break;
        default:
          LOGE(TAG, "Unknown MilCAN frame type (0x%02x)", frame.frame_type);
          break;
      }
    }
    // if(nanos() > heartbeat_time) {
    //   // heartbeat_time = nanos() + MS_TO_NS(HEARTBEAT_PERIOD_MS);
    //   heartbeat_time += MS_TO_NS(HEARTBEAT_PERIOD_MS);
    //   milcan_send(interface, &heartbeat_frame);
    //   heartbeat_frame.frame.data[0]++;
    //   heartbeat_frame.frame.data[1] ^= 0xFF;
    // }
    if((config_mode_time > 0) && (nanos() > config_mode_time)) {
      config_mode_time = 0; // Only fires once.
      LOGI(TAG, "Ask to enter Config Mode.");
      milcan_change_to_config_mode(interface);
    }
    usleep(SLEEP_TIME_US);
  }

  tidyBeforeExit();
  return EXIT_SUCCESS;
}

