#ifndef __TIMESTAMP_H__
#define __TIMESTAMP_H__

#include <inttypes.h>

// Convert milliseconds to nanoseconds
#define MS_TO_NS(ms) ((ms) * 1000000L)

/// Get a time stamp in milliseconds.
extern uint64_t millis();

/// Get a time stamp in microseconds.
extern uint64_t micros();

/// Get a time stamp in nanoseconds.
extern uint64_t nanos();

#endif // __TIMESTAMP_H__
