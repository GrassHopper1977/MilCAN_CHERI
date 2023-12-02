// priorities.c

#include <stdio.h>      /* Standard input/output definitions */
#include <sys/types.h>
#include <errno.h>      /* Error number definitions */
#include <sys/rtprio.h>

#include "priorities.h"
// #define LOG_LEVEL 3
#include "logs.h"

#define TAG "Priorities"

void displayRTpriority() {
  // Get the current real time priority
  struct rtprio rtdata;
  LOGI(TAG, "Getting Real Time Priority settings.");
  int ret = rtprio(RTP_LOOKUP, 0, &rtdata);
  if(ret < 0) {
    switch(errno) {
      default:
        LOGE(TAG, "ERROR rtprio returned unknown error (%i).", errno);
        break;
      case EFAULT:
        LOGE(TAG, "EFAULT Pointer to struct rtprio is invalid.");
        break;
      case EINVAL:
        LOGE(TAG, "EINVAL The specified priocess was out of range.");
        break;
      case EPERM:
        LOGE(TAG, "EPERM The calling thread is not allowed to set the priority. Try running as SU or root.");
        break;
      case ESRCH:
        LOGE(TAG, "ESRCH The specified process or thread could not be found.");
        break;
    }
  } else {
    switch(rtdata.type) {
      case RTP_PRIO_REALTIME:
        LOGI(TAG, "Real Time Priority type is: RTP_PRIO_REALTIME.");
        break;
      case RTP_PRIO_NORMAL:
        LOGI(TAG, "Real Time Priority type is: RTP_PRIO_NORMAL.");
        break;
      case RTP_PRIO_IDLE:
        LOGI(TAG, "Real Time Priority type is: RTP_PRIO_IDLE.");
        break;
      default:
        LOGI(TAG, "Real Time Priority type is: %u.", rtdata.type);
        break;
    }
    LOGI(TAG, "Real Time Priority priority is: %u.", rtdata.prio);
  }
}

int setRTpriority(u_short prio) {
  struct rtprio rtdata;

  // Set the real time priority here
  LOGI(TAG, "Setting the Real Time Priority type to RTP_PRIO_REALTIME and priority to %u.", prio);
  rtdata.type = RTP_PRIO_REALTIME;  // Real Time priority
  // rtdata.type = RTP_PRIO_NORMAL;  // Normal
  // rtdata.type = RTP_PRIO_IDLE;  // Low priority
  rtdata.prio = prio;  // 0 = highest priority, 31 = lowest.
  int ret = rtprio(RTP_SET, 0, &rtdata);
  if(ret < 0) {
    switch(errno) {
      default:
        LOGE(TAG, "ERROR rtprio returned unknown error (%i).", errno);
        break;
      case EFAULT:
        LOGE(TAG, "EFAULT Pointer to struct rtprio is invalid.");
        break;
      case EINVAL:
        LOGE(TAG, "EINVAL The specified priocess was out of range.");
        break;
      case EPERM:
        LOGE(TAG, "EPERM The calling thread is not allowed to set the priority. Try running as SU or root.");
        break;
      case ESRCH:
        LOGE(TAG, "ESRCH The specified process or thread could not be found.");
        break;
    }
  } else {
    switch(rtdata.type) {
      case RTP_PRIO_REALTIME:
        LOGI(TAG, "Real Time Priority type is: RTP_PRIO_REALTIME.");
        break;
      case RTP_PRIO_NORMAL:
        LOGI(TAG, "Real Time Priority type is: RTP_PRIO_NORMAL.");
        break;
      case RTP_PRIO_IDLE:
        LOGI(TAG, "Real Time Priority type is: RTP_PRIO_IDLE.");
        break;
      default:
        LOGI(TAG, "Real Time Priority type is: %u.", rtdata.type);
        break;
    }
    LOGI(TAG, "Real Time Priority priority is: %u.", rtdata.prio);
  }
  displayRTpriority();
  return ret;
}
