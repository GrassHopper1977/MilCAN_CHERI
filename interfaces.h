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
#define SYNC_PERIOD_1PC(a) (uint64_t)((a) * 0.01)
#define SYNC_PERIOD_20PC(a) (uint64_t)((a) * 0.2)
#define SYNC_PERIOD_80PC(a) (uint64_t)((a) * 0.8)
// #define SYNC_SLAVE_TIMEOUT(ptu_len, )

// System modes as defined in MWG-MILA-001 Rev 3 Section 4.2 (Page 37 of 79)
#define MILCAN_A_MODE_POWER_OFF             (0) // System is off
#define MILCAN_A_MODE_PRE_OPERATIONAL       (1) // The only messages that we can send are Sync or Enter Config
#define MILCAN_A_MODE_OPERATIONAL           (2) // Normal usage
#define MILCAN_A_MODE_SYSTEM_CONFIGURATION  (3) // Config Messages only

struct milcan_rx_q {
  pthread_mutex_t rxBufferMutex;  // Mutex to control threaded access to read data buffer.
  struct milcan_frame buffer[RX_BUFFER_SIZE]; // The input buffer.
  uint16_t write_offset;
};

struct milcan_a {
  uint8_t sourceAddress;      // This device's physical network address
  uint8_t can_interface_type; // The CAN Interface type e.g. CAN_INTERFACE_GSUSB_FIFO
  uint8_t speed;              // The MilCAN speed MILCAN_A_250K, MILCAN_A_500K or, MILCAN_A_1M
  uint16_t sync;              // The Sync Slot Counter - this is masked with MILCAN_A_SYNC_COUNT_MASK after any changes
  uint64_t syncTimer;         // Used to calculate when the next sync frame is due to be sent.
  uint16_t sync_freq_hz;      // The frequency to send the sync frame
  uint64_t sync_time_ns;      // The period to send the sync frame in ns - this is also called the PTU (Primary Time Unit)
  uint64_t sync_slave_time_ns;  // The sync slave time in ns. This is how long we have to wait without a sync before we attempt to take over a sync master.
  uint8_t current_sync_master;    // Who is the current Sync Master?
  int rfdfifo;                // Read FIFO
  int wfdfifo;                // Write FIFO
  int mode;                   // The current MILCAN_A_MODE
  uint16_t options;           // The various MILCAN_A_OPTION
  // GSUSB data
  struct gsusb_ctx ctx;
  pthread_t rxThreadId; // Read thread ID.
  uint8_t eventRunFlag; // Used to close the therad when exiting.
  struct milcan_rx_q rx; // The input buffer.
};

// Function definitions
struct milcan_a* interface_open(uint8_t speed, uint16_t sync_freq_hz, uint8_t sourceAddress, uint8_t can_interface_type, uint16_t moduleNumber, uint16_t options);
struct milcan_a* interface_close(struct milcan_a* milcan_a);
int interface_send(struct milcan_a* interface, struct milcan_frame * frame);
void interface_display_mode(struct milcan_a* interface);
int interface_recv(struct milcan_a* interface, struct milcan_frame *frame);
int interface_handle_rx(struct milcan_a* interface);
int interface_q_tx(struct milcan_a* interface, struct milcan_frame *frame);

#endif  // __INTERFACES_H__
