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

#define BAUDRATE B38400
#define MODEMDEVICE "/dev/ttyS1"
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

#define FLAG 0x7E
#define A    0x03
#define SET  0x03
#define UA  0x07

#define INIT_S 0
#define ADDRESS_S 1
#define CONTROL_S 2
#define BCC1_S 3
#define END_F 4
#define END_S 5

#define TIMEOUT 3
#define MAX_SIZE 512

typedef enum serial_status {TRANSMIT, RECEIVE} serialStatus;

typedef struct applicationLayer {
  int fd;
  serialStatus status;
} applicationLayer;

typedef struct linkLayer {
  char port[20];
  int baudRate;
  unsigned int sequenceNumber;
  unsigned int timeout;
  unsigned int numTransmissions;
  char frame[MAX_SIZE];
} linkLayer;

//TODO move to .h
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

int llopen(int port, serialStatus role) {

  int fd, res;
  struct termios oldtio, newtio;
  applicationLayer appLayer;
  
  strncpy(link_layer.port, "/dev/ttyS" + port, 10);

  uint8_t buf[255];
  uint8_t receive[1];

  uint8_t state = INIT_S;
  uint16_t nread = 0;



  bzero(&buf, sizeof(buf));

  /*
  Open serial port device for reading and writing and not as controlling tty
  because we don't want to get killed if linenoise sends CTRL-C.
  */

  app_layer.fd = open(link_layer.port, O_RDWR | O_NOCTTY);
  if (app_layer.fd < 0) {perror(link_layer.port); exit(-1); }

  if (tcgetattr(app_layer.fd, &oldtio) == -1) { /* save current port settings */
    perror("tcgetattr");
    exit(-1);
  }

  bzero(&newtio, sizeof(newtio));
  newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
  newtio.c_iflag = IGNPAR;
  newtio.c_oflag = 0;

  /*Set input mode (non-canonical, no echo,...) */
  newtio.c_lflag = 0;

  newtio.c_cc[VTIME]    = 10;   /* inter-character timer unused */
  newtio.c_cc[VMIN]     = 0;   /* blocking read until 5 chars received */

  /*
  VTIME e VMIN devem ser alterados de forma a proteger com um temporizador ad
  leitura do(s) pr�ximo(s) caracter(es)
  */

  tcflush(app_layer.fd, TCIOFLUSH);

  if (tcsetattr(app_layer.fd, TCSANOW, &newtio) == -1) {
    perror("tcsetattr");
    exit(-1);
  }

  //Main routine

  uint8_t super_frame[5];
  super_frame[0] = FLAG;
  super_frame[1] = A;     //Address Field
  super_frame[2] = SET;   //Control Field
  super_frame[3] = A^SET; //BCC
  super_frame[4] = FLAG;

  printf("New termios structure set\n");

  size_t i = 0;
  for(i; i < sizeof(super_frame); i++) {
    LOG_MSG("0x%02X\n", super_frame[i]);
  }

  uint8_t succeeded = 0;

  while(alarm_count < TIMEOUT) {

    res = write(app_layer.fd, super_frame, sizeof(super_frame));

    LOG_MSG("%d bytes written\n", res);

    if(alarm_flag) {
      alarm(3);
      alarm_flag = 0;
    }

    while (STOP == FALSE) {

      res = read(app_layer.fd, receive, 1);

      LOG_MSG("Read: %d bytes\n", res);

      if(res != 0) {

        buf[nread] = receive[0];
        uint8_t BCC = A^UA;

        switch(state) {
    		case INIT_S:
    			if(buf[nread] == FLAG) state = ADDRESS_S;
    			else state = INIT_S;
    			break;
    		case ADDRESS_S:
    			if (buf[nread] == A) state = CONTROL_S;
    			else state = INIT_S;
    			break;
    		case CONTROL_S:
    			if (buf[nread] == UA) state = BCC1_S;
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

if (tcsetattr(app_layer.fd, TCSANOW, &oldtio) == -1) {
  perror("tcsetattr");
  exit(-1);
}

}

int main(int argc, char** argv) {

  if(argc < 2) {
    printf("Wrong number of arguments!\n");
    printf("\tUsage: nserial <int - port number>\n");
    exit(1);
  } else if((strcmp("0", argv[1] != 0)) && (strcmp("1", argv[1] != 0))) {
    printf("Port number must be 0 or 1!\n");
    printf("\tUsage: nserial <int - port number>\n");
    exit(1);
  }

  (void) signal(SIGALRM, incrementAlarm);

  llopen(0, TRANSMIT);

  close(app_layer.fd);
  return 0;
}
