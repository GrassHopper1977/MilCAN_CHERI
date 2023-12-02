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

// #define LOG_LEVEL 3
#include "utils/logs.h"
#include "utils/timestamp.h"
#include "utils/priorities.h"
#include "milcan.h"
// #include "interfaces.h"

#define TAG "test"
#define SLEEP_TIME_US 100
#define HEARTBEAT_PERIOD_MS 100


// struct milcan_a* interface = NULL;

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

// void tidyBeforeExit() {
//   milcan_close(interface);
// }

// void diep(const char *s) {
//   perror(s); 
//   tidyBeforeExit();
//   exit(EXIT_FAILURE);
// }

// void sigint_handler(int sig) {
//   printf("\n%s: Signal received (%i).\n", __FILE__, sig);
//   fflush(stdout);
//   fflush(stderr);
//   if(sig == SIGINT) {
//     tidyBeforeExit();
//     // Make sure the signal is passed down the line correctly.
//     signal(SIGINT, SIG_DFL);
//     kill(getpid(), SIGINT);
//   }
// }


// int testx(int argc, char *argv[])
// {
//   // // Create the signal handler here - ensures that Ctrl-C gets passed back up to 
//   // signal(SIGINT, sigint_handler);

//   int result;
//   uint8_t localAddress = 0;
//   uint8_t can_interface_type = CAN_INTERFACE_NONE;
//   uint16_t moduleNumber = 0;
  
//   printf("MilCAN Test\n\n");

//   // check argument count
//   if (argc != 3) {
//     fprintf(stderr, "usage: %s a<address> c<CANdo> | g<GSUSB>\n", argv[0]);
//     fprintf(stderr, "   <address> - The MilCAN device's ID in the range 1-255\n");
//     fprintf(stderr, "   <CANdo> - The module number of the CANdo module. They are normally numbered in the order that you plugged them in.\n");
//     fprintf(stderr, "   <GSUSB> - The module number of the GSUSB compatible module. They are normally numbered in the order that you plugged them in.\n");
//     fprintf(stderr, "Note: You should only attempt to connect to one module at a time. Including both options 'c' and 'g' may cause unexpected results.\n");
//     tidyBeforeExit();
//     exit(EXIT_FAILURE);
//   }
  
//   unsigned long tempLong;
//   // size_t tempLen;
//   for(int i = 1; i < argc; i++) {
//     switch(argv[i][0]) {
//     case 'a':
//       tempLong = strtoul(&argv[i][1], NULL, 10);
//       if((tempLong > 255) || (tempLong < 1)) {
//         LOGE(TAG, "The local address is invalid. You must define a device address between 0 and 255.");
//         tidyBeforeExit();
//         exit(EXIT_FAILURE);
//       }
//       localAddress = (uint8_t)tempLong;
//       break;
//     case 'c': // A CANdo module's number.
//       tempLong = strtoul(&argv[i][1], NULL, 10);
//       if((tempLong > 9) || (tempLong < 0)) {
//         LOGE(TAG, "The CANdo number is invalid. You must choose a device number between 0 and 9.");
//         tidyBeforeExit();
//         exit(EXIT_FAILURE);
//       }
//       moduleNumber = (uint16_t)tempLong;
//       LOGI(TAG, "Use CANdo device: %u", moduleNumber);
//       can_interface_type = CAN_INTERFACE_CANDO;
//       break;
//     case 'g': // A GSUSB module's number.
//       tempLong = strtoul(&argv[i][1], NULL, 10);
//       moduleNumber = (uint16_t)tempLong;
//       LOGI(TAG, "Use GSUSB device: %u", moduleNumber);
//       can_interface_type = CAN_INTERFACE_GSUSB_SO;
//       break;
//     default:
//       LOGE(TAG, "Unrecognised option '%c'!", argv[i][0]);
//       tidyBeforeExit();
//       exit(EXIT_FAILURE);
//       break;
//     }
//   }

//   if(localAddress == 0) {
//     LOGE(TAG, "The local address is invalid. You must define a device address between 0 and 255.");
//     tidyBeforeExit();
//     exit(EXIT_FAILURE);
//   }
//   if(can_interface_type == CAN_INTERFACE_NONE) {
//     LOGE(TAG, "The CAN connection point has not been defined. You must choose a method to talk to a CAN device.");
//     tidyBeforeExit();
//     exit(EXIT_FAILURE);
//   }
//   LOGI(TAG, "starting...");

//   interface = milcan_open(MILCAN_A_500K, MILCAN_A_500K_DEFAULT_SYNC_HZ, localAddress, can_interface_type, moduleNumber, 0);
//   if(interface == NULL) {
//     LOGE(TAG, "Unable to open interface.");
//     tidyBeforeExit();
//     exit(EXIT_FAILURE);
//   }
  
//   LOGI(TAG, "Starting loop...");
//   struct milcan_frame frame;
//   // uint64_t heartbeat_time = nanos() + MS_TO_NS(HEARTBEAT_PERIOD_MS); // Send the heartbeat signal every HEARTBEAT_PERIOD_MS ms.
//   // struct milcan_frame heartbeat_frame;
//   // heartbeat_frame.mortal = 0;
//   // heartbeat_frame.frame.can_id = MILCAN_MAKE_ID(1, 0, 11, 12, localAddress);
//   // heartbeat_frame.frame.len = 8;
//   // heartbeat_frame.frame.data[0] = 0x00;
//   // heartbeat_frame.frame.data[1] = 0x5A;
//   // heartbeat_frame.frame.data[2] = 0x01;
//   // heartbeat_frame.frame.data[3] = 0x23;
//   // heartbeat_frame.frame.data[4] = 0x45;
//   // heartbeat_frame.frame.data[5] = 0x67;
//   // heartbeat_frame.frame.data[6] = 0x89;
//   // heartbeat_frame.frame.data[7] = 0xab;
  
//   uint64_t config_mode_time = nanos() + SECS_TO_NS(2);
  
//   // loop forever
//   for (;;) {
//     result = milcan_recv(interface, &frame);
//     if(result < MILCAN_ERROR_FATAL) {
//       LOGE(TAG, "MILCAN_ERROR_FATAL...");
//       exit(EXIT_FAILURE);
//     } else if(result > 0) {
//       switch(frame.frame_type) {
//         case MILCAN_FRAME_TYPE_MESSAGE:
//           print_can_frame(TAG, &(frame.frame), 0, "Mesage In");
//           break;
//         case MILCAN_FRAME_TYPE_CHANGE_MODE:
//           milcan_display_mode(interface);
//           break;
//         case MILCAN_FRAME_TYPE_NEW_FRAME:
//           // printf("Frame: %000x\r", frame.frame.can_id);
//           break;
//         case MILCAN_FRAME_TYPE_CHANGE_SYNC_MASTER:
//           LOGI(TAG, "Sync Master is %02x.", frame.frame.can_id);
//           break;
//         default:
//           LOGE(TAG, "Unknown MilCAN frame type (0x%02x)", frame.frame_type);
//           break;
//       }
//     }
//     // if((current_mode == MILCAN_A_MODE_OPERATIONAL) && (nanos() > heartbeat_time)) {
//     //   // heartbeat_time = nanos() + MS_TO_NS(HEARTBEAT_PERIOD_MS);
//     //   heartbeat_time += MS_TO_NS(HEARTBEAT_PERIOD_MS);
//     //   milcan_send(interface, &heartbeat_frame);
//     //   heartbeat_frame.frame.data[0]++;
//     //   heartbeat_frame.frame.data[1] ^= 0xFF;
//     // }
//     if((config_mode_time > 0) && (nanos() > config_mode_time)) {
//       config_mode_time = 0; // Only fires once.
//       LOGI(TAG, "Ask to enter Config Mode.");
//       milcan_change_to_config_mode(interface);
//     }
//     usleep(SLEEP_TIME_US);
//   }

//   tidyBeforeExit();
//   return EXIT_SUCCESS;
// }

void* device0 = NULL;
void* device1 = NULL;
void* device2 = NULL;

void tidyTestsExit() {
  milcan_close(device0);
  milcan_close(device1);
  milcan_close(device2);
}

void sigint_handler(int sig) {
  printf("\n%s: Signal received (%i).\n", __FILE__, sig);
  fflush(stdout);
  fflush(stderr);
  if(sig == SIGINT) {
    tidyTestsExit();
    // Make sure the signal is passed down the line correctly.
    signal(SIGINT, SIG_DFL);
    kill(getpid(), SIGINT);
  }
}

struct timing {
  uint16_t totalSamples;
  float tolerance;
  uint64_t targetPeriod;
  uint64_t last;
  uint64_t min;
  uint64_t max;
  uint64_t average;
  uint64_t tolm3;
  uint64_t tolm2;
  uint64_t tolm1;
  uint64_t tolp1;
  uint64_t tolp2;
  uint64_t tolp3;
  uint16_t tollowcnt;
  uint16_t tolm3cnt;
  uint16_t tolm2cnt;
  uint16_t tolm1cnt;
  uint16_t tolp1cnt;
  uint16_t tolp2cnt;
  uint16_t tolp3cnt;
  uint16_t tolhighcnt;
};

void timingInit(struct timing* timing, uint64_t targetPeriod, float tolerance) {
  timing->totalSamples = 0;
  timing->tolerance = tolerance;
  timing->targetPeriod = targetPeriod;
  timing->average = 0;
  timing->last = 0;
  timing->max = 0;
  timing->min = 0;
  timing->tollowcnt = 0;
  timing->tolm3cnt = 0;
  timing->tolm2cnt = 0;
  timing->tolm1cnt = 0;
  timing->tolp1cnt = 0;
  timing->tolp2cnt = 0;
  timing->tolp3cnt = 0;
  timing->tolhighcnt = 0;
  timing->tolm3 = timing->targetPeriod - ((uint64_t)(timing->targetPeriod * tolerance) * 3);
  timing->tolm2 = timing->targetPeriod - ((uint64_t)(timing->targetPeriod * tolerance) * 2);
  timing->tolm1 = timing->targetPeriod - (uint64_t)(timing->targetPeriod * tolerance);
  timing->tolp1 = timing->targetPeriod + (uint64_t)(timing->targetPeriod * tolerance);
  timing->tolp2 = timing->targetPeriod + ((uint64_t)(timing->targetPeriod * tolerance) * 2);
  timing->tolp3 = timing->targetPeriod + ((uint64_t)(timing->targetPeriod * tolerance) * 3);
}

void timingRecord(struct timing* timing, uint64_t now) {
  uint64_t diff;
  if(timing->last > 0) { // The 1st record is ignored as we need a difference.
    timing->totalSamples++;
    diff = now - timing->last;
    if(timing->average == 0) {  // first value.
      timing->average = diff;
      timing->min = diff;
      timing->max = diff;
    } else {
      timing->average = (timing->average  + diff) / 2;  // Rolling average.
      if(diff < timing->min) timing->min = diff;
      if(diff > timing->max) timing->max = diff;
    }
    if(diff < timing->tolm3) {
      timing->tollowcnt++;
    } else if(diff < timing->tolm2) {
      timing->tolm3cnt++;
    } else if(diff < timing->tolm1) {
      timing->tolm2cnt++;
    } else if(diff < timing->targetPeriod) {
      timing->tolm1cnt++;
    } else if(diff <= timing->tolp1) {
      timing->tolp1cnt++;
    } else if(diff <= timing->tolp2) {
      timing->tolp2cnt++;
    } else if(diff <= timing->tolp3) {
      timing->tolp3cnt++;
    } else {
      timing->tolhighcnt++;
    }
  }
  timing->last = now;
}

void timingResults(struct timing* timing) {
  printf("    Total samples: %u\n", timing->totalSamples);
  printf("    Target period: %luns +/- %f%%\n", timing->targetPeriod, timing->tolerance);
  printf("Min Target period: %luns\n", timing->tolm1);
  printf("Max Target period: %luns\n", timing->tolp1);
  printf("   Average period: %luns\n", timing->average);
  printf("            < -3%%: %u\n", timing->tollowcnt);
  printf("     >= -3%% < -2%%: %u\n", timing->tolm3cnt);
  printf("     >= -2%% < -1%%: %u\n", timing->tolm2cnt);
  printf("      >= -1%% < 0%%: %u\n", timing->tolm1cnt);
  printf("      >= 0%% <= 1%%: %u\n", timing->tolp1cnt);
  printf("       > 1%% <= 2%%: %u\n", timing->tolp2cnt);
  printf("       > 2%% <= 3%%: %u\n", timing->tolp3cnt);
  printf("             > 3%%: %u\n", timing->tolhighcnt);
}

void printDeviceType(uint8_t devicetype) {
  switch(devicetype) {
    case CAN_INTERFACE_CANDO:
      printf("CANdo");
      break;
    case CAN_INTERFACE_GSUSB_SO:
      printf("GSUSB");
      break;
    default:
      printf("UNKNOWN");
      break;

  }
}

int test0(uint8_t testNo, uint16_t syncFreqHz, uint8_t device0heartbeatFreqHz, uint8_t device1heartbeatFreqHz, uint8_t device0type, uint8_t device0num, uint8_t device0addr, uint8_t device1type, uint8_t device1num, uint8_t device1addr) {
  int ret = EXIT_SUCCESS;
  struct milcan_frame framein0;
  struct milcan_frame framein1;
  int result;
  uint64_t total_sync_frames = 0;
  // uint16_t current_frame = 0;
  struct timing sync_timing;
  uint16_t noResults = 2000;

  uint64_t syncPeriodNs =  (uint64_t) (1000000000L/syncFreqHz);
  timingInit(&sync_timing, syncPeriodNs, 0.01);

  uint64_t device0heartbeatPeriodNs = 0;
  struct timing heartbeat0_timing;
  if(device0heartbeatFreqHz > 0)
    device0heartbeatPeriodNs =  (uint64_t) (1000000000L/device0heartbeatFreqHz);
  timingInit(&heartbeat0_timing, device0heartbeatPeriodNs, 0.1);
  uint64_t device1heartbeatPeriodNs = 0;
  struct timing heartbeat1_timing;
  if(device1heartbeatFreqHz > 0)
    device1heartbeatPeriodNs =  (uint64_t) (1000000000L/device1heartbeatFreqHz);
  timingInit(&heartbeat1_timing, device1heartbeatPeriodNs, 0.1);

  uint64_t heartbeat_time0 = nanos() + device0heartbeatPeriodNs;
  struct milcan_frame heartbeat0_frame;
  heartbeat0_frame.mortal = 0;
  heartbeat0_frame.frame.can_id = MILCAN_MAKE_ID(1, 0, 11, 12, device0addr);
  heartbeat0_frame.frame.len = 8;
  heartbeat0_frame.frame.data[0] = 0x00;
  heartbeat0_frame.frame.data[1] = 0x5A;
  heartbeat0_frame.frame.data[2] = 0x01;
  heartbeat0_frame.frame.data[3] = 0x23;
  heartbeat0_frame.frame.data[4] = 0x45;
  heartbeat0_frame.frame.data[5] = 0x67;
  heartbeat0_frame.frame.data[6] = 0x89;
  heartbeat0_frame.frame.data[7] = 0xab;

  uint64_t heartbeat_time1 = nanos() + device1heartbeatPeriodNs;
  struct milcan_frame heartbeat1_frame;
  heartbeat1_frame.mortal = 0;
  heartbeat1_frame.frame.can_id = MILCAN_MAKE_ID(1, 0, 11, 12, device1addr);
  heartbeat1_frame.frame.len = 8;
  heartbeat1_frame.frame.data[0] = 0x00;
  heartbeat1_frame.frame.data[1] = 0x5A;
  heartbeat1_frame.frame.data[2] = 0x01;
  heartbeat1_frame.frame.data[3] = 0x23;
  heartbeat1_frame.frame.data[4] = 0x45;
  heartbeat1_frame.frame.data[5] = 0x67;
  heartbeat1_frame.frame.data[6] = 0x89;
  heartbeat1_frame.frame.data[7] = 0xab;

  uint16_t current_mode = MILCAN_A_MODE_POWER_OFF;
  printf("Starting Test %u\n", testNo);

  // Device 0: Open the first CANdo device as non SYNC capable @ address 12.
  printf("Device 0: ");
  printDeviceType(device0type);
  printf(" %u on address %u with sync frequency of %uHz and heartbeat frequency of %uHz.\n", device0num, device0addr, syncFreqHz, device0heartbeatFreqHz);
  device0 = milcan_open(MILCAN_A_500K, syncFreqHz, device0addr, device0type, device0num, 0);
  if(device0 == NULL) {
    LOGE(TAG, "Unable to open device0.");
    tidyTestsExit();
    return EXIT_FAILURE;
  }

  // Device 1: Open the first GSUSB device as SYNC capable @ address 10.
  printf("Device 1: ");
  printDeviceType(device1type);
  printf(" %u on address %u with sync frequency of %uHz and heartbeat frequency of %uHz.\n", device1num, device1addr, syncFreqHz, device1heartbeatFreqHz);
  device1 = milcan_open(MILCAN_A_500K, syncFreqHz, device1addr, device1type, device1num,  MILCAN_A_OPTION_SYNC_MASTER);
  if(device1 == NULL) {
    LOGE(TAG, "Unable to open device1.");
    tidyTestsExit();
    return EXIT_FAILURE;
  }

  // Main Loop
  uint64_t test_started = nanos();
  while (total_sync_frames < (noResults + 1)) {
    result = milcan_recv(device0, &framein0);
    if(result <= MILCAN_ERROR_FATAL) {
      LOGE(TAG, "MILCAN_ERROR_FATAL...");
      tidyTestsExit();
      return EXIT_FAILURE;
    } else if(result > 0) {
      switch(framein0.frame_type) {
        case MILCAN_FRAME_TYPE_MESSAGE:
          // print_can_frame(TAG, &(framein0.frame), 0, "Mesage In");
          break;
        case MILCAN_FRAME_TYPE_CHANGE_MODE:
          milcan_display_mode(device0);
          break;
        case MILCAN_FRAME_TYPE_NEW_FRAME:
          // printf("Frame: %000x\r", framein0.frame.can_id);
          break;
        case MILCAN_FRAME_TYPE_CHANGE_SYNC_MASTER:
          LOGI(TAG, "Sync Master is %02x.", framein0.frame.can_id);
          break;
        default:
          LOGE(TAG, "Unknown MilCAN frame type (0x%02x)", framein0.frame_type);
          break;
      }
    }
    result = milcan_recv(device1, &framein1);
    if(result <= MILCAN_ERROR_FATAL) {
      LOGE(TAG, "MILCAN_ERROR_FATAL...");
      tidyTestsExit();
      return EXIT_FAILURE;
    } else if(result > 0) {
      switch(framein1.frame_type) {
        case MILCAN_FRAME_TYPE_MESSAGE:
          // print_can_frame(TAG, &(framein1.frame), 0, "Mesage In");
          break;
        case MILCAN_FRAME_TYPE_CHANGE_MODE:
          milcan_display_mode(device1);
          current_mode = framein1.frame.can_id;
          break;
        case MILCAN_FRAME_TYPE_NEW_FRAME:
          // LOGI(TAG, "Frame: %000x\r", framein1.frame.can_id);
          printf(".");
          fflush(stdout);
          // current_frame = framein1.frame.can_id;
          timingRecord(&sync_timing, nanos());
          total_sync_frames++;
          break;
        case MILCAN_FRAME_TYPE_CHANGE_SYNC_MASTER:
          LOGI(TAG, "Sync Master is %02x.", framein1.frame.can_id);
          break;
        default:
          LOGE(TAG, "Unknown MilCAN frame type (0x%02x)", framein1.frame_type);
          break;
      }
    }

    if((device0heartbeatFreqHz > 0) && (current_mode == MILCAN_A_MODE_OPERATIONAL) && (nanos() > heartbeat_time0)) {
      heartbeat_time0 += device0heartbeatPeriodNs;
      milcan_send(device0, &heartbeat0_frame);
      timingRecord(&heartbeat0_timing, nanos());
      heartbeat0_frame.frame.data[0]++;
      heartbeat0_frame.frame.data[1] ^= 0xFF;
    }

    if((device1heartbeatFreqHz > 0) && (current_mode == MILCAN_A_MODE_OPERATIONAL) && (nanos() > heartbeat_time1)) {
      heartbeat_time1 += device1heartbeatPeriodNs;
      milcan_send(device1, &heartbeat1_frame);
      timingRecord(&heartbeat1_timing, nanos());
      heartbeat1_frame.frame.data[0]++;
      heartbeat1_frame.frame.data[1] ^= 0xFF;
    }

  }

  milcan_close(device0);  // Device 0: Close
  milcan_close(device1);  // Device 1: Close
  device0 = NULL;
  device1 = NULL;
  printf("Test Finished\n");
  printf("Test took %luns\n", nanos() - test_started);
  printf("Total sync frames received: %lu\n", total_sync_frames);
  printf("Sync Frame Timing\n");
  timingResults(&sync_timing);
  if(device0heartbeatFreqHz > 0) {
    printf("Device 0 Heartbeat Timing\n");
    timingResults(&heartbeat0_timing);
  }
  if(device1heartbeatFreqHz > 0) {
    printf("Device 1 Heartbeat Timing\n");
    timingResults(&heartbeat1_timing);
  }
  return ret;
}

// Entry point
int main(int argc, char *argv[])
{
  int ret = EXIT_SUCCESS;
  // printf("MilCAN Interface Tests\n\n");
  // printf("This requires 2 x GSUSB compatible devices and 1 x CANdo compatible device.\n");
  // printf("This code will connect to the various devices and run a series of communication tests to varify the code is working as expected.\n");

  if(argc < 3) {
    printf("Incorrect argument count\n");
    printf("Usage:\n");
    printf("  tests_pc n t\n\n");
    printf("Where:\n");
    printf("  n = Test number to display.\n");
    printf("  t = Test type to conduct.\n");
    exit(EXIT_FAILURE);
  }

  int testNo = atoi(argv[1]);
  char testType = argv[2][0];

  // Create the signal handler here - ensures that Ctrl-C gets passed back up to 
  signal(SIGINT, sigint_handler);
  
  // Ensure that we're runnning at a high priority.
  // displayRTpriority();
  setRTpriority(0);

  // ret = testx(arc, argv);

  switch(testType) {
    case '0':
      ret = test0(testNo, MILCAN_A_500K_DEFAULT_SYNC_HZ, 0, 0, CAN_INTERFACE_CANDO, 0, 12, CAN_INTERFACE_GSUSB_SO, 0, 10);
      break;
    case '1':
      ret = test0(testNo, MILCAN_A_500K_DEFAULT_SYNC_HZ, 0, 0, CAN_INTERFACE_GSUSB_SO, 0, 12, CAN_INTERFACE_CANDO, 0, 10);
      break;
    case '2':
      ret = test0(testNo, MILCAN_A_500K_DEFAULT_SYNC_HZ, 10, 0, CAN_INTERFACE_GSUSB_SO, 0, 12, CAN_INTERFACE_CANDO, 0, 10);
      break;
    case '3':
      ret = test0(testNo, MILCAN_A_500K_DEFAULT_SYNC_HZ, 10, 0, CAN_INTERFACE_GSUSB_SO, 0, 12, CAN_INTERFACE_CANDO, 0, 10);
      break;
    case '4':
      ret = test0(testNo, MILCAN_A_500K_DEFAULT_SYNC_HZ, 10, 10, CAN_INTERFACE_GSUSB_SO, 0, 12, CAN_INTERFACE_CANDO, 0, 10);
      break;
    case '5':
      ret = test0(testNo, MILCAN_A_500K_DEFAULT_SYNC_HZ, 10, 10, CAN_INTERFACE_GSUSB_SO, 0, 12, CAN_INTERFACE_CANDO, 0, 10);
      break;
    default:
      printf("ERROR! Unknown test type.");
      ret = EXIT_FAILURE;
      break;
  }

  return ret;
}
