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
      // printf("  total samples = %u  \n", timing->totalSamples);
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
  printf("    Total samples:\t%u\n", timing->totalSamples);
  printf("    Target period:\t%luns +/-%i%%\n", timing->targetPeriod, (uint16_t)(100 * timing->tolerance));
  printf("Min Target period:\t%luns\n", timing->tolm1);
  printf("Max Target period:\t%luns\n", timing->tolp1);
  printf("   Average period:\t%luns\n", timing->average);
  printf("< %0.01f%%:\t\t%u\n", (-3 * timing->tolerance * 100), timing->tollowcnt);
  printf(">= %0.01f%% < %0.01f%%:\t%u\n", (-3 * timing->tolerance * 100), (-2 * timing->tolerance * 100), timing->tolm3cnt);
  printf(">= %0.01f%% < %0.01f%%:\t%u\n", (-2 * timing->tolerance) * 100, (-1 * timing->tolerance * 100), timing->tolm2cnt);
  printf(">= %0.01f%% < 0%%:\t\t%u\n", (-1 * timing->tolerance * 100), timing->tolm1cnt);
  printf(">= 0%% <= %0.01f%%:\t\t%u\n", timing->tolerance * 100, timing->tolp1cnt);
  printf("> %0.01f%% <= %0.01f%%:\t%u\n", timing->tolerance * 100, (2 * timing->tolerance * 100), timing->tolp2cnt);
  printf("> %0.01f%% <= %0.01f%%:\t%u\n", (2 * timing->tolerance * 100), (3 * timing->tolerance * 100), timing->tolp3cnt);
  printf("> %0.01f%%:\t\t%u\n", (3 * timing->tolerance * 100), timing->tolhighcnt);
}

int timingPass(struct timing* timing, double passPercent) {
  int ret = EXIT_SUCCESS;
  uint32_t passes = timing->tolm1cnt + timing->tolp1cnt;
  uint32_t fails = timing->tollowcnt + timing->tolm3cnt + timing->tolm2cnt + timing->tolp2cnt + timing->tolp3cnt + timing->tolhighcnt;
  // float pcPasses = (100.0 / timing->totalSamples) * passes;
  // float pcFails = (100.0 / timing->totalSamples) * fails;
  double pcPasses = ((double)passes) / timing->totalSamples;
  double pcFails = ((double)fails) / timing->totalSamples;
  printf("Total Passes: %0.02f%% (%u/%u)\n", pcPasses * 100, passes, timing->totalSamples);
  printf(" Total Fails: %0.02f%% (%u/%u)\n", pcFails * 100, fails, timing->totalSamples);
  if(pcPasses > passPercent) {
    // printf("Test passed.\n");
  } else {
    // printf("Test passed.\n");
    ret = EXIT_FAILURE;
  }
  return ret;
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
  heartbeat_time0 = nanos() + device0heartbeatPeriodNs;
  heartbeat_time1 = nanos() + device1heartbeatPeriodNs;
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
  if(EXIT_FAILURE == timingPass(&sync_timing, 0.95)) {
    ret = EXIT_FAILURE;
  }
  if(device0heartbeatFreqHz > 0) {
    printf("Device 0 Heartbeat Timing\n");
    timingResults(&heartbeat0_timing);
    if(EXIT_FAILURE == timingPass(&heartbeat0_timing, 0.95)) {
      ret = EXIT_FAILURE;
    }
  }
  if(device1heartbeatFreqHz > 0) {
    printf("Device 1 Heartbeat Timing\n");
    timingResults(&heartbeat1_timing);
    if(EXIT_FAILURE == timingPass(&heartbeat1_timing, 0.95)) {
      ret = EXIT_FAILURE;
    }
  }
  return ret;
}

int test1(uint8_t testNo, uint16_t syncFreqHz,
  uint8_t device0type, uint8_t device0num, uint8_t device0addr, uint8_t device0heartbeatFreqHz, 
  uint8_t device1type, uint8_t device1num, uint8_t device1addr, uint8_t device1heartbeatFreqHz, 
  uint8_t device2type, uint8_t device2num, uint8_t device2addr, uint8_t device2heartbeatFreqHz) {
  
  int ret = EXIT_SUCCESS;
  int result;
  uint64_t total_sync_frames = 0;
  struct timing sync_timing;
  uint16_t noResults = 2000;
  uint64_t syncPeriodNs =  (uint64_t) (1000000000L/syncFreqHz);
  timingInit(&sync_timing, syncPeriodNs, 0.01);

  // Device 0
  struct milcan_frame framein0;
  uint64_t device0heartbeatPeriodNs = 0;
  struct timing heartbeat0_timing;
  if(device0heartbeatFreqHz > 0)
    device0heartbeatPeriodNs =  (uint64_t) (1000000000L/device0heartbeatFreqHz);
  timingInit(&heartbeat0_timing, device0heartbeatPeriodNs, 0.1);
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

  // Device 1
  struct milcan_frame framein1;
  uint64_t device1heartbeatPeriodNs = 0;
  struct timing heartbeat1_timing;
  if(device1heartbeatFreqHz > 0)
    device1heartbeatPeriodNs =  (uint64_t) (1000000000L/device1heartbeatFreqHz);
  timingInit(&heartbeat1_timing, device1heartbeatPeriodNs, 0.1);
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

    // Device 2
  struct milcan_frame framein2;
  uint64_t device2heartbeatPeriodNs = 0;
  struct timing heartbeat2_timing;
  if(device2heartbeatFreqHz > 0)
    device2heartbeatPeriodNs =  (uint64_t) (1000000000L/device2heartbeatFreqHz);
  timingInit(&heartbeat2_timing, device2heartbeatPeriodNs, 0.1);
  uint64_t heartbeat_time2 = nanos() + device2heartbeatPeriodNs;
  struct milcan_frame heartbeat2_frame;
  heartbeat2_frame.mortal = 0;
  heartbeat2_frame.frame.can_id = MILCAN_MAKE_ID(1, 0, 11, 12, device2addr);
  heartbeat2_frame.frame.len = 8;
  heartbeat2_frame.frame.data[0] = 0x00;
  heartbeat2_frame.frame.data[1] = 0x5A;
  heartbeat2_frame.frame.data[2] = 0x01;
  heartbeat2_frame.frame.data[3] = 0x23;
  heartbeat2_frame.frame.data[4] = 0x45;
  heartbeat2_frame.frame.data[5] = 0x67;
  heartbeat2_frame.frame.data[6] = 0x89;
  heartbeat2_frame.frame.data[7] = 0xab;

  uint16_t current_mode = MILCAN_A_MODE_POWER_OFF;
  printf("Starting Test %u\n", testNo);
  printf("Connect three devices and check that the sync is at the correct speed and the heartbeat signals are transmitted.\n");

  // Device 0: Open the first device as non SYNC capable.
  printf("Device 0: ");
  printDeviceType(device0type);
  printf(" %u on address %u with sync frequency of %uHz and heartbeat frequency of %uHz.\n", device0num, device0addr, syncFreqHz, device0heartbeatFreqHz);
  device0 = milcan_open(MILCAN_A_500K, syncFreqHz, device0addr, device0type, device0num, MILCAN_A_OPTION_SYNC_MASTER);
  if(device0 == NULL) {
    LOGE(TAG, "Unable to open device 0.");
    tidyTestsExit();
    return EXIT_FAILURE;
  }

  // Device 1: Open the second device as SYNC capable.
  printf("Device 1: ");
  printDeviceType(device1type);
  printf(" %u on address %u with sync frequency of %uHz and heartbeat frequency of %uHz.\n", device1num, device1addr, syncFreqHz, device1heartbeatFreqHz);
  device1 = milcan_open(MILCAN_A_500K, syncFreqHz, device1addr, device1type, device1num, MILCAN_A_OPTION_SYNC_MASTER);
  if(device1 == NULL) {
    LOGE(TAG, "Unable to open device 1.");
    tidyTestsExit();
    return EXIT_FAILURE;
  }

  // Device 2: Open the third device as SYNC capable at a higher priority.
  printf("Device 2: ");
  printDeviceType(device2type);
  printf(" %u on address %u with sync frequency of %uHz and heartbeat frequency of %uHz.\n", device2num, device2addr, syncFreqHz, device2heartbeatFreqHz);
  device2 = milcan_open(MILCAN_A_500K, syncFreqHz, device2addr, device2type, device2num, MILCAN_A_OPTION_SYNC_MASTER);
  if(device2 == NULL) {
    LOGE(TAG, "Unable to open device 2.");
    tidyTestsExit();
    return EXIT_FAILURE;
  }

  // Main Loop
  heartbeat_time0 = nanos() + device0heartbeatPeriodNs;
  heartbeat_time1 = nanos() + device1heartbeatPeriodNs;
  uint64_t test_started = nanos();
  while (total_sync_frames < (noResults + 1)) {
    result = milcan_recv(device0, &framein0);
    if(result <= MILCAN_ERROR_FATAL) {
      LOGE(TAG, "Device 0: MILCAN_ERROR_FATAL...");
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
          LOGI(TAG, "Device 0: Sync Master is %02x.", framein0.frame.can_id);
          break;
        default:
          LOGE(TAG, "Device 0: Unknown MilCAN frame type (0x%02x)", framein0.frame_type);
          break;
      }
    }
    result = milcan_recv(device1, &framein1);
    if(result <= MILCAN_ERROR_FATAL) {
      LOGE(TAG, "Device 1: MILCAN_ERROR_FATAL...");
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
          LOGI(TAG, "Device 1: Sync Master is %02x.", framein1.frame.can_id);
          break;
        default:
          LOGE(TAG, "Device 1: Unknown MilCAN frame type (0x%02x)", framein1.frame_type);
          break;
      }
    }
    result = milcan_recv(device2, &framein2);
    if(result <= MILCAN_ERROR_FATAL) {
      LOGE(TAG, "Device 2: MILCAN_ERROR_FATAL...");
      tidyTestsExit();
      return EXIT_FAILURE;
    } else if(result > 0) {
      switch(framein2.frame_type) {
        case MILCAN_FRAME_TYPE_MESSAGE:
          // print_can_frame(TAG, &(framein1.frame), 0, "Mesage In");
          break;
        case MILCAN_FRAME_TYPE_CHANGE_MODE:
          milcan_display_mode(device2);
          current_mode = framein2.frame.can_id;
          break;
        case MILCAN_FRAME_TYPE_NEW_FRAME:
          // LOGI(TAG, "Frame: %000x\r", framein1.frame.can_id);
          // printf(".");
          // fflush(stdout);
          // timingRecord(&sync_timing, nanos());
          // total_sync_frames++;
          break;
        case MILCAN_FRAME_TYPE_CHANGE_SYNC_MASTER:
          LOGI(TAG, "Device 2: Sync Master is %02x.", framein2.frame.can_id);
          break;
        default:
          LOGE(TAG, "Device 2: Unknown MilCAN frame type (0x%02x)", framein2.frame_type);
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

    if((device2heartbeatFreqHz > 0) && (current_mode == MILCAN_A_MODE_OPERATIONAL) && (nanos() > heartbeat_time2)) {
      heartbeat_time2 += device2heartbeatPeriodNs;
      milcan_send(device2, &heartbeat2_frame);
      timingRecord(&heartbeat2_timing, nanos());
      heartbeat2_frame.frame.data[0]++;
      heartbeat2_frame.frame.data[1] ^= 0xFF;
    }

  }

  milcan_close(device0);  // Device 0: Close
  device0 = NULL;
  milcan_close(device1);  // Device 1: Close
  device1 = NULL;
  milcan_close(device2);  // Device 2: Close
  device2 = NULL;
  printf("Test Finished\n");
  printf("Test took %luns\n", nanos() - test_started);
  printf("Total sync frames received: %lu\n", total_sync_frames);
  printf("Sync Frame Timing\n");
  timingResults(&sync_timing);
  if(EXIT_FAILURE == timingPass(&sync_timing, 0.95)) {
    ret = EXIT_FAILURE;
  }
  if(device0heartbeatFreqHz > 0) {
    printf("Device 0 Heartbeat Timing\n");
    timingResults(&heartbeat0_timing);
    if(EXIT_FAILURE == timingPass(&heartbeat0_timing, 0.95)) {
      ret = EXIT_FAILURE;
    }
  }
  if(device1heartbeatFreqHz > 0) {
    printf("Device 1 Heartbeat Timing\n");
    timingResults(&heartbeat1_timing);
    if(EXIT_FAILURE == timingPass(&heartbeat1_timing, 0.95)) {
      ret = EXIT_FAILURE;
    }
  }
  if(device2heartbeatFreqHz > 0) {
    printf("Device 2 Heartbeat Timing\n");
    timingResults(&heartbeat2_timing);
    if(EXIT_FAILURE == timingPass(&heartbeat2_timing, 0.95)) {
      ret = EXIT_FAILURE;
    }
  }
  return ret;
}

int test2(uint8_t testNo, uint16_t syncFreqHz,
  uint8_t device0type, uint8_t device0num, uint8_t device0addr, uint8_t device0heartbeatFreqHz, 
  uint8_t device1type, uint8_t device1num, uint8_t device1addr, uint8_t device1heartbeatFreqHz, 
  uint8_t device2type, uint8_t device2num, uint8_t device2addr, uint8_t device2heartbeatFreqHz) {
  
  int ret = EXIT_SUCCESS;
  int result;
  uint64_t total_sync_frames = 0;
  // struct timing sync_timing;
  uint8_t current_sync_master = 0;
  uint16_t noResults = 2000;
  // uint64_t syncPeriodNs =  (uint64_t) (1000000000L/syncFreqHz);
  // timingInit(&sync_timing, syncPeriodNs, 0.01);

  // Device 0
  struct milcan_frame framein0;
  uint64_t device0heartbeatPeriodNs = 0;
  // struct timing heartbeat0_timing;
  if(device0heartbeatFreqHz > 0)
    device0heartbeatPeriodNs =  (uint64_t) (1000000000L/device0heartbeatFreqHz);
  // timingInit(&heartbeat0_timing, device0heartbeatPeriodNs, 0.1);
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

  // Device 1
  struct milcan_frame framein1;
  uint64_t device1heartbeatPeriodNs = 0;
  // struct timing heartbeat1_timing;
  if(device1heartbeatFreqHz > 0)
    device1heartbeatPeriodNs =  (uint64_t) (1000000000L/device1heartbeatFreqHz);
  // timingInit(&heartbeat1_timing, device1heartbeatPeriodNs, 0.1);
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

    // Device 2
  struct milcan_frame framein2;
  uint64_t device2heartbeatPeriodNs = 0;
  // struct timing heartbeat2_timing;
  if(device2heartbeatFreqHz > 0)
    device2heartbeatPeriodNs =  (uint64_t) (1000000000L/device2heartbeatFreqHz);
  // timingInit(&heartbeat2_timing, device2heartbeatPeriodNs, 0.1);
  uint64_t heartbeat_time2 = nanos() + device2heartbeatPeriodNs;
  struct milcan_frame heartbeat2_frame;
  heartbeat2_frame.mortal = 0;
  heartbeat2_frame.frame.can_id = MILCAN_MAKE_ID(1, 0, 11, 12, device2addr);
  heartbeat2_frame.frame.len = 8;
  heartbeat2_frame.frame.data[0] = 0x00;
  heartbeat2_frame.frame.data[1] = 0x5A;
  heartbeat2_frame.frame.data[2] = 0x01;
  heartbeat2_frame.frame.data[3] = 0x23;
  heartbeat2_frame.frame.data[4] = 0x45;
  heartbeat2_frame.frame.data[5] = 0x67;
  heartbeat2_frame.frame.data[6] = 0x89;
  heartbeat2_frame.frame.data[7] = 0xab;

  uint16_t current_mode = MILCAN_A_MODE_POWER_OFF;
  printf("Starting Test %u\n", testNo);
  printf("Connect the first two devices then add the 3rd device after 5 seconds. 3rd device is expected to become Sync Master.\n");

  // Device 0: Open the first device as SYNC capable.
  printf("Device 0: ");
  printDeviceType(device0type);
  printf(" %u on address %u with sync frequency of %uHz and heartbeat frequency of %uHz.\n", device0num, device0addr, syncFreqHz, device0heartbeatFreqHz);
  device0 = milcan_open(MILCAN_A_500K, syncFreqHz, device0addr, device0type, device0num, MILCAN_A_OPTION_SYNC_MASTER);
  if(device0 == NULL) {
    LOGE(TAG, "Unable to open device 0.");
    tidyTestsExit();
    return EXIT_FAILURE;
  }

  // Device 1: Open the second device as SYNC capable.
  printf("Device 1: ");
  printDeviceType(device1type);
  printf(" %u on address %u with sync frequency of %uHz and heartbeat frequency of %uHz.\n", device1num, device1addr, syncFreqHz, device1heartbeatFreqHz);
  device1 = milcan_open(MILCAN_A_500K, syncFreqHz, device1addr, device1type, device1num, MILCAN_A_OPTION_SYNC_MASTER);
  if(device1 == NULL) {
    LOGE(TAG, "Unable to open device 1.");
    tidyTestsExit();
    return EXIT_FAILURE;
  }

  // Main Loop
  heartbeat_time0 = nanos() + device0heartbeatPeriodNs;
  heartbeat_time1 = nanos() + device1heartbeatPeriodNs;
  uint64_t test_started = nanos();
  uint64_t time_elapsed = nanos() + SECS_TO_NS(5);
  while (total_sync_frames < (noResults + 1)) {
    if((time_elapsed > 0) && (nanos() >= time_elapsed)) {
      time_elapsed = 0;
      // Start the 3rd device here.
      printf("\n");
      // Device 2: Open the third device as SYNC capable at a higher priority.
      printf("Device 2: ");
      printDeviceType(device2type);
      printf(" %u on address %u with sync frequency of %uHz and heartbeat frequency of %uHz.\n", device2num, device2addr, syncFreqHz, device2heartbeatFreqHz);
      device2 = milcan_open(MILCAN_A_500K, syncFreqHz, device2addr, device2type, device2num, MILCAN_A_OPTION_SYNC_MASTER);
      if(device2 == NULL) {
        LOGE(TAG, "Unable to open device 2.");
        tidyTestsExit();
        return EXIT_FAILURE;
      }
      heartbeat_time2 = nanos() + device2heartbeatPeriodNs;
    }
    if(device0 != NULL) {
      result = milcan_recv(device0, &framein0);
      if(result <= MILCAN_ERROR_FATAL) {
        LOGE(TAG, "Device 0: MILCAN_ERROR_FATAL...");
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
            printf("\nDevice 0: Sync Master is %02x.\n", framein0.frame.can_id);
            current_sync_master = framein0.frame.can_id;
            break;
          default:
            LOGE(TAG, "Device 0: Unknown MilCAN frame type (0x%02x)", framein0.frame_type);
            break;
        }
      }
    }
    if(device1 != NULL) {
      result = milcan_recv(device1, &framein1);
      if(result <= MILCAN_ERROR_FATAL) {
        LOGE(TAG, "Device 1: MILCAN_ERROR_FATAL...");
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
            // timingRecord(&sync_timing, nanos());
            total_sync_frames++;
            break;
          case MILCAN_FRAME_TYPE_CHANGE_SYNC_MASTER:
            printf("\nDevice 1: Sync Master is %02x.\n", framein1.frame.can_id);
            break;
          default:
            LOGE(TAG, "Device 1: Unknown MilCAN frame type (0x%02x)", framein1.frame_type);
            break;
        }
      }
    }
    if(device2 != NULL) {
      result = milcan_recv(device2, &framein2);
      if(result <= MILCAN_ERROR_FATAL) {
        LOGE(TAG, "Device 2: MILCAN_ERROR_FATAL...");
        tidyTestsExit();
        return EXIT_FAILURE;
      } else if(result > 0) {
        switch(framein2.frame_type) {
          case MILCAN_FRAME_TYPE_MESSAGE:
            // print_can_frame(TAG, &(framein1.frame), 0, "Mesage In");
            break;
          case MILCAN_FRAME_TYPE_CHANGE_MODE:
            milcan_display_mode(device2);
            current_mode = framein2.frame.can_id;
            break;
          case MILCAN_FRAME_TYPE_NEW_FRAME:
            // LOGI(TAG, "Frame: %000x\r", framein1.frame.can_id);
            // printf(".");
            // fflush(stdout);
            // timingRecord(&sync_timing, nanos());
            // total_sync_frames++;
            break;
          case MILCAN_FRAME_TYPE_CHANGE_SYNC_MASTER:
            printf("\nDevice 2: Sync Master is %02x.\n", framein2.frame.can_id);
            break;
          default:
            LOGE(TAG, "Device 2: Unknown MilCAN frame type (0x%02x)", framein2.frame_type);
            break;
        }
      }
    }

    if((device0 != NULL) && (device0heartbeatFreqHz > 0) && (current_mode == MILCAN_A_MODE_OPERATIONAL) && (nanos() > heartbeat_time0)) {
      heartbeat_time0 += device0heartbeatPeriodNs;
      milcan_send(device0, &heartbeat0_frame);
      // timingRecord(&heartbeat0_timing, nanos());
      heartbeat0_frame.frame.data[0]++;
      heartbeat0_frame.frame.data[1] ^= 0xFF;
    }

    if((device1 != NULL) && (device1heartbeatFreqHz > 0) && (current_mode == MILCAN_A_MODE_OPERATIONAL) && (nanos() > heartbeat_time1)) {
      heartbeat_time1 += device1heartbeatPeriodNs;
      milcan_send(device1, &heartbeat1_frame);
      // timingRecord(&heartbeat1_timing, nanos());
      heartbeat1_frame.frame.data[0]++;
      heartbeat1_frame.frame.data[1] ^= 0xFF;
    }

    if((device2 != NULL) && (device2heartbeatFreqHz > 0) && (current_mode == MILCAN_A_MODE_OPERATIONAL) && (nanos() > heartbeat_time2)) {
      heartbeat_time2 += device2heartbeatPeriodNs;
      milcan_send(device2, &heartbeat2_frame);
      // timingRecord(&heartbeat2_timing, nanos());
      heartbeat2_frame.frame.data[0]++;
      heartbeat2_frame.frame.data[1] ^= 0xFF;
    }
  }

  milcan_close(device0);  // Device 0: Close
  device0 = NULL;
  milcan_close(device1);  // Device 1: Close
  device1 = NULL;
  milcan_close(device2);  // Device 2: Close
  device2 = NULL;
  printf("Test Finished\n");
  printf("Test took %luns\n", nanos() - test_started);
  printf("Total sync frames received: %lu\n", total_sync_frames);
  // printf("Sync Frame Timing\n");
  // timingResults(&sync_timing);
  // if(EXIT_FAILURE == timingPass(&sync_timing, 0.95)) {
  //   ret = EXIT_FAILURE;
  // }
  // if(device0heartbeatFreqHz > 0) {
  //   printf("Device 0 Heartbeat Timing\n");
  //   timingResults(&heartbeat0_timing);
  //   if(EXIT_FAILURE == timingPass(&heartbeat0_timing, 0.95)) {
  //     ret = EXIT_FAILURE;
  //   }
  // }
  // if(device1heartbeatFreqHz > 0) {
  //   printf("Device 1 Heartbeat Timing\n");
  //   timingResults(&heartbeat1_timing);
  //   if(EXIT_FAILURE == timingPass(&heartbeat1_timing, 0.95)) {
  //     ret = EXIT_FAILURE;
  //   }
  // }
  // if(device2heartbeatFreqHz > 0) {
  //   printf("Device 2 Heartbeat Timing\n");
  //   timingResults(&heartbeat2_timing);
  //   if(EXIT_FAILURE == timingPass(&heartbeat2_timing, 0.95)) {
  //     ret = EXIT_FAILURE;
  //   }
  // }
  if(current_sync_master != device2addr) {
    ret = EXIT_FAILURE;
  }
  printf("Sync Master: Expected: %u, Got: %u\n", device2addr, current_sync_master);
  if(ret == EXIT_SUCCESS) {
    printf("Test PASSED.\n");
  } else {
    printf("Test FAILED.\n");
  }
  return ret;
}

int test3(uint8_t testNo, uint16_t syncFreqHz,
  uint8_t device0type, uint8_t device0num, uint8_t device0addr, uint8_t device0heartbeatFreqHz, 
  uint8_t device1type, uint8_t device1num, uint8_t device1addr, uint8_t device1heartbeatFreqHz, 
  uint8_t device2type, uint8_t device2num, uint8_t device2addr, uint8_t device2heartbeatFreqHz) {
  
  int ret = EXIT_SUCCESS;
  int result;
  uint64_t total_sync_frames = 0;
  // struct timing sync_timing;
  uint8_t current_sync_master = 0;
  uint16_t noResults = 2000;
  // uint64_t syncPeriodNs =  (uint64_t) (1000000000L/syncFreqHz);
  // timingInit(&sync_timing, syncPeriodNs, 0.01);

  // Device 0
  struct milcan_frame framein0;
  uint64_t device0heartbeatPeriodNs = 0;
  // struct timing heartbeat0_timing;
  if(device0heartbeatFreqHz > 0)
    device0heartbeatPeriodNs =  (uint64_t) (1000000000L/device0heartbeatFreqHz);
  // timingInit(&heartbeat0_timing, device0heartbeatPeriodNs, 0.1);
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

  // Device 1
  struct milcan_frame framein1;
  uint64_t device1heartbeatPeriodNs = 0;
  // struct timing heartbeat1_timing;
  if(device1heartbeatFreqHz > 0)
    device1heartbeatPeriodNs =  (uint64_t) (1000000000L/device1heartbeatFreqHz);
  // timingInit(&heartbeat1_timing, device1heartbeatPeriodNs, 0.1);
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

    // Device 2
  struct milcan_frame framein2;
  uint64_t device2heartbeatPeriodNs = 0;
  // struct timing heartbeat2_timing;
  if(device2heartbeatFreqHz > 0)
    device2heartbeatPeriodNs =  (uint64_t) (1000000000L/device2heartbeatFreqHz);
  // timingInit(&heartbeat2_timing, device2heartbeatPeriodNs, 0.1);
  uint64_t heartbeat_time2 = nanos() + device2heartbeatPeriodNs;
  struct milcan_frame heartbeat2_frame;
  heartbeat2_frame.mortal = 0;
  heartbeat2_frame.frame.can_id = MILCAN_MAKE_ID(1, 0, 11, 12, device2addr);
  heartbeat2_frame.frame.len = 8;
  heartbeat2_frame.frame.data[0] = 0x00;
  heartbeat2_frame.frame.data[1] = 0x5A;
  heartbeat2_frame.frame.data[2] = 0x01;
  heartbeat2_frame.frame.data[3] = 0x23;
  heartbeat2_frame.frame.data[4] = 0x45;
  heartbeat2_frame.frame.data[5] = 0x67;
  heartbeat2_frame.frame.data[6] = 0x89;
  heartbeat2_frame.frame.data[7] = 0xab;

  uint16_t current_mode = MILCAN_A_MODE_POWER_OFF;
  printf("Starting Test %u\n", testNo);
  printf("Connect all three devices then disconnect the 3rd device after 5 seconds. 3rd device should have highest priority, then device 0. Device 0 is expected to become Sync Master.\n");

  // Device 0: Open the first device as non SYNC capable.
  printf("Device 0: ");
  printDeviceType(device0type);
  printf(" %u on address %u with sync frequency of %uHz and heartbeat frequency of %uHz.\n", device0num, device0addr, syncFreqHz, device0heartbeatFreqHz);
  device0 = milcan_open(MILCAN_A_500K, syncFreqHz, device0addr, device0type, device0num, MILCAN_A_OPTION_SYNC_MASTER);
  if(device0 == NULL) {
    LOGE(TAG, "Unable to open device 0.");
    tidyTestsExit();
    return EXIT_FAILURE;
  }

  // Device 1: Open the second device as SYNC capable.
  printf("Device 1: ");
  printDeviceType(device1type);
  printf(" %u on address %u with sync frequency of %uHz and heartbeat frequency of %uHz.\n", device1num, device1addr, syncFreqHz, device1heartbeatFreqHz);
  device1 = milcan_open(MILCAN_A_500K, syncFreqHz, device1addr, device1type, device1num, MILCAN_A_OPTION_SYNC_MASTER);
  if(device1 == NULL) {
    LOGE(TAG, "Unable to open device 1.");
    tidyTestsExit();
    return EXIT_FAILURE;
  }

  // Device 2: Open the third device as SYNC capable at a higher priority.
  printf("Device 2: ");
  printDeviceType(device2type);
  printf(" %u on address %u with sync frequency of %uHz and heartbeat frequency of %uHz.\n", device2num, device2addr, syncFreqHz, device2heartbeatFreqHz);
  device2 = milcan_open(MILCAN_A_500K, syncFreqHz, device2addr, device2type, device2num, MILCAN_A_OPTION_SYNC_MASTER);
  if(device2 == NULL) {
    LOGE(TAG, "Unable to open device 2.");
    tidyTestsExit();
    return EXIT_FAILURE;
  }

  heartbeat_time0 = nanos() + device0heartbeatPeriodNs;
  heartbeat_time1 = nanos() + device1heartbeatPeriodNs;
  heartbeat_time2 = nanos() + device2heartbeatPeriodNs;

  // Main Loop
  uint64_t test_started = nanos();
  uint64_t time_elapsed = nanos() + SECS_TO_NS(5);
  while (total_sync_frames < (noResults + 1)) {
    if((time_elapsed > 0) && (nanos() >= time_elapsed)) {
      time_elapsed = 0;
      // Close the 3rd device here.
      printf("\n");
      milcan_close(device2);  // Device 2: Close
      device2 = NULL;
    }
    if(device0 != NULL) {
      result = milcan_recv(device0, &framein0);
      if(result <= MILCAN_ERROR_FATAL) {
        LOGE(TAG, "Device 0: MILCAN_ERROR_FATAL...");
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
            printf("\nDevice 0: Sync Master is %02x.\n", framein0.frame.can_id);
            current_sync_master = framein0.frame.can_id;
            break;
          default:
            LOGE(TAG, "Device 0: Unknown MilCAN frame type (0x%02x)", framein0.frame_type);
            break;
        }
      }
    }
    if(device1 != NULL) {
      result = milcan_recv(device1, &framein1);
      if(result <= MILCAN_ERROR_FATAL) {
        LOGE(TAG, "Device 1: MILCAN_ERROR_FATAL...");
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
            // timingRecord(&sync_timing, nanos());
            total_sync_frames++;
            break;
          case MILCAN_FRAME_TYPE_CHANGE_SYNC_MASTER:
            printf("\nDevice 1: Sync Master is %02x.\n", framein1.frame.can_id);
            break;
          default:
            LOGE(TAG, "Device 1: Unknown MilCAN frame type (0x%02x)", framein1.frame_type);
            break;
        }
      }
    }
    if(device2 != NULL) {
      result = milcan_recv(device2, &framein2);
      if(result <= MILCAN_ERROR_FATAL) {
        LOGE(TAG, "Device 2: MILCAN_ERROR_FATAL...");
        tidyTestsExit();
        return EXIT_FAILURE;
      } else if(result > 0) {
        switch(framein2.frame_type) {
          case MILCAN_FRAME_TYPE_MESSAGE:
            // print_can_frame(TAG, &(framein1.frame), 0, "Mesage In");
            break;
          case MILCAN_FRAME_TYPE_CHANGE_MODE:
            milcan_display_mode(device2);
            current_mode = framein2.frame.can_id;
            break;
          case MILCAN_FRAME_TYPE_NEW_FRAME:
            // LOGI(TAG, "Frame: %000x\r", framein1.frame.can_id);
            // printf(".");
            // fflush(stdout);
            // timingRecord(&sync_timing, nanos());
            // total_sync_frames++;
            break;
          case MILCAN_FRAME_TYPE_CHANGE_SYNC_MASTER:
            printf("\nDevice 2: Sync Master is %02x.\n", framein2.frame.can_id);
            break;
          default:
            LOGE(TAG, "Device 2: Unknown MilCAN frame type (0x%02x)", framein2.frame_type);
            break;
        }
      }
    }

    if((device0 != NULL) && (device0heartbeatFreqHz > 0) && (current_mode == MILCAN_A_MODE_OPERATIONAL) && (nanos() > heartbeat_time0)) {
      heartbeat_time0 += device0heartbeatPeriodNs;
      milcan_send(device0, &heartbeat0_frame);
      // timingRecord(&heartbeat0_timing, nanos());
      heartbeat0_frame.frame.data[0]++;
      heartbeat0_frame.frame.data[1] ^= 0xFF;
    }

    if((device1 != NULL) && (device1heartbeatFreqHz > 0) && (current_mode == MILCAN_A_MODE_OPERATIONAL) && (nanos() > heartbeat_time1)) {
      heartbeat_time1 += device1heartbeatPeriodNs;
      milcan_send(device1, &heartbeat1_frame);
      // timingRecord(&heartbeat1_timing, nanos());
      heartbeat1_frame.frame.data[0]++;
      heartbeat1_frame.frame.data[1] ^= 0xFF;
    }

    if((device2 != NULL) && (device2heartbeatFreqHz > 0) && (current_mode == MILCAN_A_MODE_OPERATIONAL) && (nanos() > heartbeat_time2)) {
      heartbeat_time2 += device2heartbeatPeriodNs;
      milcan_send(device2, &heartbeat2_frame);
      // timingRecord(&heartbeat2_timing, nanos());
      heartbeat2_frame.frame.data[0]++;
      heartbeat2_frame.frame.data[1] ^= 0xFF;
    }
  }

  milcan_close(device0);  // Device 0: Close
  device0 = NULL;
  milcan_close(device1);  // Device 1: Close
  device1 = NULL;
  milcan_close(device2);  // Device 2: Close
  device2 = NULL;
  printf("Test Finished\n");
  printf("Test took %luns\n", nanos() - test_started);
  printf("Total sync frames received: %lu\n", total_sync_frames);
  // printf("Sync Frame Timing\n");
  // timingResults(&sync_timing);
  // if(EXIT_FAILURE == timingPass(&sync_timing, 0.95)) {
  //   ret = EXIT_FAILURE;
  // }
  // if(device0heartbeatFreqHz > 0) {
  //   printf("Device 0 Heartbeat Timing\n");
  //   timingResults(&heartbeat0_timing);
  //   if(EXIT_FAILURE == timingPass(&heartbeat0_timing, 0.95)) {
  //     ret = EXIT_FAILURE;
  //   }
  // }
  // if(device1heartbeatFreqHz > 0) {
  //   printf("Device 1 Heartbeat Timing\n");
  //   timingResults(&heartbeat1_timing);
  //   if(EXIT_FAILURE == timingPass(&heartbeat1_timing, 0.95)) {
  //     ret = EXIT_FAILURE;
  //   }
  // }
  // if(device2heartbeatFreqHz > 0) {
  //   printf("Device 2 Heartbeat Timing\n");
  //   timingResults(&heartbeat2_timing);
  //   if(EXIT_FAILURE == timingPass(&heartbeat2_timing, 0.95)) {
  //     ret = EXIT_FAILURE;
  //   }
  // }
  if(current_sync_master != device0addr) {
    ret = EXIT_FAILURE;
  }
  printf("Sync Master: Expected: %u, Got: %u\n", device0addr, current_sync_master);
  if(ret == EXIT_SUCCESS) {
    printf("Test PASSED.\n");
  } else {
    printf("Test FAILED.\n");
  }
  return ret;
}

int test4(uint8_t testNo, uint16_t syncFreqHz,
  uint8_t device0type, uint8_t device0num, uint8_t device0addr, uint8_t device0heartbeatFreqHz, 
  uint8_t device1type, uint8_t device1num, uint8_t device1addr, uint8_t device1heartbeatFreqHz, 
  uint8_t device2type, uint8_t device2num, uint8_t device2addr, uint8_t device2heartbeatFreqHz,
  uint64_t time_elapsed2, uint64_t disconnecttimens, uint64_t totaltimens) {
  
  int ret = EXIT_SUCCESS;
  int result;
  uint64_t total_sync_frames = 0;
  // struct timing sync_timing;
  // uint8_t current_sync_master = 0;
  // uint16_t noResults = 2000;
  // uint64_t syncPeriodNs =  (uint64_t) (1000000000L/syncFreqHz);
  // timingInit(&sync_timing, syncPeriodNs, 0.01);

  // Device 0
  struct milcan_frame framein0;
  uint64_t device0heartbeatPeriodNs = 0;
  // struct timing heartbeat0_timing;
  if(device0heartbeatFreqHz > 0)
    device0heartbeatPeriodNs =  (uint64_t) (1000000000L/device0heartbeatFreqHz);
  // timingInit(&heartbeat0_timing, device0heartbeatPeriodNs, 0.1);
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

  // Device 1
  struct milcan_frame framein1;
  uint64_t device1heartbeatPeriodNs = 0;
  // struct timing heartbeat1_timing;
  if(device1heartbeatFreqHz > 0)
    device1heartbeatPeriodNs =  (uint64_t) (1000000000L/device1heartbeatFreqHz);
  // timingInit(&heartbeat1_timing, device1heartbeatPeriodNs, 0.1);
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

    // Device 2
  struct milcan_frame framein2;
  uint64_t device2heartbeatPeriodNs = 0;
  // struct timing heartbeat2_timing;
  if(device2heartbeatFreqHz > 0)
    device2heartbeatPeriodNs =  (uint64_t) (1000000000L/device2heartbeatFreqHz);
  // timingInit(&heartbeat2_timing, device2heartbeatPeriodNs, 0.1);
  uint64_t heartbeat_time2 = nanos() + device2heartbeatPeriodNs;
  struct milcan_frame heartbeat2_frame;
  heartbeat2_frame.mortal = 0;
  heartbeat2_frame.frame.can_id = MILCAN_MAKE_ID(1, 0, 11, 12, device2addr);
  heartbeat2_frame.frame.len = 8;
  heartbeat2_frame.frame.data[0] = 0x00;
  heartbeat2_frame.frame.data[1] = 0x5A;
  heartbeat2_frame.frame.data[2] = 0x01;
  heartbeat2_frame.frame.data[3] = 0x23;
  heartbeat2_frame.frame.data[4] = 0x45;
  heartbeat2_frame.frame.data[5] = 0x67;
  heartbeat2_frame.frame.data[6] = 0x89;
  heartbeat2_frame.frame.data[7] = 0xab;

  uint16_t current_mode = MILCAN_A_MODE_POWER_OFF;
  printf("Starting Test %u\n", testNo);
  printf("Connect all three devices. 3rd device should have highest priority, After 5 seconds device 2 will ask to enter Config Mode. If time_elapsed2 is gretaer than 0 we will exit Config Mode after that period of time.\n");

  // Device 0: Open the first device as SYNC capable.
  printf("Device 0: ");
  printDeviceType(device0type);
  printf(" %u on address %u with sync frequency of %uHz and heartbeat frequency of %uHz.\n", device0num, device0addr, syncFreqHz, device0heartbeatFreqHz);
  device0 = milcan_open(MILCAN_A_500K, syncFreqHz, device0addr, device0type, device0num, MILCAN_A_OPTION_SYNC_MASTER);
  if(device0 == NULL) {
    LOGE(TAG, "Unable to open device 0.");
    tidyTestsExit();
    return EXIT_FAILURE;
  }

  // Device 1: Open the second device as SYNC capable.
  printf("Device 1: ");
  printDeviceType(device1type);
  printf(" %u on address %u with sync frequency of %uHz and heartbeat frequency of %uHz.\n", device1num, device1addr, syncFreqHz, device1heartbeatFreqHz);
  device1 = milcan_open(MILCAN_A_500K, syncFreqHz, device1addr, device1type, device1num, MILCAN_A_OPTION_SYNC_MASTER);
  if(device1 == NULL) {
    LOGE(TAG, "Unable to open device 1.");
    tidyTestsExit();
    return EXIT_FAILURE;
  }

  // Device 2: Open the third device as SYNC capable at a higher priority.
  printf("Device 2: ");
  printDeviceType(device2type);
  printf(" %u on address %u with sync frequency of %uHz and heartbeat frequency of %uHz.\n", device2num, device2addr, syncFreqHz, device2heartbeatFreqHz);
  device2 = milcan_open(MILCAN_A_500K, syncFreqHz, device2addr, device2type, device2num, MILCAN_A_OPTION_SYNC_MASTER);
  if(device2 == NULL) {
    LOGE(TAG, "Unable to open device 2.");
    tidyTestsExit();
    return EXIT_FAILURE;
  }

  heartbeat_time0 = nanos() + device0heartbeatPeriodNs;
  heartbeat_time1 = nanos() + device1heartbeatPeriodNs;
  heartbeat_time2 = nanos() + device2heartbeatPeriodNs;

  // Main Loop
  uint64_t test_started = nanos();
  uint64_t time_elapsed = nanos() + SECS_TO_NS(5);
  if(time_elapsed2 > 0) time_elapsed2 += nanos();
  if(totaltimens > 0) totaltimens += nanos();
  if(disconnecttimens > 0) disconnecttimens += nanos();
  uint64_t timer_sys_config = 0;
  while (totaltimens > nanos()) {
    if((time_elapsed > 0) && (nanos() >= time_elapsed)) {
      printf("\nDevice 2: Switch to SYSTEM CONFIGURATION mode.\n");
      time_elapsed = 0;
      milcan_change_to_config_mode(device2);
    }
    if((time_elapsed2 > 0) && (nanos() >= time_elapsed2)) {
      printf("\nExit SYSTEM CONFIGURATION mode.\n");
      time_elapsed2 = 0;
      milcan_exit_configuration_mode(device2);
    }
    if((disconnecttimens > 0) && (nanos() >= disconnecttimens)) {
      printf("\nClose Device 2\n");
      disconnecttimens = 0;
      milcan_close(device2);  // Device 2: Close
      if(current_mode != MILCAN_A_MODE_SYSTEM_CONFIGURATION) {
        printf("ERROR! Should still be in SYSTEM CONFIGURATION here!\n");
        ret = EXIT_FAILURE;
      }
    }
    if(device0 != NULL) {
      result = milcan_recv(device0, &framein0);
      if(result <= MILCAN_ERROR_FATAL) {
        LOGE(TAG, "Device 0: MILCAN_ERROR_FATAL...");
        tidyTestsExit();
        return EXIT_FAILURE;
      } else if(result > 0) {
        switch(framein0.frame_type) {
          case MILCAN_FRAME_TYPE_MESSAGE:
            // print_can_frame(TAG, &(framein0.frame), 0, "Mesage In");
            break;
          case MILCAN_FRAME_TYPE_CHANGE_MODE:
            // milcan_display_mode(device0);
            switch(current_mode) {
              case MILCAN_A_MODE_POWER_OFF:
                printf("Device 0: New mode: POWER OFF\n");
                break;
              case MILCAN_A_MODE_PRE_OPERATIONAL:
                printf("Device 0: New mode: PRE OPERATIONAL\n");
                break;
              case MILCAN_A_MODE_OPERATIONAL:
                printf("Device 0: New mode: OPERATIONAL\n");
                break;
              case MILCAN_A_MODE_SYSTEM_CONFIGURATION:
                printf("Device 0: New mode: SYSTEM_CONFIGURATION\n");
                timer_sys_config = nanos() + SECS_TO_NS(1);
                break;
              default:
                printf("Device 0: New mode: UNKNOWN (%u)\n", current_mode);
                break;
            }
            break;
          case MILCAN_FRAME_TYPE_NEW_FRAME:
            // printf("Frame: %000x\r", framein0.frame.can_id);
            break;
          case MILCAN_FRAME_TYPE_CHANGE_SYNC_MASTER:
            printf("\nDevice 0: Sync Master is %02x.\n", framein0.frame.can_id);
            // current_sync_master = framein0.frame.can_id;
            break;
          default:
            LOGE(TAG, "Device 0: Unknown MilCAN frame type (0x%02x)", framein0.frame_type);
            break;
        }
      }
    }
    if(device1 != NULL) {
      result = milcan_recv(device1, &framein1);
      if(result <= MILCAN_ERROR_FATAL) {
        LOGE(TAG, "Device 1: MILCAN_ERROR_FATAL...");
        tidyTestsExit();
        return EXIT_FAILURE;
      } else if(result > 0) {
        switch(framein1.frame_type) {
          case MILCAN_FRAME_TYPE_MESSAGE:
            // print_can_frame(TAG, &(framein1.frame), 0, "Mesage In");
            break;
          case MILCAN_FRAME_TYPE_CHANGE_MODE:
            // milcan_display_mode(device1);
            current_mode = framein1.frame.can_id;
            switch(current_mode) {
              case MILCAN_A_MODE_POWER_OFF:
                printf("Device 1: New mode: POWER OFF\n");
                break;
              case MILCAN_A_MODE_PRE_OPERATIONAL:
                printf("Device 1: New mode: PRE OPERATIONAL\n");
                break;
              case MILCAN_A_MODE_OPERATIONAL:
                printf("Device 1: New mode: OPERATIONAL\n");
                break;
              case MILCAN_A_MODE_SYSTEM_CONFIGURATION:
                printf("Device 1: New mode: SYSTEM_CONFIGURATION\n");
                // timer_sys_config = nanos() + SECS_TO_NS(1);
                break;
              default:
                printf("Device 1: New mode: UNKNOWN (%u)\n", current_mode);
                break;
            }
            break;
          case MILCAN_FRAME_TYPE_NEW_FRAME:
            // LOGI(TAG, "Frame: %000x\r", framein1.frame.can_id);
            printf(".");
            fflush(stdout);
            // current_frame = framein1.frame.can_id;
            // timingRecord(&sync_timing, nanos());
            total_sync_frames++;
            break;
          case MILCAN_FRAME_TYPE_CHANGE_SYNC_MASTER:
            // printf("\nDevice 1: Sync Master is %02x.\n", framein1.frame.can_id);
            break;
          default:
            LOGE(TAG, "Device 1: Unknown MilCAN frame type (0x%02x)", framein1.frame_type);
            break;
        }
      }
    }
    if(device2 != NULL) {
      result = milcan_recv(device2, &framein2);
      if(result <= MILCAN_ERROR_FATAL) {
        LOGE(TAG, "Device 2: MILCAN_ERROR_FATAL...");
        tidyTestsExit();
        return EXIT_FAILURE;
      } else if(result > 0) {
        switch(framein2.frame_type) {
          case MILCAN_FRAME_TYPE_MESSAGE:
            // print_can_frame(TAG, &(framein1.frame), 0, "Mesage In");
            break;
          case MILCAN_FRAME_TYPE_CHANGE_MODE:
            milcan_display_mode(device2);
            // current_mode = framein2.frame.can_id;
            switch(current_mode) {
              case MILCAN_A_MODE_POWER_OFF:
                printf("Device 2: New mode: POWER OFF\n");
                break;
              case MILCAN_A_MODE_PRE_OPERATIONAL:
                printf("Device 2: New mode: PRE OPERATIONAL\n");
                break;
              case MILCAN_A_MODE_OPERATIONAL:
                printf("Device 2: New mode: OPERATIONAL\n");
                break;
              case MILCAN_A_MODE_SYSTEM_CONFIGURATION:
                printf("Device 2: New mode: SYSTEM_CONFIGURATION\n");
                // timer_sys_config = nanos() + SECS_TO_NS(1);
                break;
              default:
                printf("Device 2: New mode: UNKNOWN (%u)\n", current_mode);
                break;
            }
            break;
          case MILCAN_FRAME_TYPE_NEW_FRAME:
            // LOGI(TAG, "Frame: %000x\r", framein1.frame.can_id);
            // printf(".");
            // fflush(stdout);
            // timingRecord(&sync_timing, nanos());
            // total_sync_frames++;
            break;
          case MILCAN_FRAME_TYPE_CHANGE_SYNC_MASTER:
            // printf("\nDevice 2: Sync Master is %02x.\n", framein2.frame.can_id);
            break;
          default:
            LOGE(TAG, "Device 2: Unknown MilCAN frame type (0x%02x)", framein2.frame_type);
            break;
        }
      }
    }

    if((device0 != NULL) && (device0heartbeatFreqHz > 0) && (current_mode == MILCAN_A_MODE_OPERATIONAL) && (nanos() > heartbeat_time0)) {
      heartbeat_time0 += device0heartbeatPeriodNs;
      milcan_send(device0, &heartbeat0_frame);
      // timingRecord(&heartbeat0_timing, nanos());
      heartbeat0_frame.frame.data[0]++;
      heartbeat0_frame.frame.data[1] ^= 0xFF;
    }

    if((device1 != NULL) && (device1heartbeatFreqHz > 0) && (current_mode == MILCAN_A_MODE_OPERATIONAL) && (nanos() > heartbeat_time1)) {
      heartbeat_time1 += device1heartbeatPeriodNs;
      milcan_send(device1, &heartbeat1_frame);
      // timingRecord(&heartbeat1_timing, nanos());
      heartbeat1_frame.frame.data[0]++;
      heartbeat1_frame.frame.data[1] ^= 0xFF;
    }

    if((device2 != NULL) && (device2heartbeatFreqHz > 0) && (current_mode == MILCAN_A_MODE_OPERATIONAL) && (nanos() > heartbeat_time2)) {
      heartbeat_time2 += device2heartbeatPeriodNs;
      milcan_send(device2, &heartbeat2_frame);
      // timingRecord(&heartbeat2_timing, nanos());
      heartbeat2_frame.frame.data[0]++;
      heartbeat2_frame.frame.data[1] ^= 0xFF;
    }

    if(current_mode == MILCAN_A_MODE_SYSTEM_CONFIGURATION) {
      if((timer_sys_config > 0) && (timer_sys_config <= nanos())) {
        printf(".");
        fflush(stdout);
        timer_sys_config += SECS_TO_NS(1);
      }
    }
  }

  if(current_mode != MILCAN_A_MODE_OPERATIONAL) {
    ret = EXIT_FAILURE;
  }
  printf("Mode: Expected: OPERATIONAL, Got: ");
  switch(current_mode) {
    case MILCAN_A_MODE_POWER_OFF:
      printf("POWER OFF\n");
      break;
    case MILCAN_A_MODE_PRE_OPERATIONAL:
      printf("PRE OPERATIONAL\n");
      break;
    case MILCAN_A_MODE_OPERATIONAL:
      printf("OPERATIONAL\n");
      break;
    case MILCAN_A_MODE_SYSTEM_CONFIGURATION:
      printf("SYSTEM_CONFIGURATION\n");
      break;
    default:
      printf("UNKNOWN\n");
      break;
  }

  milcan_close(device0);  // Device 0: Close
  device0 = NULL;
  milcan_close(device1);  // Device 1: Close
  device1 = NULL;
  milcan_close(device2);  // Device 2: Close
  device2 = NULL;
  printf("Test Finished\n");
  printf("Test took %luns\n", nanos() - test_started);
  printf("Total sync frames received: %lu\n", total_sync_frames);
  // printf("Sync Frame Timing\n");
  // timingResults(&sync_timing);
  // if(EXIT_FAILURE == timingPass(&sync_timing, 0.95)) {
  //   ret = EXIT_FAILURE;
  // }
  // if(device0heartbeatFreqHz > 0) {
  //   printf("Device 0 Heartbeat Timing\n");
  //   timingResults(&heartbeat0_timing);
  //   if(EXIT_FAILURE == timingPass(&heartbeat0_timing, 0.95)) {
  //     ret = EXIT_FAILURE;
  //   }
  // }
  // if(device1heartbeatFreqHz > 0) {
  //   printf("Device 1 Heartbeat Timing\n");
  //   timingResults(&heartbeat1_timing);
  //   if(EXIT_FAILURE == timingPass(&heartbeat1_timing, 0.95)) {
  //     ret = EXIT_FAILURE;
  //   }
  // }
  // if(device2heartbeatFreqHz > 0) {
  //   printf("Device 2 Heartbeat Timing\n");
  //   timingResults(&heartbeat2_timing);
  //   if(EXIT_FAILURE == timingPass(&heartbeat2_timing, 0.95)) {
  //     ret = EXIT_FAILURE;
  //   }
  // }
  if(ret == EXIT_SUCCESS) {
    printf("Test PASSED.\n");
  } else {
    printf("Test FAILED.\n");
  }
  return ret;
}

// Entry point
int main(int argc, char *argv[])
{
  int ret = EXIT_SUCCESS;

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
    case '6':
      ret = test1(testNo, MILCAN_A_500K_DEFAULT_SYNC_HZ, 
      CAN_INTERFACE_GSUSB_SO, 0, 10, 10,
      CAN_INTERFACE_GSUSB_SO, 1, 14, 10,
      CAN_INTERFACE_CANDO, 0, 23, 10);
      break;
    case '7':
      ret = test2(testNo, MILCAN_A_500K_DEFAULT_SYNC_HZ, 
      CAN_INTERFACE_GSUSB_SO, 0, 15, 10,
      CAN_INTERFACE_GSUSB_SO, 1, 16, 10,
      CAN_INTERFACE_CANDO, 0, 10, 10);
      break;
    case '8':
      ret = test3(testNo, MILCAN_A_500K_DEFAULT_SYNC_HZ, 
      CAN_INTERFACE_GSUSB_SO, 0, 20, 10,
      CAN_INTERFACE_GSUSB_SO, 1, 30, 10,
      CAN_INTERFACE_CANDO, 0, 10, 10);
      break;
    case '9':
      ret = test4(testNo, MILCAN_A_500K_DEFAULT_SYNC_HZ, 
      CAN_INTERFACE_GSUSB_SO, 0, 20, 10,
      CAN_INTERFACE_GSUSB_SO, 1, 30, 10,
      CAN_INTERFACE_CANDO, 0, 10, 10
      , 0, SECS_TO_NS(15), SECS_TO_NS(30));
      break;
    case 'A':
      ret = test4(testNo, MILCAN_A_500K_DEFAULT_SYNC_HZ, 
      CAN_INTERFACE_GSUSB_SO, 0, 20, 10,
      CAN_INTERFACE_GSUSB_SO, 1, 30, 10,
      CAN_INTERFACE_CANDO, 0, 10, 10
      , SECS_TO_NS(15), 0, SECS_TO_NS(30));
      break;
    default:
      printf("ERROR! Unknown test type.");
      ret = EXIT_FAILURE;
      break;
  }

  return ret;
}
