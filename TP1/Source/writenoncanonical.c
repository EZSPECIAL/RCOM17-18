/*Non-Canonical Input Processing*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>

#include "globals.h"


#define MODEMDEVICE "/dev/ttyS1"
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

#define INIT_S 0
#define ADDRESS_S 1
#define CONTROL_S 2
#define BCC1_S 3
#define END_F 4
#define END_S 5

#define TIMEOUT 3

applicationLayer app_layer;
linkLayer link_layer;

#define ENABLE_DEBUG 1

#if ENABLE_DEBUG
#define LOG_MSG printf
#else
#define LOG_MSG(...)
#endif

volatile int STOP = FALSE;
uint8_t alarm_count = 0;
uint8_t alarm_flag = 1;

void incrementAlarm() {

  LOG_MSG("Alarm #: %d\n", alarm_count);
  alarm_flag = 1;
  alarm_count++;
}

/*
  Creates a supervision or unnumbered frame from address and control field given, sets it on linkLayer struct
*/
int8_t createSUFrame(uint8_t address, uint8_t control) {

  link_layer.frame[0] = FLAG;
  link_layer.frame[1] = address;
  link_layer.frame[2] = control;
  link_layer.frame[3] = BCC1(address, control);
  link_layer.frame[4] = FLAG;

  return 0;
}

/*
  Initializes serial port, sets port name, baudrate and timeout attempts on linkLayer struct
*/
int init_serial(char port[10], uint32_t baudrate, uint8_t timeout) {

  int fd;

  /* Open serial port for reading and writing and not as controlling terminal */
  fd = open(port, O_RDWR | O_NOCTTY);
  if (fd < 0) {
    printf("init_serial: failed to open port %s\n", port);
    return -1;
  }

  /* Save current port settings */
  if(tcgetattr(fd, &link_layer.oldtio) == -1) {
    printf("init_serial: couldn't get port settings on port %s\n", port);
    return -1;
  }

  bzero(&link_layer.newtio, sizeof(link_layer.newtio));

  link_layer.newtio.c_cflag = baudrate | CS8 | CLOCAL | CREAD;
  link_layer.newtio.c_iflag = IGNPAR;
  link_layer.newtio.c_oflag = 0;
  link_layer.newtio.c_lflag = 0; /* Set input mode: non-canonical / no echo */

  link_layer.newtio.c_cc[VTIME] = 10; /* Inter-character timer */
  link_layer.newtio.c_cc[VMIN] = 0; /* Minimum number of characters to read */

  tcflush(fd, TCIOFLUSH);

  if(tcsetattr(fd, TCSANOW, &link_layer.newtio) == -1) {
    printf("init_serial: couldn't set port settings on port %s\n", port);
    return -1;
  }

  strncpy(link_layer.port, port, PORT_NAME_SIZE);
  link_layer.baudrate = baudrate;
  link_layer.timeout = timeout;
  return fd;
}

//TODO move to linkLayer
int llopen(int port, serialStatus role) {

  int res, fd;
  char portname[PORT_NAME_SIZE];

  strncpy(portname, "/dev/ttyS0", PORT_NAME_SIZE); //TODO fix user defined port

  printf("string: %s\n", link_layer.port);

  uint8_t buf[255];
  uint8_t receive[1];

  uint8_t state = INIT_S;
  uint16_t nread = 0;

  bzero(&buf, sizeof(buf));

  fd = init_serial(portname, BAUDRATE, TIMEOUT); //TODO check return, cleanup function on fail

  // uint8_t* su_frame = (uint8_t*) malloc(SU_FRAME_SIZE);
  // size_t su_frame_length = SU_FRAME_SIZE;

  createSUFrame(A_TX, C_SET); //TODO check return, cleanup function on fail

  printf("New termios structure set\n");

  size_t i;
  for(i = 0; i < SU_FRAME_SIZE; i++) {
    LOG_MSG("0x%02X ", link_layer.frame[i]);
  }

  uint8_t succeeded = 0;

  while(alarm_count < TIMEOUT) {

    res = write(fd, link_layer.frame, SU_FRAME_SIZE);

    LOG_MSG("%d bytes written\n", res);

    if(alarm_flag) {
      alarm(3);
      alarm_flag = 0;
    }

    while (STOP == FALSE) {

      res = read(fd, receive, 1);

      LOG_MSG("Read: %d bytes\n", res);

      if(res != 0) {

        buf[nread] = receive[0];
        // uint8_t BCC = A^UA;
        uint8_t BCC = BCC1(A_TX, C_UA);

        switch(state) {
          case INIT_S:
          if(buf[nread] == FLAG) state = ADDRESS_S;
          else state = INIT_S;
          break;
          case ADDRESS_S:
          if (buf[nread] == A_TX) state = CONTROL_S;
          else state = INIT_S;
          break;
          case CONTROL_S:
          if (buf[nread] == C_UA) state = BCC1_S;
          else state = INIT_S;
          break;
          case BCC1_S:
          if (buf[nread] == BCC) state = END_F;
          else state = INIT_S;
          break;
          case END_F:
          if (buf[nread] == FLAG) state = END_S;
          else state = INIT_S;
          break;
          default:
          state = INIT_S;
          break;
        }

        nread += res;
      }

      LOG_MSG("State: %i\n", state);

      if(state == END_S) {
        succeeded = 1;
        break;
      } else if(alarm_flag == 1) {
        break;
      }
    }

    if(succeeded) break;
  }

  alarm(0);

  if(succeeded) {
    for (i = 0; i < nread; i++){
      LOG_MSG("0x%02X\n", buf[i]);
    }
  }

  if (tcsetattr(fd, TCSANOW, &link_layer.oldtio) == -1) {
    perror("tcsetattr");
    exit(-1);
  }

  return 0;
}

int main(int argc, char** argv) {

  if(argc < 2) {
    printf("%s: wrong number of arguments!\n", argv[0]);
    printf("\tUsage: %s <port number [0,1]>\n", argv[0]);
    exit(1);
  } else if((strcmp("0", argv[1]) != 0) && (strcmp("1", argv[1]) != 0)) {
    printf("%s: port number must be 0 or 1!\n", argv[0]);
    exit(1);
  }

  (void) signal(SIGALRM, incrementAlarm);

  llopen(0, TRANSMIT);

  close(app_layer.fd);
  return 0;
}
