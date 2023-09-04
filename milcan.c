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


#include "../BSD-USB-to-CAN/usb2can.h"
#include "timestamp.h"
#define LOG_LEVEL 3
#include "logs.h"
#include "milcan.h"

#define BUFSIZE 1024

// function prototypes
// int tcpopen(const char *host, int port);
// void sendbuftosck(int sckfd, const char *buf, int len);
int sendcantosck(int fd, struct milcan_frame * frame);

#define KQUEUE_CH_SIZE 	1
#define KQUEUE_EV_SIZE	10
#define TIMER_FD  1234

#define TAG "MilCAN"

void print_can_frame(const char* tag, struct can_frame *frame, uint8_t err, const char *format, ...) {
  FILE * fd = stdout;

  if(err || (frame->can_id & CAN_ERR_FLAG)) {
    fd = stderr;
  }
  fprintf(fd, "%lu: %s: %s() line %i: %s: ID: ", nanos(), __FILE__, __FUNCTION__, __LINE__, tag);

  if((frame->can_id & CAN_EFF_FLAG) || (frame->can_id & CAN_ERR_FLAG)) {
    fprintf(fd, "%08x", frame->can_id & CAN_EFF_MASK);
  } else {
    fprintf(fd, "     %03x", frame->can_id & CAN_SFF_MASK);
  }
  fprintf(fd, ", len: %2u", frame->len);
  fprintf(fd, ", Data: ");
  for(int n = 0; n < CAN_MAX_DLC; n++) {
    if(n < frame->len) {
      fprintf(fd, "%02x, ", frame->data[n]);
    } else {
      fprintf(fd, "    ");
    }
  }

  va_list args;
  va_start(args, format);

  vfprintf(fd, format, args);
  va_end(args);

  if(frame->can_id & CAN_ERR_FLAG) {
    fprintf(fd, ", ERROR FRAME");
  }

  fprintf(fd, "\n");
}

void diep(const char *s) {
  perror(s); exit(EXIT_FAILURE);
}

void sigint_handler(int sig) {
  printf("\nSignal received (%i).\n", sig);
  fflush(stdout);
  fflush(stderr);
  if(sig == SIGINT) {
    // Make sure the signal is passed down the line correctly.
    signal(SIGINT, SIG_DFL);
    kill(getpid(), SIGINT);
  }
}

#define SYNC_PERIOD_NS  7812500L
#define SYNC_PERIOD_NS_1PC (uint64_t)(SYNC_PERIOD_NS * 0.01)

void checkTimer(uint64_t* timer, int wfdfifo, uint8_t localAddress, uint16_t* syncCounter) {
  // struct can_frame frame;	// The incoming frame
  uint64_t now = nanos();
  // static uint16_t count = 0;
  
  if(now >= (*timer - SYNC_PERIOD_NS_1PC)) {
    *timer = now + SYNC_PERIOD_NS;  // Next period from now.
    // *timer += SYNC_PERIOD_NS; // Next period from when we should've been.

    // // MilCAN Sync Frame
    // frame.can_id = 0x0200802A | CAN_EFF_FLAG;
    // frame.len = 2;
    // frame.data[0] = (uint8_t)(count & 0x000000FF);
    // frame.data[1] = (uint8_t)((count >> 8) & 0x00000003);
    // frame.data[2] = 0x00;
    // frame.data[3] = 0x00;
    // frame.data[4] = 0x00;
    // frame.data[5] = 0x00;
    // frame.data[6] = 0x00;
    // frame.data[7] = 0x00;
    // count++;
    // count &= 0x000003FF;

    // MilCAN Sync Frame
    struct milcan_frame frame = MILCAN_MAKE_SYNC(localAddress, *syncCounter);
    (*syncCounter)++;
    (*syncCounter) &= 0x000003FF;

    sendcantosck(wfdfifo, &frame);
  }
}

void displayRTpriority() {
  // Get the current real time priority
  struct rtprio rtdata;
  LOGI(TAG, "%s(): Getting Real Time Priority settings.\n", __FUNCTION__);
  int ret = rtprio(RTP_LOOKUP, 0, &rtdata);
  if(ret < 0) {
    switch(errno) {
      default:
        LOGE(TAG, "%s(): ERROR rtprio returned unknown error (%i)\n", __FUNCTION__, errno);
        break;
      case EFAULT:
        LOGE(TAG, "%s(): EFAULT Pointer to struct rtprio is invalid.\n", __FUNCTION__);
        break;
      case EINVAL:
        LOGE(TAG, "%s(): EINVAL The specified priocess was out of range.\n", __FUNCTION__);
        break;
      case EPERM:
        LOGE(TAG, "%s(): EPERM The calling thread is not allowed to set the priority. Try running as SU or root.\n", __FUNCTION__);
        break;
      case ESRCH:
        LOGE(TAG, "%s(): ESRCH The specified process or thread could not be found.\n", __FUNCTION__);
        break;
    }
  } else {
    switch(rtdata.type) {
      case RTP_PRIO_REALTIME:
        LOGI(TAG, "%s(): INFO Real Time Priority type is: RTP_PRIO_REALTIME\n", __FUNCTION__);
        break;
      case RTP_PRIO_NORMAL:
        LOGI(TAG, "%s(): INFO Real Time Priority type is: RTP_PRIO_NORMAL\n", __FUNCTION__);
        break;
      case RTP_PRIO_IDLE:
        LOGI(TAG, "%s(): INFO Real Time Priority type is: RTP_PRIO_IDLE\n", __FUNCTION__);
        break;
      default:
        LOGI(TAG, "%s(): INFO Real Time Priority type is: %u\n", __FUNCTION__, rtdata.type);
        break;
    }
    LOGI(TAG, "%s(): INFO Real Time Priority priority is: %u\n", __FUNCTION__, rtdata.prio);
  }
}

int setRTpriority(u_short prio) {
  struct rtprio rtdata;

  // Set the real time priority here
  LOGI(TAG, "%s(): Setting the Real Time Priority type to RTP_PRIO_REALTIME and priority to %u\n", __FUNCTION__, prio);
  rtdata.type = RTP_PRIO_REALTIME;  // Real Time priority
  // rtdata.type = RTP_PRIO_NORMAL;  // Normal
  // rtdata.type = RTP_PRIO_IDLE;  // Low priority
  rtdata.prio = prio;  // 0 = highest priority, 31 = lowest.
  int ret = rtprio(RTP_SET, 0, &rtdata);
  if(ret < 0) {
    switch(errno) {
      default:
        LOGE(TAG, "%s(): ERROR rtprio returned unknown error (%i)\n", __FUNCTION__, errno);
        break;
      case EFAULT:
        LOGE(TAG, "%s(): EFAULT Pointer to struct rtprio is invalid.\n", __FUNCTION__);
        break;
      case EINVAL:
        LOGE(TAG, "%s(): EINVAL The specified priocess was out of range.\n", __FUNCTION__);
        break;
      case EPERM:
        LOGE(TAG, "%s(): EPERM The calling thread is not allowed to set the priority. Try running as SU or root.\n", __FUNCTION__);
        break;
      case ESRCH:
        LOGE(TAG, "%s(): ESRCH The specified process or thread could not be found.\n", __FUNCTION__);
        break;
    }
  } else {
    switch(rtdata.type) {
      case RTP_PRIO_REALTIME:
        LOGI(TAG, "%s(): Real Time Priority type is: RTP_PRIO_REALTIME\n", __FUNCTION__);
        break;
      case RTP_PRIO_NORMAL:
        LOGI(TAG, "%s(): Real Time Priority type is: RTP_PRIO_NORMAL\n", __FUNCTION__);
        break;
      case RTP_PRIO_IDLE:
        LOGI(TAG, "%s(): Real Time Priority type is: RTP_PRIO_IDLE\n", __FUNCTION__);
        break;
      default:
        LOGI(TAG, "%s(): Real Time Priority type is: %u\n", __FUNCTION__, rtdata.type);
        break;
    }
    LOGI(TAG, "%s(): Real Time Priority priority is: %u\n", __FUNCTION__, rtdata.prio);
  }
  displayRTpriority();
  return ret;
}

int main(int argc, char *argv[])
{

  // Create the signal handler here - ensures that Ctrl-C gets passed back up to 
  signal(SIGINT, sigint_handler);

  struct kevent chlist[KQUEUE_CH_SIZE]; // events we want to monitor
  struct kevent evlist[KQUEUE_EV_SIZE]; // events that were triggered
  // char buf[BUFSIZE]; 
  // int sckfd;
  int kq, nev, i;
  struct can_frame frame;	// The incoming frame
  // int period_ms = 8;
  uint8_t localAddress = 42;
  uint16_t syncCounter = 0;
  char rfifopath[PATH_MAX + 1] = {0x00};  // We will open the read FIFO here.
  char wfifopath[PATH_MAX + 1] = {0x00};  // We will open the write FIFO here.
  int rfdfifo = -1;
  int wfdfifo = -1;
  uint64_t timer;

  sprintf(rfifopath,"%sr", argv[1]);
  sprintf(wfifopath,"%sw", argv[1]);

  printf("MilCAN Implementation V0.0.2\n\n");

  // check argument count
  if (argc != 2) {
    fprintf(stderr, "usage: %s pipe (e.g. /tmp/can0.0 Note: Omit the \"r\" or \"w\" on the end of the filename)\n", argv[0]);
    exit(EXIT_FAILURE);
  }
  LOGI(TAG, "starting...\n");

  // // open a connection to a host:port pair
  // sckfd = tcpopen(argv[1], atoi(argv[2]));

  // Open the FIFOs
  LOGI(TAG, "Opening FIFO (%s)...\n", rfifopath);
  rfdfifo = open(rfifopath, O_RDONLY | O_NONBLOCK);
  if(rfdfifo < 0) {
    LOGE(TAG, "unable to open FIFO (%s) for Read. Error \n", rfifopath);
    exit(EXIT_FAILURE);
  }

  LOGI(TAG, "Opening FIFO (%s)...\n", wfifopath);
  wfdfifo = open(wfifopath, O_WRONLY | O_NONBLOCK);
  if(wfdfifo < 0) {
    LOGE(TAG, "unable to open FIFO (%s) for Write. Error \n", wfifopath);
    exit(EXIT_FAILURE);
  }

  // create a new kernel event queue
  if ((kq = kqueue()) == -1) {
    LOGE(TAG, "Unable to create kqueue\n");
    exit(EXIT_FAILURE);
  }

  // initialise kevent structures
  // EV_SET(&chlist[0], sckfd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, 0);
  // EV_SET(&chlist[1], fileno(stdin), EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, 0);
  EV_SET(&chlist[0], rfdfifo, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, 0);
  // EV_SET(&chlist[1], TIMER_FD, EVFILT_TIMER, EV_ADD | EV_ENABLE, 0, period_ms, 0);
  nev = kevent(kq, chlist, KQUEUE_CH_SIZE, NULL, 0, NULL);
  if (nev < 0) {
    LOGE(TAG, "Unable to listen to kqueue\n");
    exit(EXIT_FAILURE);
  }

  printf("Starting loop...\n");
  // uint32_t count = 0;
  timer = nanos() + SYNC_PERIOD_NS;  // 8ms
  struct timespec zero_ts = {
    .tv_sec = 0,
    .tv_nsec = 0
  };

  // loop forever
  for (;;) {
    // nev = kevent(kq, NULL, 0, evlist, KQUEUE_EV_SIZE, NULL);  // Blocking
    checkTimer(&timer, wfdfifo, localAddress, &syncCounter);
    nev = kevent(kq, NULL, 0, evlist, KQUEUE_EV_SIZE, &zero_ts);  // Non-blocking

    if (nev < 0) {
      LOGE(TAG, "%s() Unable to listen to kqueue\n", __FUNCTION__);
      exit(EXIT_FAILURE);
    }
    else if (nev > 0) {
      for (i = 0; i < nev; i++) {
        if (evlist[i].flags & EV_EOF) {
          LOGE(TAG, "Read direction of socket has shutdown\n");
          exit(EXIT_FAILURE);
        }

        if (evlist[i].flags & EV_ERROR) {                /* report errors */
          LOGE(TAG, "EV_ERROR: %s\n", strerror(evlist[i].data));
          exit(EXIT_FAILURE);
        }
  
        // if(evlist[i].ident == TIMER_FD) {
        //   struct milcan_frame syncframe = MILCAN_MAKE_SYNC(localAddress, syncCounter);
        //   sendcantosck(sckfd, &syncframe.frame);
        //   syncCounter++;
        // } else 
        // if (evlist[i].ident == sckfd) {                  /* we have data from the host */
        //   memset(buf, 0, BUFSIZE);
        //   int i = recv(sckfd, &frame, sizeof(frame), 0);
        //   if (i < 0){                /* report errors */
        //     LOGE(TAG, "recv()\n");
        //     exit(EXIT_FAILURE);
        //   }

        //   print_can_frame(" PIPE IN", &frame, 0, "");
        // } else 
        if (evlist[i].ident == rfdfifo) {                  /* we have data from the host */
          int ret;
          int i = 0;
          int toread = (int)(evlist[i].data);
          do {
            i++;
            ret = read(rfdfifo, &frame, sizeof(struct can_frame));
            toread -= ret;
            if(ret != sizeof(struct can_frame)) {
              LOGE(TAG, "Read %u bytes, expected %lu bytes!\n", ret, sizeof(struct can_frame));
            } else {
              print_can_frame(TAG, &frame, 0, "PIPE IN");
            }
          } while (toread >= sizeof(struct can_frame));

        }
      }
    }
  }

  close(kq);
  return EXIT_SUCCESS;
}

int sendcantosck(int fd, struct milcan_frame * frame) {
  print_can_frame(TAG, &(frame->frame), 0, "PIPE OUT");
  return write(fd, &(frame->frame), sizeof(struct can_frame));
}
