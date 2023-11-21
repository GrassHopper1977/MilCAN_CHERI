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

#define BUFSIZE 1024

// function prototypes
// int sendcantosck(int fd, struct milcan_frame * frame);

#define SLEEP_TIME  100  // sleep time in us e.g. 1000 = 1ms

#define TAG "MilCAN"


void checkSync(struct milcan_a* interface) {
  uint64_t now = nanos();
  struct milcan_frame * qframe = NULL;

  if((interface->current_sync_master == interface->sourceAddress) // We are Sync Master
    || (((interface->options & MILCAN_A_OPTION_SYNC_MASTER) == MILCAN_A_OPTION_SYNC_MASTER) && 
    ((interface->current_sync_master == 0) || (interface->mode == MILCAN_A_MODE_PRE_OPERATIONAL)))) { // Or we want to be sync Master
    // if(now >= interface->syncTimer) {
    if(now >= (interface->syncTimer - SYNC_PERIOD_1PC(interface->sync_time_ns))) {
    // if(now >= (interface->syncTimer - SYNC_PERIOD_0_5PC(interface->sync_time_ns))) {
      interface->syncTimer = now + interface->sync_time_ns;  // Next period from now.
      // interface->syncTimer += interface->sync_time_ns; // Next period from when we should've been.

      // MilCAN Sync Frame
      interface->sync++;
      interface->sync &= 0x000003FF;
      struct milcan_frame frame = MILCAN_MAKE_SYNC(interface->sourceAddress, interface->sync);

      interface_send(interface, &frame);  // Sync frames bypass the Tx queue
    }
  } else if(((interface->options & MILCAN_A_OPTION_SYNC_MASTER) == MILCAN_A_OPTION_SYNC_MASTER) && (interface->sourceAddress < interface->current_sync_master)) {  // We are not Sync Master but we have higher priority than the current Sync Master.
    if(now >= (interface->syncTimer - SYNC_PERIOD_20PC(interface->sync_time_ns))) { // We can interrupt at 0.8 of PTU if we have a higher priority.
      LOGI(TAG, "2. We are HIGHER PRIORITY than current.");
      interface->syncTimer = now + interface->sync_time_ns;  // Next period from now.
      // interface->syncTimer += interface->sync_time_ns; // Next period from when we should've been.

      // MilCAN Sync Frame
      interface->sync++;
      interface->sync &= 0x000003FF;
      struct milcan_frame frame = MILCAN_MAKE_SYNC(interface->sourceAddress, interface->sync);

      interface_send(interface, &frame);  // Sync frames bypass the Tx queue
    }
  } else {
    // We are not the Sync Master but we haven't had a Sync Message for 8 PTUs
    // if(now >= (interface->syncTimer + (interface->sync_time_ns * 7))) {
    if(now >= (interface->syncTimer + interface->sync_slave_time_ns)) {
      LOGI(TAG, "3. No Sync for 8 PTUs.");
      interface->current_sync_master = 0;
      interface->mode = MILCAN_A_MODE_PRE_OPERATIONAL;
    }
  }

  switch(interface->mode) {
    case MILCAN_A_MODE_POWER_OFF:             // System is off
      break;
    case MILCAN_A_MODE_PRE_OPERATIONAL:       // The only messages that we can send are Sync or Enter Config
      break;
    case MILCAN_A_MODE_OPERATIONAL:           // Normal usage
      // Transmit anything that need transmitting form the Tx Q.
      qframe = interface_tx_read_q(interface);
      if(qframe != NULL) {
        interface_send(interface, qframe);
        free(qframe);
        qframe = NULL;
      }
      break;
    case MILCAN_A_MODE_SYSTEM_CONFIGURATION:  // Config Messages only
      break;

  }
}

static void * EventHandler(void * eventContext)
{
  struct milcan_a* interface = (struct milcan_a*)eventContext;
  LOGI(TAG, "Enter event handler");
  while (interface->eventRunFlag == TRUE) {
    interface_handle_rx(interface);  // Check anything to read an put it in the Rx Q.
    checkSync(interface); // Check Sync
  }
  LOGI(TAG, "Exit event handler");
  
  pthread_exit(NULL);  // Terminate thread
}

/// @brief Open a new interface.
void * milcan_open(uint8_t speed, uint16_t sync_freq_hz, uint8_t sourceAddress, uint8_t can_interface_type, uint16_t moduleNumber, uint16_t options) {
  struct milcan_a* interface = NULL;
  interface = interface_open(speed, sync_freq_hz, sourceAddress, can_interface_type, moduleNumber, options);
  if(NULL == interface) {
    return NULL;
  }

  // We've connected so start the background tasks.
  // Start the rx thread.
  interface->eventRunFlag = TRUE;
  if (pthread_create(&(interface->rxThreadId), NULL, EventHandler, (void *)interface) == 0)
  {
    // Thread started.
    LOGI(TAG, "Thread started!");
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
  return interface_recv(interface, frame);
}