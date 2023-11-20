// txq.c
#include <inttypes.h>
#include <string.h>     /* String function definitions */
#include <errno.h>      /* Error number definitions */
//#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include "milcan.h"
#include "utils/timestamp.h"
#define LOG_LEVEL 3
#include "utils/logs.h"
#include "interfaces.h"

#include <stdio.h>      /* Standard input/output definitions */

#define TAG "TXQ"

void txQ_print_sizes(struct milcan_a* interface) {
    for(uint8_t i = 0; i < MILCAN_ID_PRIORITY_COUNT; i++) {
        uint16_t count = 0;
        struct list_milcan_frame** current = &(interface->tx.tx_queue[i]);
        while(*current != NULL) {
            current = &(*current)->next;
            count++;
        }
        printf("Priority %u: %u\n", i, count);
    }
}

/// @brief Adds a CAN frame to the output buffer. They will be added taking into account the message priority. Invalid messages will be discarded.
/// @param frame The CAN frame to transmit.
int txQAdd(struct milcan_a* interface, struct milcan_frame* frame) {
    // All CAN IDs are extended IDs (29-bit).
    // Bits 26 to 28 - Priority. 0 is the highest, 7 is the lowest.
    // Bit 25 - Should be 1 for a MilCAN message or 0 for SAE J1939
    // Bit 24 Request Bit - Request Message = 1 (0 bytes of data will be sent), Status/Command Message = 0
    // Bits 16 to 23 - Message Primary type
    // Bits 8 to 16 - Message Sub-Type
    // Bits 0 to 7 - Source Address (unique ID of ECU)

    // The lower the ID the higher the priority so the further up the queue it should be placed.
    struct list_milcan_frame* list_frame = calloc(1, sizeof(struct list_milcan_frame));
    if(frame == NULL) {
        return ENOMEM;  // We're out of memory!
    }
    frame->frame.can_id |= CAN_EFF_FLAG;
    if(frame->mortal != 0) {
        frame->mortal += nanos();
    }
    list_frame->frame = frame;
    list_frame->next = NULL;


    uint8_t priority = ((frame->frame.can_id & MILCAN_ID_PRIORITY_MASK) >> 26) & 0x07;
    struct list_milcan_frame** current = &(interface->tx.tx_queue[priority]);
    
    while(*current != NULL) {
        if((list_frame->frame->frame.can_id & MILCAN_ID_MASK ) < ((*current)->frame->frame.can_id & MILCAN_ID_MASK)) {
            list_frame->next = *current;
            *current = list_frame;
            break;
        } else {
            current = &(*current)->next;
        }
    }
    
    if(*current == NULL) {
        *current = list_frame;
    }

    return 0;
}

/// @brief Returns a pointer to the next milcan_frame of the required priority to be sent. If the queue is empty then returns NULL. Any mortal frame that has exceeded it's time to live will automtaiclaly be discarded.
struct milcan_frame* txQRead(struct milcan_a* interface, uint8_t priority) {
    if(priority >= MILCAN_ID_PRIORITY_COUNT) {
        LOGE(TAG, "Invalid priority level.");
        return NULL;
    }
    struct list_milcan_frame* head = interface->tx.tx_queue[priority];
    struct milcan_frame* frame = NULL;
    uint64_t now = nanos();

    while((frame == NULL) && (interface->tx.tx_queue[priority] != NULL)) {
        if((head->frame->mortal == 0) || (head->frame->mortal > now)) {
            frame = head->frame;
        }
        interface->tx.tx_queue[priority] = head->next;
        free(head);
        head = interface->tx.tx_queue[priority];
    }
    
    return frame;
}
