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
#include <pthread.h>


#define LOG_LEVEL 3
#include "utils/logs.h"
#include "utils/timestamp.h"
#include "utils/priorities.h"
#include "milcan.h"
#include "interfaces.h"
#include "txq.h"

// #define BUFSIZE 1024
// #define SLEEP_TIME  100  // sleep time in us e.g. 1000 = 1ms

#define TAG "MilCAN"

// void print_can_frame(const char* tag, struct can_frame *frame, uint8_t err, const char *format, ...) {
//   FILE * fd = stdout;
//
//   if(err || (frame->can_id & CAN_ERR_FLAG)) {
//     fd = stderr;
//   }
//   fprintf(fd, "%lu:  INFO: %s: %s() line %i: %s: ID: ", nanos(), __FILE__, __FUNCTION__, __LINE__, tag);
//
//   if((frame->can_id & CAN_EFF_FLAG) || (frame->can_id & CAN_ERR_FLAG)) {
//     fprintf(fd, "%08x", frame->can_id & CAN_EFF_MASK);
//   } else {
//     fprintf(fd, "     %03x", frame->can_id & CAN_SFF_MASK);
//   }
//   fprintf(fd, ", len: %2u", frame->len);
//   fprintf(fd, ", Data: ");
//   for(int n = 0; n < CAN_MAX_DLC; n++) {
//     if(n < frame->len) {
//       fprintf(fd, "%02x, ", frame->data[n]);
//     } else {
//       fprintf(fd, "    ");
//     }
//   }
//
//   va_list args;
//   va_start(args, format);
//
//   vfprintf(fd, format, args);
//   va_end(args);
//
//   if(frame->can_id & CAN_ERR_FLAG) {
//     fprintf(fd, ", ERROR FRAME");
//   }
//
//   fprintf(fd, "\n");
// }

int milcan_add_to_rx_buffer(struct milcan_a* interface, struct milcan_frame *frame) {
  int ret = MILCAN_OK;
  pthread_mutex_lock(&(interface->rx.rxBufferMutex));
  if(interface->rx.write_offset < RX_BUFFER_SIZE) {
    // LOGI(TAG, "1. Id = %08x, Len = %u", frame->frame.can_id, frame->frame.len);
    memcpy(&(interface->rx.buffer[interface->rx.write_offset]), frame, sizeof(struct milcan_frame));
    // LOGI(TAG, "2. Id = %08x, Len = %u", interface->rx.buffer[interface->rx.write_offset].frame.can_id, interface->rx.buffer[interface->rx.write_offset].frame.len);
    interface->rx.write_offset++;
    // LOGI(TAG, "Rx buffer contains %u messages.", interface->rx.write_offset);
  } else {
    LOGE(TAG, "Rx Buffer full!");
    ret = MILCAN_ERROR_MEM;
  }
  pthread_mutex_unlock(&(interface->rx.rxBufferMutex));
  return ret;
}

int notify_new_sync(struct milcan_a* interface) {
  // Notify application that teh frame has changed.
  uint16_t sync = interface->sync;
  struct milcan_frame mode_sync = MILCAN_MAKE_NEW_FRAME(sync);
  return milcan_add_to_rx_buffer(interface, &mode_sync);
}

int notify_new_sync_master(struct milcan_a* interface) {
  // Notify application that teh frame has changed.
  uint16_t id = interface->current_sync_master;
  struct milcan_frame mode_sync_master = MILCAN_MAKE_NEW_SYNC_MASTER(id);
  return milcan_add_to_rx_buffer(interface, &mode_sync_master);
}

int change_mode(struct milcan_a* interface, int mode) {
  interface->mode = mode;
  // Notify application that we've changed mode.
  struct milcan_frame mode_frame = MILCAN_MAKE_CHANGE_MODE(mode);
  switch(mode) {
    default:
    case MILCAN_A_MODE_POWER_OFF:
      // Don't do anything. Nothing at all. Nada. Zip. Zilch.
      break;
    case MILCAN_A_MODE_PRE_OPERATIONAL:
      // Start the Sync Slave Timeout Period timer.
      interface->current_sync_master = 0;
      interface->mode_exit_timer = nanos() + interface->sync_slave_time_ns;
      notify_new_sync_master(interface);
      break;
    case MILCAN_A_MODE_OPERATIONAL:
      // Start the 8 PDUs timer that is reset whenever a SYNC is received. If it times out then enter MILCAN_A_MODE_PRE_OPERATIONAL.
      interface->mode_exit_timer = nanos() + (8 * interface->sync_time_ns); 
      break;
    case MILCAN_A_MODE_SYSTEM_CONFIGURATION:
      // Start the 8 second timer that is reset by the enter config mode sequence. If it times out then enter MILCAN_A_MODE_PRE_OPERATIONAL.
      break;
  }
  return milcan_add_to_rx_buffer(interface, &mode_frame);
}

int send_sync_frame(struct milcan_a* interface) {
  interface->syncTimer = nanos() + interface->sync_time_ns; // Next period from when we should've been.
  // MilCAN Sync Frame
  interface->sync++;
  interface->sync &= 0x000003FF;
  struct milcan_frame frame = MILCAN_MAKE_SYNC(interface->sourceAddress, interface->sync);
  return interface_send(interface, &frame);  // Sync frames bypass the Tx queue
}

uint64_t set_sync_slave_time_ns(struct milcan_a* interface, uint64_t new_time) {
  uint64_t min_time = 0;
  uint64_t one_bit = 0;
  switch(interface->speed) {
    case MILCAN_A_250K:
      one_bit = 4000; // One bit is 4000ns long.
      break;
    case MILCAN_A_500K:
      one_bit = 2000; // One bit is 2000ns long.
      break;
    case MILCAN_A_1M:
      one_bit = 1000; // One bit is 1000ns long.
      break;
  }
  // The formula is one PTU + (2 * time to transmit to messages of maximum length including bit stuffing)
  // The maximum length including bit stuffing is 140 bits with an extra 3 bits of interframe spacing.
  min_time = interface->sync_time_ns + (2 * (140 + 3) * one_bit);
  if(new_time < min_time) {
    new_time = min_time;
  }
  interface->sync_slave_time_ns = new_time;
  LOGI(TAG, "Sync Slave timeout period %lu", interface->sync_slave_time_ns);
  return interface->sync_slave_time_ns;
}

// React to any MilCAN mesages the we receive, send any messages that we need to send and react to Mode changes.
void doStateMachine(struct milcan_a* interface, int rxframeValid, struct milcan_frame* rxframe) {
  uint64_t now = nanos();
  struct milcan_frame * qframe = NULL;
  uint8_t rxframeIsSelf = FALSE;
  uint8_t rxframeIsControl = FALSE;

  // Calculate some useful state information here.
  if(rxframeValid == MILCAN_OK) {
    // Is it from us?
    if((rxframe->frame.can_id & MILCAN_ID_SOURCE_MASK) == interface->sourceAddress) {
      rxframeIsSelf = TRUE;
    }
    // Is it a control message (i.e. Sync, Enter Config or Exit Config)
    if(((rxframe->frame.can_id & MILCAN_ID_PRIMARY_MASK) == (MILCAN_ID_PRIMARY_SYSTEM_MANAGEMENT << 16))
      && ((rxframe->frame.can_id & MILCAN_ID_SECONDARY_MASK) >= (MILCAN_ID_SECONDARY_SYSTEM_MANAGEMENT_SYNC_FRAME << 8))
      && ((rxframe->frame.can_id & MILCAN_ID_SECONDARY_MASK) <= (MILCAN_ID_SECONDARY_SYSTEM_MANAGEMENT_EXIT_CONFIG << 8))) {
      rxframeIsControl = TRUE;
      // if(rxframeIsSelf == FALSE) {
      //   LOGI(TAG, "ID: %08x", rxframe->frame.can_id & (MILCAN_ID_PRIMARY_MASK | MILCAN_ID_SECONDARY_MASK));
      // }
    }
  }

  // Everything depends upon our current mode.
  switch(interface->mode) {
    default:
    case MILCAN_A_MODE_POWER_OFF:             // System is off
      // We don't Tx or Rx. We just change to Pre-Operational
      interface->current_sync_master = 0;
      interface->syncTimer = now;
      change_mode(interface, MILCAN_A_MODE_PRE_OPERATIONAL);
      break;
    case MILCAN_A_MODE_PRE_OPERATIONAL:       // The only messages that we can send are Sync or Enter Config
      // The only messages that are valid here are Sync or or Enter/Exit Config messages. Everything else is ignored and not added to the RxQ.
      if((rxframeValid == MILCAN_OK) && ((rxframe->frame.can_id & MILCAN_ID_PRIMARY_MASK) == MILCAN_ID_PRIMARY_SYSTEM_MANAGEMENT) && ((rxframe->frame.can_id & MILCAN_ID_SECONDARY_MASK) == (MILCAN_ID_SECONDARY_SYSTEM_MANAGEMENT_SYNC_FRAME << 8))) {
        // It's a Sync Frame!
        // Is it higher priority or the same as the last one?
        if((interface->current_sync_master == 0) || ((rxframe->frame.can_id & MILCAN_ID_SOURCE_MASK) <= interface->current_sync_master)) {
          if(rxframeIsSelf == FALSE) {
            // Save the Sync Value.
            int changes = FALSE;
            if(interface->current_sync_master != (rxframe->frame.can_id & MILCAN_ID_SOURCE_MASK)) {
              changes = TRUE;
            }
            interface->current_sync_master = (uint8_t) (rxframe->frame.can_id & MILCAN_ID_SOURCE_MASK);
            interface->syncTimer = now + interface->sync_time_ns;  // Next period from now.
            interface->sync = rxframe->frame.data[0] + ((uint16_t) rxframe->frame.data[1] * 256);
            notify_new_sync(interface);
            if(changes == TRUE) {
              notify_new_sync_master(interface);
            }
          }
        }
        if(((rxframeIsSelf == FALSE) || ((interface->options & MILCAN_A_OPTION_ECHO) && (rxframeIsSelf == TRUE))) &&
          ((rxframeIsControl == FALSE) || ((interface->options & MILCAN_A_OPTION_LISTEN_CONTROL) && (rxframeIsControl == TRUE)))) {
          milcan_add_to_rx_buffer(interface, rxframe);
        }
      }
      if((interface->options & MILCAN_A_OPTION_SYNC_MASTER) == MILCAN_A_OPTION_SYNC_MASTER) {
        // We can be sync master.
        // Send a sync if the sync time has 80% expired and if we're higher priority than anything that we've seen so far.
        if((now >= (interface->syncTimer - SYNC_PERIOD_20PC(interface->sync_time_ns))) && ((interface->current_sync_master == 0) || (interface->sourceAddress < interface->current_sync_master))) {
          send_sync_frame(interface);
          interface->current_sync_master = interface->sourceAddress;
          notify_new_sync(interface);
          notify_new_sync_master(interface);
        }
        // Send a sync if the sync time has 99% expired and we're already the highest priority seen so far.
        if((now >= (interface->syncTimer - SYNC_PERIOD_1PC(interface->sync_time_ns))) && (interface->sourceAddress == interface->current_sync_master)) {
          send_sync_frame(interface);
          notify_new_sync(interface);
        }
      }
      // Leave Pre-Operational mode to Operational mode if there has been a sync frame and the Sync Slave Timeout Period has occurred.
      if((nanos() >= interface->mode_exit_timer) && (interface->current_sync_master != 0x00)) {
        change_mode(interface, MILCAN_A_MODE_OPERATIONAL);
      }
      break;
    case MILCAN_A_MODE_OPERATIONAL:           // Normal usage
      // Save anything to Rx Q that needs saving.
      if(rxframeValid == MILCAN_OK) {
        if(((rxframeIsSelf == FALSE) || ((interface->options & MILCAN_A_OPTION_ECHO) && (rxframeIsSelf == TRUE))) &&
          ((rxframeIsControl == FALSE) || ((interface->options & MILCAN_A_OPTION_LISTEN_CONTROL) && (rxframeIsControl == TRUE)))) {
          milcan_add_to_rx_buffer(interface, rxframe);
        }

        if(((rxframe->frame.can_id & MILCAN_ID_PRIMARY_MASK) == MILCAN_ID_PRIMARY_SYSTEM_MANAGEMENT) && ((rxframe->frame.can_id & MILCAN_ID_SECONDARY_MASK) == (MILCAN_ID_SECONDARY_SYSTEM_MANAGEMENT_SYNC_FRAME << 8))) {
          // It's a Sync Frame!
          if((interface->current_sync_master == 0) || ((rxframe->frame.can_id & MILCAN_ID_SOURCE_MASK) <= interface->current_sync_master)) {
            if(rxframeIsSelf == FALSE) {
              // Save the Sync Value.
              int changes = FALSE;
              if(interface->current_sync_master != (rxframe->frame.can_id & MILCAN_ID_SOURCE_MASK)) {
                changes = TRUE;
              }
              interface->current_sync_master = (uint8_t) (rxframe->frame.can_id & MILCAN_ID_SOURCE_MASK);
              interface->syncTimer = now + interface->sync_time_ns;  // Next period from now.
              interface->sync = rxframe->frame.data[0] + ((uint16_t) rxframe->frame.data[1] * 256);
              interface->mode_exit_timer = now + (8 * interface->sync_time_ns);
              notify_new_sync(interface);
              if(changes == TRUE) {
                notify_new_sync_master(interface);
              }
            }
          } else {
            // TEST CODE!
            LOGI(TAG, "IGNORE SYNC: %02x:%0x%02x, Ours: %02x:%03x", (rxframe->frame.can_id & MILCAN_ID_SOURCE_MASK), rxframe->frame.data[1], rxframe->frame.data[0],
            interface->sourceAddress, interface->sync);
          }
        }
      }
      if((interface->options & MILCAN_A_OPTION_SYNC_MASTER) == MILCAN_A_OPTION_SYNC_MASTER) {
        // We can be a SYNC MASTER
        if(interface->current_sync_master == interface->sourceAddress) {
          // We are the current SYNC MASTER - Tx at 99% PTU (we've found that seems to work best).
          if(now >= (interface->syncTimer - SYNC_PERIOD_1PC(interface->sync_time_ns))) {
            send_sync_frame(interface);
            interface->mode_exit_timer = now + (8 * interface->sync_time_ns);
            notify_new_sync(interface);
          }
        } else if((interface->current_sync_master == 0) || (interface->sourceAddress < interface->current_sync_master)) {
          // We aren't the current SYNC MASTER but we have higher priority than the current SYNC MASTER so we Tx at 80% of PTU.
          if(now >= (interface->syncTimer - SYNC_PERIOD_20PC(interface->sync_time_ns))) {
            send_sync_frame(interface);
            interface->current_sync_master = interface->sourceAddress;
            interface->mode_exit_timer = now + (8 * interface->sync_time_ns);
            notify_new_sync(interface);
            notify_new_sync_master(interface);
          }
        // } else {
        //   // We aren't the current SYNC MASTER and we have a lower priority than them so don't transmit sync frames.
        }
      }
      // Transmit anything that need transmitting form the Tx Q.
      qframe = interface_tx_read_q(interface);
      if(qframe != NULL) {
        interface_send(interface, qframe);
        free(qframe);
        qframe = NULL;
      }
      // Have we had a sync frame in time? If not, go to PRE-OPERATIONAL mode.
      if(now >= interface->mode_exit_timer) {
        change_mode(interface, MILCAN_A_MODE_PRE_OPERATIONAL);
      }
      break;
    case MILCAN_A_MODE_SYSTEM_CONFIGURATION:  // Config Messages only
      // No sync messages.
      // Looking for Enter Config Messages - After 8 seconds without these we must leave to Pre-Operational.
      // Looking for Exit Config Messages - After successful reception we must exit to Pre-Operational.
      // We always exit to Pre-Operational.

      // TO DO! Check for Exit Config message sequence.
      // TO DO! Check for Enter Config message sequence.
      // TO DO! Tx anything that needs Txing and is valid for this mode.
      
      // Save anything to Rx Q that needs saving.
      if(rxframeValid == MILCAN_OK) {
        if(((rxframeIsSelf == FALSE) || ((interface->options & MILCAN_A_OPTION_ECHO) && (rxframeIsSelf == TRUE))) &&
          ((rxframeIsControl == FALSE) || ((interface->options & MILCAN_A_OPTION_LISTEN_CONTROL) && (rxframeIsControl == TRUE)))) {
          milcan_add_to_rx_buffer(interface, rxframe);
        }
      }
      break;
  }
}

static void * EventHandler(void * eventContext)
{
  struct milcan_a* interface = (struct milcan_a*)eventContext;
  struct milcan_frame frame;
  int frameValid = MILCAN_ERROR_EOF;
  LOGI(TAG, "Enter event handler");
  while (interface->eventRunFlag == TRUE) {
    frameValid = interface_handle_rx(interface, &frame);  // Check anything to read an put it in the Rx Q.
    doStateMachine(interface, frameValid, &frame); // The state machne goes here.
  }
  LOGI(TAG, "Exit event handler");
  
  pthread_exit(NULL);  // Terminate thread
}

void milcan_display_mode(void* interface) {
  struct milcan_a* i = (struct milcan_a*)interface;
  switch(i->mode) {
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
        LOGE(TAG, "Mode: Unrecognised (%u)", i->mode);
        break;
  }
}

/// @brief Open a new interface.
void * milcan_open(uint8_t speed, uint16_t sync_freq_hz, uint8_t sourceAddress, uint8_t can_interface_type, uint16_t moduleNumber, uint16_t options) {
  struct milcan_a* interface = NULL;
  interface = interface_open(speed, sync_freq_hz, sourceAddress, can_interface_type, moduleNumber, options);
  if(NULL == interface) {
    return NULL;
  }
  set_sync_slave_time_ns(interface, 0); // Set the slave sync time to the minimum acceptable value.

  // We've connected so start the background tasks.
  // Start the rx thread.
  interface->eventRunFlag = TRUE;
  if (pthread_create(&(interface->rxThreadId), NULL, EventHandler, (void *)interface) == 0)
  {
    // Thread started.
    LOGI(TAG, "Thread started!");
    milcan_display_mode(interface);
  }
  else
  {
    LOGE(TAG, "Unable to create thread.");
    interface = interface_close(interface);
  }

  return (void*) interface;
}

/// @brief Close the interface.
void milcan_close(void * interface) {
  interface_close((struct milcan_a*) interface);
}

// Add a message to the output stack.
int milcan_send(void* interface, struct milcan_frame * frame) {
  return interface_tx_add_to_q(interface, frame);
}

// Read a mesage from the incoming stack.
int milcan_recv(void* interface, struct milcan_frame * frame) {
  struct milcan_a* i = (struct milcan_a*)interface;
  int ret = 0;
  pthread_mutex_lock(&(i->rx.rxBufferMutex));
  if(i->rx.write_offset > 0) {
    memcpy(frame, &(i->rx.buffer[0]), sizeof(struct milcan_frame));
    memmove(&(i->rx.buffer[0]), &(i->rx.buffer[1]), sizeof(struct milcan_frame) * (RX_BUFFER_SIZE  -1));
    if(i->rx.write_offset > 0) {  // We shouldn't have to to check but memory errors suck so I'll check.
      i->rx.write_offset--;
    }
    ret = 1;
  }
  pthread_mutex_unlock(&(i->rx.rxBufferMutex));

  return ret;
}