// interfaces.h
#ifndef __INTERFACES_H__
#define __INTERFACES_H__
#include <inttypes.h>
#include "milcan.h"

#define CAN_INTERFACE_SOCKET_CAN    0   // Not supported yet
#define CAN_INTERFACE_GSUSB_FIFO    1   // Our BSD-USB-to-CAN implementation over FIFOs
#define CAN_INTERFACE_GSUSB_SOCK    2   // Our BSD-USB-to-CAN implementation over sockets

// Bit Rates as defined in MWG-MILA-001 Rev 3 Section 2.4.1 (Page 13 of 79)
#define MILCAN_A_250K   0
#define MILCAN_A_500K   1
#define MILCAN_A_1M     2

// Sync Frame Frequencies as defined in MWG-MILA-001 Rev 3 Section 3.2.5.3 (Page 19 of 79)
// These are recommended frequnecies so we shoudl allow for these to be changed.
#define MILCAN_A_250K_DEFAULT_SYNC_HZ   (512)
#define MILCAN_A_500K_DEFAULT_SYNC_HZ   (128)
#define MILCAN_A_1M_DEFAULT_SYNC_HZ     (64)
#define MILCAN_A_SYNC_COUNT_MASK        (0x03FF)    // 0 to 1023
#define SYNC_PERIOD_1PC(a) (uint64_t)((a) * 0.01)
#define SYNC_PERIOD_20PC(a) (uint64_t)((a) * 0.2)

// System modes as defined in MWG-MILA-001 Rev 3 Section 4.2 (Page 37 of 79)
#define MILCAN_A_MODE_POWER_OFF             (0) // System is off
#define MILCAN_A_MODE_PRE_OPERATIONAL       (1) // The only messages that we can send are Sync or Enter Config
#define MILCAN_A_MODE_OPERATIONAL           (2) // Normal usage
#define MILCAN_A_MODE_SYSTEM_CONFIGURATION  (3) // Config Messages only

#define MILCAN_A_OPTION_SYNC_MASTER     (0x0001)    // This device can be a Sync Master

struct milcan_a {
    uint8_t sourceAddress;      // This device's physical network address
    uint8_t can_interface_type; // The CAN Interface type e.g. CAN_INTERFACE_GSUSB_FIFO
    uint8_t speed;              // The MilCAN speed MILCAN_A_250K, MILCAN_A_500K or, MILCAN_A_1M
    uint16_t sync;              // The Sync Slot Counter - this is masked with MILCAN_A_SYNC_COUNT_MASK after any changes
    uint64_t syncTimer;         // Used to calculate when the next sync frame is due to be sent.
    uint16_t sync_freq_hz;      // The frequency to send the sync frame
    uint64_t sync_time_ns;      // The period to send the sync frame in ns - this is also called the PTU (Primary Time Unit)
    uint8_t current_sync_master;    // Who is the current Sync Master?
    int rfdfifo;                // Read FIFO
    int wfdfifo;                // Write FIFO
    int mode;                   // The current MILCAN_A_MODE
    uint16_t options;           // The various MILCAN_A_OPTION
};

// Function definitions
extern struct milcan_a* milcan_open(uint8_t speed, uint16_t sync_freq_hz, uint8_t sourceAddress, uint8_t can_interface_type, char* address, uint16_t port, uint16_t options);
extern struct milcan_a* milcan_close(struct milcan_a* milcan_a);
extern ssize_t milcan_send(struct milcan_a* interface, struct milcan_frame * frame);
extern int milcan_recv(struct milcan_a* interface, int toRead);
extern void milcan_display_mode(struct milcan_a* interface);
#endif  // __INTERFACES_H__
