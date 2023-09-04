//txq.h

#ifndef __TXQ_H__
#define __TXQ_H__

#include "../BSD-USB-to-CAN/usb2can.h"

/// @brief Adds a CAN frame to the output buffer. They will be added taking into account the message priority. Invalid messages will be discarded.
/// @param frame The CAN frame to transmit.
extern int txQAdd(struct milcan_frame* frame);

/// @brief Returns a pointer to the next CAN frame to be sent. If the queue is empty then returns NULL.
extern struct milcan_frame* txQRead(uint8_t priority);

#endif  // __TXQ_H__