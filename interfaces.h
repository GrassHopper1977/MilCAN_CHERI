// interfaces.h
#ifndef __INTERFACES_H__
#define __INTERFACES_H__
#include <inttypes.h>
#include "milcan.h"
#include "gsusb.h"

#define MAX_BITS_PER_FRAME  (143) // The maximum with bit stuffing is 140 then 3 bits of interframe spacing.

#define RX_BUFFER_SIZE  (30)  // How big our receive buffer is.

#define MILCAN_A_SYNC_COUNT_MASK        (0x03FF)    // 0 to 1023
#define SYNC_PERIOD_0_5PC(a) (uint64_t)((a) * 0.005)
#define SYNC_PERIOD_PC(a, b) (uint64_t)((a) * (b))
#define SYNC_PERIOD_1PC(a) (uint64_t)((a) * 0.01)
#define SYNC_PERIOD_20PC(a) (uint64_t)((a) * 0.2)
#define SYNC_PERIOD_80PC(a) (uint64_t)((a) * 0.8)

struct milcan_rx_q {
  pthread_mutex_t rxBufferMutex;  // Mutex to control threaded access to read data buffer.
  struct milcan_frame buffer[RX_BUFFER_SIZE]; // The input buffer.
  uint16_t write_offset;
};

/// @brief Structure for the link list entries
struct list_milcan_frame {
    struct milcan_frame* frame;
    struct list_milcan_frame* next;
};

struct milcan_tx_q {
  pthread_mutex_t txBufferMutex;  // Mutex to control threaded access to read data buffer.
  struct list_milcan_frame* tx_queue[MILCAN_ID_PRIORITY_COUNT];
};

struct milcan_a {
  uint8_t sourceAddress;        // This device's physical network address
  uint8_t can_interface_type;   // The CAN Interface type e.g. CAN_INTERFACE_GSUSB_FIFO
  uint8_t speed;                // The MilCAN speed MILCAN_A_250K, MILCAN_A_500K or, MILCAN_A_1M
  uint16_t sync;                // The Sync Slot Counter - this is masked with MILCAN_A_SYNC_COUNT_MASK after any changes
  uint64_t syncTimer;           // Used to calculate when the next sync frame is due to be sent.
  uint16_t sync_freq_hz;        // The frequency to send the sync frame
  uint64_t sync_time_ns;        // The period to send the sync frame in ns - this is also called the PTU (Primary Time Unit)
  uint8_t current_sync_master;  // Who is the current Sync Master?
  int rfdfifo;                  // Read FIFO
  int wfdfifo;                  // Write FIFO
  int mode;                     // The current MILCAN_A_MODE
  uint16_t options;             // The various MILCAN_A_OPTION
  struct gsusb_ctx ctx;         // The context for the GSUSB USB to CAN driver
  pthread_t rxThreadId;         // Read thread ID.
  uint8_t eventRunFlag;         // Used to close the therad when exiting.
  struct milcan_rx_q rx;        // The input buffer.
  struct milcan_tx_q tx;        // The output buffer.
  uint64_t sync_slave_time_ns;  // The sync slave time in ns. This is how long we have to wait without a sync before we attempt to take over a sync master.
  uint64_t mode_exit_timer;     // Used to time the various timers to exit the modes.
  uint8_t config_flags;         // Used to control entry and exit of Config Mode.
  uint8_t config_counter;       // Which of the config messages we are on.
  uint64_t config_timer;        // Used for a 1 second timer to Tx the next Enter Config Message Chain
  uint8_t config_enter_count;   // Count our position through reading the enter config messages.
  uint64_t config_enter_timeout;// Whole message must be less than 400ms or start again.
};

// Function definitions
struct milcan_a* interface_open(uint8_t speed, uint16_t sync_freq_hz, uint8_t sourceAddress, uint8_t can_interface_type, uint16_t moduleNumber, uint16_t options);
struct milcan_a* interface_close(struct milcan_a* milcan_a);
int interface_send(struct milcan_a* interface, struct milcan_frame * frame);
// void interface_display_mode(struct milcan_a* interface);
// int interface_recv(struct milcan_a* interface, struct milcan_frame *frame);
int interface_handle_rx(struct milcan_a* interface, struct milcan_frame* frame);
int interface_tx_add_to_q(struct milcan_a* interface, struct milcan_frame *frame);
struct milcan_frame * interface_tx_read_q(struct milcan_a* interface);

#endif  // __INTERFACES_H__
