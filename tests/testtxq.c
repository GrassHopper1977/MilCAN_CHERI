// testtxq.c
#include <stdio.h>      /* Standard input/output definitions */
#include <string.h>     /* String function definitions */
#include <unistd.h>     /* UNIX standard function definitions */
//#include <fcntl.h>      /* File control definitions */
#include <errno.h>      /* Error number definitions */
//#include <termios.h>    /* POSIX terminal control definitions */
//#include <sys/socket.h> // Sockets
//#include <netinet/in.h>
//#include <sys/un.h>     // ?
//#include <sys/event.h>  // Events
#include <assert.h>     // The assert function
//#include <unistd.h>     // ?
//#include <stdint.h>
#include <stdlib.h>     // Needed to calloc() and probably other things too.
//#include <sys/stat.h>
//#include <sys/types.h>
//#include <netdb.h>
//#include <signal.h>
//#include <stdarg.h>     // Needed for variable argument handling __VA_ARGS__, etc.
//#include <inttypes.h>

#define LOG_LEVEL 3
#include "../log.h"
#include "../milcan.h"
#include "../txq.h"
#include "../timestamp.h"

#define TAG "testtxq"

// Add items to the Tx Queue (txq) and then read them out and check that they are in the expected order.

#define NUM_TEST_FRAMES 41
struct milcan_frame test1[NUM_TEST_FRAMES] = {
    {   // 0
        .frame.can_id = MILCAN_MAKE_ID(1, 0, 0x31, 0x02, 0x10),
        .frame.data = {0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
        .frame.len = 8,
        .frame.__pad = 0,
        .frame.__res0 = 0,
        .frame.__res1 = 0,
        .mortal = 0
    },
    {   // 1
        .frame.can_id = MILCAN_MAKE_ID(3, 0, 0x35, 0x02, 0x03),
        .frame.data = {0x02, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
        .frame.len = 7,
        .frame.__pad = 0,
        .frame.__res0 = 0,
        .frame.__res1 = 0,
        .mortal = 0
    },
    {   // 2
        .frame.can_id = MILCAN_MAKE_ID(6, 0, 0x31, 0x10, 0x03),
        .frame.data = {0x03, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
        .frame.len = 3,
        .frame.__pad = 0,
        .frame.__res0 = 0,
        .frame.__res1 = 0,
        .mortal = 0
    },
    {   // 3
        .frame.can_id = MILCAN_MAKE_ID(2, 0, 0x54, 0x01, 0x10),
        .frame.data = {0x04, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
        .frame.len = 2,
        .frame.__pad = 0,
        .frame.__res0 = 0,
        .frame.__res1 = 0,
        .mortal = 0
    },
    {   // 4
        .frame.can_id = MILCAN_MAKE_ID(1, 0, 0x66, 0x01, 0xF0),
        .frame.data = {0x05, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
        .frame.len = 8,
        .frame.__pad = 0,
        .frame.__res0 = 0,
        .frame.__res1 = 0,
        .mortal = 0
    },
    {   // 5
        .frame.can_id = MILCAN_MAKE_ID(2, 0, 0x66, 0x01, 0xF1),
        .frame.data = {0x06, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
        .frame.len = 3,
        .frame.__pad = 0,
        .frame.__res0 = 0,
        .frame.__res1 = 0,
        .mortal = 0
    },
    {   // 6
        .frame.can_id = MILCAN_MAKE_ID(2, 0, 0x63, 0x21, 0xF1),
        .frame.data = {0x07, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
        .frame.len = 5,
        .frame.__pad = 0,
        .frame.__res0 = 0,
        .frame.__res1 = 0,
        .mortal = 0
    },
    {   // 7
        .frame.can_id = MILCAN_MAKE_ID(2, 0, 0x66, 0x64, 0xF0),
        .frame.data = {0x08, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
        .frame.len = 3,
        .frame.__pad = 0,
        .frame.__res0 = 0,
        .frame.__res1 = 0,
        .mortal = 0
    },
    {   // 8
        .frame.can_id = MILCAN_MAKE_ID(5, 0, 0x92, 0x01, 0x12),
        .frame.data = {0x09, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
        .frame.len = 7,
        .frame.__pad = 0,
        .frame.__res0 = 0,
        .frame.__res1 = 0,
        .mortal = 0
    },
    {   // 9
        .frame.can_id = MILCAN_MAKE_ID(4, 0, 0x30, 0x01, 0x7E),
        .frame.data = {0x0A, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
        .frame.len = 1,
        .frame.__pad = 0,
        .frame.__res0 = 0,
        .frame.__res1 = 0,
        .mortal = 0
    },
    {   // 10
        .frame.can_id = MILCAN_MAKE_ID(7, 0, 0x30, 0x01, 0x7F),
        .frame.data = {0x0B, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
        .frame.len = 3,
        .frame.__pad = 0,
        .frame.__res0 = 0,
        .frame.__res1 = 0,
        .mortal = 0
    },
    MILCAN_MAKE_SYNC(0x13, 0),  // 11
    MILCAN_MAKE_ENTER_CONFIG_0(0x13),   // 12
    MILCAN_MAKE_ENTER_CONFIG_1(0x13),   // 13
    MILCAN_MAKE_ENTER_CONFIG_2(0x13),   // 14
    MILCAN_MAKE_EXIT_CONFIG_0(0x13),   // 15
    MILCAN_MAKE_EXIT_CONFIG_1(0x13),   // 16
    MILCAN_MAKE_EXIT_CONFIG_2(0x13),   // 17
    MILCAN_MAKE_SYNC(0x13, 1),   // 18
    MILCAN_MAKE_SYNC(0x13, 255),   // 19
    MILCAN_MAKE_SYNC(0x13, 256),   // 20
    MILCAN_MAKE_SYNC(0x13, 257),   // 21
    MILCAN_MAKE_SYNC(0x13, 1023),   // 22
    MILCAN_MAKE_SYNC(0x13, 1024),   // 23 Should wrap to 0000
    MILCAN_MAKE_SYNC(0x13, 1025),   // 24 Should wrap to 0001
    MILCAN_MAKE_FRAME(MILCAN_MAKE_ID(7, 0, 0x30, 0x02, 0x7F), 500000L, 3, 0x0B, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00),    // 25 - Frame with a 0.5 time to live
    MILCAN_MAKE_FRAME(MILCAN_MAKE_ID(7, 0, 0x30, 0x0A, 0x7F), 0, 3, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00),    // 26
    MILCAN_MAKE_FRAME(MILCAN_MAKE_ID(7, 0, 0x30, 0x0A, 0x7F), 0, 3, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00),    // 27
    MILCAN_MAKE_FRAME(MILCAN_MAKE_ID(7, 0, 0x30, 0x09, 0x7F), 0, 3, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00),    // 28
    MILCAN_MAKE_FRAME(MILCAN_MAKE_ID(7, 0, 0x30, 0x09, 0x7F), 0, 3, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00),    // 29
    MILCAN_MAKE_FRAME(MILCAN_MAKE_ID(7, 0, 0x30, 0x0A, 0x7F), 0, 3, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00),    // 30
    MILCAN_MAKE_FRAME(MILCAN_MAKE_ID(7, 0, 0x30, 0x09, 0x7F), 0, 3, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00),    // 31
    MILCAN_MAKE_FRAME(MILCAN_MAKE_ID(7, 0, 0x30, 0x09, 0x7F), 0, 3, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00),    // 32
    MILCAN_MAKE_FRAME(MILCAN_MAKE_ID(7, 0, 0x30, 0x0A, 0x7F), 0, 3, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00),    // 33
    MILCAN_MAKE_FRAME(MILCAN_MAKE_ID(7, 0, 0x30, 0x0A, 0x7F), 0, 3, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00),    // 34
    MILCAN_MAKE_FRAME(MILCAN_MAKE_ID(7, 0, 0x30, 0x09, 0x7F), 0, 3, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00),    // 35
    MILCAN_MAKE_FRAME(MILCAN_MAKE_ID(7, 0, 0x30, 0x04, 0x7F), 1750000L, 3, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00),    // 36 - Frame with a 1.75 time to live
    MILCAN_MAKE_FRAME(MILCAN_MAKE_ID(7, 0, 0x30, 0x05, 0x7F), 5500000L, 3, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00),   // 37 - Frame with a 5.5 time to live
    MILCAN_MAKE_FRAME(MILCAN_MAKE_ID(7, 0, 0x30, 0x02, 0x7F), 1250000L, 3, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00),    // 38 - Frame with a 1.25 time to live
    MILCAN_MAKE_FRAME(MILCAN_MAKE_ID(7, 0, 0x30, 0x01, 0x7F), 1125000L, 3, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00),    // 39 - Frame with a 1.125 time to live
    MILCAN_MAKE_FRAME(MILCAN_MAKE_ID(7, 0, 0x30, 0x03, 0x7F), 1500000L, 3, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00),    // 40 - Frame with a 1.5 time to live
};

void print_milcan_frame(struct milcan_frame *frame, uint8_t err, const char *format, ...) {
  FILE * fd = stdout;

  if(err || (frame->frame.can_id & CAN_ERR_FLAG)) {
    fd = stderr;
    fprintf(fd, "ID: ");
  } else {
    fprintf(fd, "ID: ");
  }

  if((frame->frame.can_id & CAN_EFF_FLAG) || (frame->frame.can_id & CAN_ERR_FLAG)) {
    fprintf(fd, "%08x", frame->frame.can_id & CAN_EFF_MASK);
  } else {
    fprintf(fd, "     %03x", frame->frame.can_id & CAN_SFF_MASK);
  }
  if(frame->frame.can_id & MILCAN_ID_MILCAN_TYPE) {
    fprintf(fd, " (MilCAN: Priority: %i, Request: %i, Primary: %02x, Secondary: %02x, Source: %02x)", 
        (((frame->frame.can_id & MILCAN_ID_PRIORITY_MASK) >> 26) & 0x07),
        (((frame->frame.can_id & MILCAN_ID_MILCAN_REQUEST) >> 24) & 0x01),
        (((frame->frame.can_id & MILCAN_ID_PRIMARY_MASK) >> 16) & 0x0FF),
        (((frame->frame.can_id & MILCAN_ID_SECONDARY_MASK) >> 8) & 0x0FF),
        (frame->frame.can_id & MILCAN_ID_SOURCE_MASK)
    );
  } else {
    fprintf(fd, " (SAE J1939: Priority: %i, Data Page: %i, PDU Format: %02x, PDU Field: %02x, Source: %02x)", 
        (((frame->frame.can_id & MILCAN_ID_PRIORITY_MASK) >> 26) & 0x07),
        (((frame->frame.can_id & MILCAN_ID_MILCAN_REQUEST) >> 24) & 0x01),
        (((frame->frame.can_id & MILCAN_ID_PRIMARY_MASK) >> 16) & 0x0FF),
        (((frame->frame.can_id & MILCAN_ID_SECONDARY_MASK) >> 8) & 0x0FF),
        (frame->frame.can_id & MILCAN_ID_SOURCE_MASK)
    );
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

  fprintf(fd, "mortal: ");
  if(frame->mortal == 0) {
      fprintf(fd, "FALSE ");
  } else {
      fprintf(fd, "TRUE ttl: %lu ", frame->mortal - nanos());
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

int loadUnsortedArray(struct milcan_frame* array, uint16_t size) {
    struct milcan_frame* frame;
    int result = 0;
    
    LOGI("testtxq", "Adding unsorted MilCAN frames to the queue.");
    for(int i = 0; i < size; i++) {
        frame = calloc(1, sizeof(struct milcan_frame));
        assert(frame != NULL);
        memcpy(frame, &array[i], sizeof(struct milcan_frame));
        result = txQAdd(frame);
        assert(result == 0);
        printf("%3i. ", i + 1);
        print_milcan_frame(frame, 0, "added.");

    }

    return result;
}

/// @brief Reads the messages out of the Q for each priority level. This code double checks that the CAN ID increases with each read and double checks that the priority is correct.
/// @return 0 is succesful, anything else is an error
int readSortedFramesFromQueues(uint32_t number) {
    int result = 0;
    struct milcan_frame* frame;
    int i;
    uint32_t count = 0;
    canid_t id_test = 0;    // We will check that the ID increases with each message that we read.

    LOGI(TAG, "Reading sorted MilCAN frames from the queues.");
    for(uint8_t priority = 0; priority < MILCAN_ID_PRIORITY_COUNT; priority++) {
        LOGI(TAG, "Priority %i:", priority);
        i = 0;
        do {
            frame = txQRead(priority);
            if(frame != NULL) {
                count++;
                printf("%3i. ", i + 1);
                print_milcan_frame(frame, 0, "read.");
                i++;
                if(frame->frame.can_id < id_test) {
                    result = EBADMSG;
                    LOGE(TAG, "CAN ID out of sequence! Privious: %08x, Current: %08x", id_test, frame->frame.can_id);
                }
                if(((frame->frame.can_id & MILCAN_ID_PRIORITY_MASK) >> 26) != priority) {
                    result = EBADMSG;
                    LOGE(TAG, "CAN ID priority mismatch!: Looking for: %u, Read: %u", priority, (frame->frame.can_id & MILCAN_ID_PRIORITY_MASK) >> 26);
                }
                free(frame);
            }
        } while(frame != NULL);
        printf("\n");
    }

    if(count != number) {
        result = EBADMSG;
        LOGE(TAG, "Incorrect number of message!: Expecting: %u, Read: %u", number, count);
    } else {
        LOGI(TAG, "Correct message number read:  Expecting: %u, Read: %u", number, count);
    }

    return result;
}

int main(int argc, char *argv[]) {
    int result = 0;
    printf("Test 1: Load unsorted data and verify that it is sorted correctly:\n");
    uint64_t timestamp = nanos();
    loadUnsortedArray(test1, NUM_TEST_FRAMES);
    printf("Time to load: %lu ns\n\n", nanos() - timestamp);

    timestamp = nanos();
    result = readSortedFramesFromQueues(NUM_TEST_FRAMES);
    printf("Time to read: %lu ns\n\n", nanos() - timestamp);
    if(result != 0) {
        LOGE(TAG, "Fatal error encountered. Execution stopped.");
        exit(result);
    } else {
        printf("Passed\n");
    }

    printf("\n\nTest 2: Load unsorted data, wait for an timed data to expire and then and verify that it is sorted correctly:\n");
    timestamp = nanos();
    loadUnsortedArray(test1, NUM_TEST_FRAMES);
    printf("Time to load: %lu ns\n\n", nanos() - timestamp);
    timestamp = nanos();
    printf("\nWait for 3 seconds...");
    uint64_t temp = nanos() + 3000000L;
    while(temp > nanos()) {}
    printf(" Done.\n");
    printf("Time waited: %lu ns\n\n", nanos() - timestamp);
    timestamp = nanos();
    result = readSortedFramesFromQueues(NUM_TEST_FRAMES - 5);
    printf("Time to read: %lu ns\n\n", nanos() - timestamp);
    if(result != 0) {
        LOGE(TAG, "Fatal error encountered. Execution stopped.");
        exit(result);
    }
}
