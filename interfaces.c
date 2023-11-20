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
#include <pthread.h>
#include "interfaces.h"
#include "txq.h"
#define LOG_LEVEL 3
#include "utils/logs.h"
#include "utils/timestamp.h"
#include "CANdoImport.h"
#include "CANdoC.h"
#include "gsusb.h"

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
struct milcan_a* interface_open(uint8_t speed, uint16_t sync_freq_hz, uint8_t sourceAddress, uint8_t can_interface_type, uint16_t moduleNumber, uint16_t options) {
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
    interface->current_sync_master = 0;
    interface->rfdfifo = -1;
    interface->wfdfifo = -1;
    interface->options = options;
    interface->mode = MILCAN_A_MODE_POWER_OFF;
    interface->rxThreadId = NULL;
    interface->eventRunFlag = FALSE;
    interface->rx.rxBufferMutex = NULL;
    interface->rx.write_offset = 0;
    interface->tx.txBufferMutex = NULL;
    for(uint8_t i = 0; i < MILCAN_ID_PRIORITY_COUNT; i++) {
      interface->tx.tx_queue[i] = NULL;
    }

    // Calculate the minimum sync slave time
    uint64_t bit_rate_in_ns;
    switch(speed) {
      case MILCAN_A_250K:
        bit_rate_in_ns = 1000000000L/250000;
        break;
      default:
      case MILCAN_A_500K:
        bit_rate_in_ns = 1000000000L/500000;
        break;
      case MILCAN_A_1M:
        bit_rate_in_ns = 1000000000L/1000000;
        break;
    }
    interface->sync_slave_time_ns = (bit_rate_in_ns * (MAX_BITS_PER_FRAME * 2)) + interface->sync_time_ns;


    LOGI(TAG, "Sync Frame Frequency requested %u", interface->sync_freq_hz);
    LOGI(TAG, "Sync Frame period calculated %lu", interface->sync_time_ns);
    LOGI(TAG, "Sync Slave timeout period %lu", interface->sync_slave_time_ns);

    interface_display_mode(interface);

    switch (can_interface_type)
    {
      case CAN_INTERFACE_CANDO:
        LOGI(TAG, "Opening CANdo (%u)...", moduleNumber);
        unsigned char status = CANdoInitialise();
        if(status) {
          CANdoConnect(moduleNumber);  // Open a connection to a CANdo device
          if(CANdoUSBStatus()->OpenFlag) {
            LOGI(TAG, "CANdo is open.");
          } else {
            LOGE(TAG, "CANdo is not open!");
            interface = interface_close(interface);
          }
          if(FALSE == CANdoStart(interface->speed)) {  // Set baud rate to 500k
            LOGE(TAG, "Unable to set CANdo baud rate!");
            interface = interface_close(interface);
          }
        } else {
          LOGE(TAG, "CANdo API library not found!");
          interface = interface_close(interface);
          break;
        }
        break;
      case CAN_INTERFACE_GSUSB_SO:
        LOGI(TAG, "Opening GSUSB (%u)...", moduleNumber);
        
        int rep = gsusbInit(&interface->ctx);
        if(rep == GSUSB_OK) {
          switch(interface->speed) {
            case MILCAN_A_250K:
              rep = gsusbOpen(&interface->ctx, moduleNumber, 6, 7, 2, 1, 12); // Sample point: 87.5%
              break;
            case MILCAN_A_500K:
              rep = gsusbOpen(&interface->ctx, moduleNumber, 6, 7, 2, 1, 6); // Sample point: 87.5%
              break;
            case MILCAN_A_1M:
              rep = gsusbOpen(&interface->ctx, moduleNumber, 6, 7, 2, 1, 3); // Sample point: 87.5%
              break;        
          }
          if(rep != GSUSB_OK) {
            gsusbExit(&interface->ctx);
          }
        }
        if(rep == GSUSB_OK) {
          LOGI(TAG, "Device opened!");
        } else {
          LOGE(TAG, "Error opening!");
          interface = interface_close(interface);
        }
        break;
      default:
        LOGE(TAG, "CAN interface type is unrecognised or unsupported.");
        interface = interface_close(interface);
        break;
    }
  }

  if(interface != NULL) {
    pthread_mutex_init(&(interface->rx.rxBufferMutex), NULL);  // Init. mutex
    pthread_mutex_init(&(interface->tx.txBufferMutex), NULL);  // Init. mutex
  }

  return interface;
}

struct milcan_a* interface_close(struct milcan_a* interface) {
  if(interface != NULL) {
    interface->eventRunFlag = FALSE;
    switch(interface->can_interface_type) {
      case CAN_INTERFACE_CANDO:
        CANdoCloseAndFinalise();
        break;
      case CAN_INTERFACE_GSUSB_SO:
        gsusbExit(&interface->ctx);
        break;
    }
    LOGI(TAG, "Freeing memory...");
    free(interface);
    interface = NULL;
    LOGI(TAG, "Done.");
  }
  return interface;
}

int interface_send(struct milcan_a* interface, struct milcan_frame * frame) {
  // ssize_t ret = -1;
  int rep = FALSE;
  uint32_t id;
  uint8_t extended = 0;

  // print_milcan_frame(TAG, frame, "PIPE OUT");
  switch(interface->can_interface_type) {
    case CAN_INTERFACE_CANDO:
      id = frame->frame.can_id;
      if(frame->frame.can_id & CAN_EFF_FLAG) {
        id &= CAN_EFF_MASK;
        extended = 1;
      } else {
        id &= CAN_SFF_MASK;
      }
      rep= CANdoTx(extended, id, frame->frame.len, frame->frame.data);
      break;

    case CAN_INTERFACE_GSUSB_SO:
      if(GSUSB_OK == gsusbWrite(&interface->ctx, &(frame->frame))) {
        rep = TRUE;
      }
      break;

    default:
      LOGE(TAG, "CAN interface type is unrecognised or unsupported.");
      break;
  }
  return rep;
}

void interface_add_to_rx_buffer(struct milcan_a* interface, struct milcan_frame *frame) {
  pthread_mutex_lock(&(interface->rx.rxBufferMutex));
  if(interface->rx.write_offset < RX_BUFFER_SIZE) {
    // LOGI(TAG, "1. Id = %08x, Len = %u", frame->frame.can_id, frame->frame.len);
    memcpy(&(interface->rx.buffer[interface->rx.write_offset]), frame, sizeof(struct milcan_frame));
    // LOGI(TAG, "2. Id = %08x, Len = %u", interface->rx.buffer[interface->rx.write_offset].frame.can_id, interface->rx.buffer[interface->rx.write_offset].frame.len);
    interface->rx.write_offset++;
    // LOGI(TAG, "Rx buffer contains %u messages.", interface->rx.write_offset);
  } else {
    LOGE(TAG, "Rx Buffer full!");
  }
  pthread_mutex_unlock(&(interface->rx.rxBufferMutex));
}

void interface_handle_rx_message(struct milcan_a* interface, struct milcan_frame *frame) {
  if((frame->frame.can_id & MILCAN_ID_SOURCE_MASK) != interface->sourceAddress) {
    print_milcan_frame(TAG, frame, "PIPE IN");
  }
  if((frame->frame.can_id & ~MILCAN_ID_SOURCE_MASK) == MILCAN_MAKE_ID(0, 0, MILCAN_ID_PRIMARY_SYSTEM_MANAGEMENT, MILCAN_ID_SECONDARY_SYSTEM_MANAGEMENT_SYNC_FRAME, 0)) {
    // We've recieved a sync frame!
    if((interface->current_sync_master == 0) || (interface->mode == MILCAN_A_MODE_PRE_OPERATIONAL)) {
      interface->mode = MILCAN_A_MODE_OPERATIONAL;
      interface->current_sync_master = (uint8_t) (frame->frame.can_id & MILCAN_ID_SOURCE_MASK);
      interface_display_mode(interface);
      if(interface->current_sync_master != interface->sourceAddress) {    // It's not from us.
        interface->syncTimer = nanos() + interface->sync_time_ns;  // Next period from now.
        interface->sync = frame->frame.data[0] + ((uint16_t) frame->frame.data[1] * 256);
      } else {
        LOGI(TAG, "We are now Sync Master!");
      }
    } else if((frame->frame.can_id & MILCAN_ID_SOURCE_MASK) < interface->current_sync_master) {
      // This device has a higher priority than us so it should be the Sync Master instead which ever device currently has it.
      if(((uint8_t)(frame->frame.can_id & MILCAN_ID_SOURCE_MASK)) != interface->sourceAddress) {    // It's not from us.
        if(interface->current_sync_master == interface->sourceAddress) {
          LOGI(TAG, "We are no longer Sync Master.");
        }
        interface->syncTimer = nanos() + interface->sync_time_ns;  // Next period from now.
        interface->sync = frame->frame.data[0] + ((uint16_t) frame->frame.data[1] * 256);
      } else {
        LOGI(TAG, "We are now Sync Master!");
      }
      interface->current_sync_master = (uint8_t) (frame->frame.can_id & MILCAN_ID_SOURCE_MASK);
    }
  } else {
    // Add the message to the Rx buffer.
    if((frame->frame.can_id & MILCAN_ID_SOURCE_MASK) != interface->sourceAddress) {
      interface_add_to_rx_buffer(interface, frame);
    }
  }
}

// Returns the number of messages left in the buffer
uint16_t interface_rx_buffer_size(struct milcan_a* interface) {
  uint16_t ret = 0;
  pthread_mutex_lock(&(interface->rx.rxBufferMutex));
  ret = interface->rx.write_offset;
  pthread_mutex_unlock(&(interface->rx.rxBufferMutex));

  return ret;
}

// Returns 1 if we've read a message from the buffer, else 0.
int interface_recv(struct milcan_a* interface, struct milcan_frame *frame) {
  int ret = 0;
  pthread_mutex_lock(&(interface->rx.rxBufferMutex));
  if(interface->rx.write_offset > 0) {
    // LOGI(TAG, "3. Id = %08x, Len = %u", interface->rx.buffer[0].frame.can_id, interface->rx.buffer[0].frame.len);
    memcpy(frame, &(interface->rx.buffer[0]), sizeof(struct milcan_frame));
    // LOGI(TAG, "4. Id = %08x, Len = %u", frame->frame.can_id, frame->frame.len);
    memmove(&(interface->rx.buffer[0]), &(interface->rx.buffer[1]), sizeof(struct milcan_frame) * (RX_BUFFER_SIZE  -1));
    if(interface->rx.write_offset > 0) {  // We shouldn't have to to check but memory errors suck so I'll check.
      interface->rx.write_offset--;
    }
    // LOGI(TAG, "Rx buffer contains %u messages.", interface->rx.write_offset);
    ret = 1;
  }
  pthread_mutex_unlock(&(interface->rx.rxBufferMutex));

  return ret;
}

int interface_handle_rx(struct milcan_a* interface) {
  int ret;
  // int i = 0;
  struct milcan_frame frame;
  frame.mortal = 0;
  // int toRead = 0;

  switch(interface->can_interface_type) {
    case CAN_INTERFACE_CANDO:
      CANdoRx();
      ret = CANdoReadRxQueue(&(frame.frame));
      while(ret) {
        switch(ret) {
            case MILCAN_OK:
                interface_handle_rx_message(interface, &frame);
                break;
            default:
            case MILCAN_ERROR:
                break;
            case MILCAN_ERROR_CONN_CLOSED:
                LOGE(TAG, "CAN connection closed.");
                return MILCAN_ERROR_FATAL;
        }
        ret = CANdoReadRxQueue(&(frame.frame));
      }
      break;
    case CAN_INTERFACE_GSUSB_SO:
      while(GSUSB_OK == gsusbRead(&interface->ctx, &(frame.frame))) {
        interface_handle_rx_message(interface, &frame);
      }
      break;
  }

  return MILCAN_OK;
}

void interface_display_mode(struct milcan_a* interface) {
    switch(interface->mode) {
    case MILCAN_A_MODE_POWER_OFF:
        LOGI(TAG, "Mode: Power Off");
        break;
    case MILCAN_A_MODE_PRE_OPERATIONAL:
        LOGI(TAG, "Mode: Pre Operational");
        break;
    case MILCAN_A_MODE_OPERATIONAL:
        LOGI(TAG, "Mode: Operational");
        break;
    case MILCAN_A_MODE_SYSTEM_CONFIGURATION:
        LOGI(TAG, "Mode: System Configuration");
        break;
    default:
        LOGE(TAG, "Mode: Unrecognised (%u)", interface->mode);
        break;
    }
}

int interface_tx_add_to_q(struct milcan_a* interface, struct milcan_frame *frame) {
  // Adjust txq functions to accept the interface. DONE
  // Create a mutex for the Tx to control access. DONE
  // CFG messages, etc should be controlled by check sync.
  // Will need a state to keep track of sending a CFG mesage and how far through we are.

  struct milcan_frame *frame2 = calloc(sizeof(struct milcan_frame), 1);
  if(frame2 == NULL) {
    return ENOMEM;
  }
  memcpy(frame2, frame, sizeof(struct milcan_frame));
  pthread_mutex_lock(&(interface->tx.txBufferMutex));
  txQAdd(interface, frame2);
  pthread_mutex_unlock(&(interface->tx.txBufferMutex));
  return 0;
}

struct milcan_frame * interface_tx_read_q(struct milcan_a* interface) {
  struct milcan_frame *frame = NULL;

  pthread_mutex_lock(&(interface->tx.txBufferMutex));
  for(int i = 0; (i < MILCAN_ID_PRIORITY_COUNT) && (frame == NULL); i++) {
    frame = txQRead(interface, i);
  }
  pthread_mutex_unlock(&(interface->tx.txBufferMutex));

  return frame;
}