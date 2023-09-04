// interfaces.h

#ifndef __INTERFACES_H__
#define __INTERFACES_H__
#include <inttypes.h>
#include "milcan.h"

#define CAN_INTERFACE_SOCKET_CAN    0   // Not supported yet
#define CAN_INTERFACE_GSUSB         1   // Our BSD-USB-to-CAN implementation over FIFOs
#define CAN_INTERFACE_GSUSB_SOCK    2   // Our BSD-USB-to-CAN implementation over sockets

// Bit Rates as defined in MWG-MILA-001 Rev 3 Section 2.4.1 (Page 13 of 79)
#define MILCAN_A_250K   0
#define MILCAN_A_500K   1
#define MILCAN_A_1M     2

// Sync Frame Frequencies as defined in MWG-MILA-001 Rev 3 Section 3.2.5.3 (Page 19 of 79)
#define MILCAN_A_250K_DEFAULT_SYNC_HZ   (512)
#define MILCAN_A_500K_DEFAULT_SYNC_HZ   (128)
#define MILCAN_A_1M_DEFAULT_SYNC_HZ     (64)

// System modes as defined in MWG-MILA-001 Rev 3 Section 4.2 (Page 37 of 79)
#define MILCAN_A_MODE_POWER_OFF             (0)
#define MILCAN_A_MODE_PRE_OPERATIONAL       (1)
#define MILCAN_A_MODE_OPERATIONAL           (2)
#define MILCAN_A_MODE_SYSTEM_CONFIGURATION  (3)

struct milcan_a {
    uint8_t speed;
    uint16_t sync;
    int fdr;
    int fdw;
    int state;
};

// Function definitions
extern struct milcan_a* milcan_open(uint8_t speed, uint16_t sync_freq_hz);
extern int milcan_close(struct milcan_a* milcan_a);
extern int milcan_send(struct milcan_a* milcan_a);
extern int milcan_recv(struct milcan_a* milcan_a);
#endif  // __INTERFACES_H__
