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
#include <limits.h>
#include <errno.h>

#include "Globals.h"

#define ENABLE_DEBUG 1

#if ENABLE_DEBUG
#define LOG_MSG printf
#else
#define LOG_MSG(...)
#endif

applicationLayer app_layer; //TODO separate to applayer
linkLayer link_layer;

/*
  Activates timeout flag and increments the timeout count
*/
void timeoutAlarm() {

  link_layer.timeout_flag = TRUE;
  link_layer.timeout_count++;
  LOG_MSG("Alarm #%d\n", link_layer.timeout_count);
}

/*
  Creates a supervision or unnumbered frame from address and control field given, sets it on linkLayer struct
*/
void createSUFrame(uint8_t address, uint8_t control) {

  link_layer.frame[0] = FLAG;
  link_layer.frame[1] = address;
  link_layer.frame[2] = control;
  link_layer.frame[3] = BCC1(address, control);
  link_layer.frame[4] = FLAG;
}

/*
  Initializes serial port and sets up the needed fiels on linkLayer struct
*/
int initSerial(uint32_t baudrate, serialStatus role) {

  int fd;

  /* Open serial port for reading and writing and not as controlling terminal */
  fd = open(link_layer.port, O_RDWR | O_NOCTTY);
  if (fd < 0) {
    printf("initSerial: failed to open port %s.\n", link_layer.port);
    return -1;
  }

  /* Save current port settings */
  if(tcgetattr(fd, &link_layer.oldtio) == -1) {
    printf("initSerial: couldn't get port settings on port %s.\n", link_layer.port);
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
    printf("initSerial: couldn't set port settings on port %s.\n", link_layer.port);
    return -1;
  }

  link_layer.baudrate = baudrate;
  if(role == TRANSMIT) link_layer.timeout_flag = TRUE;
  link_layer.state = INIT;
  LOG_MSG("Using %s.\n", link_layer.port);
  
  return fd;
}

/*
  Link layer state machine
*/
void llstate(serialState* state, serialEvent event) {
	
	switch(*state) {
		case INIT:
			if(event == FLAG_E) *state = S_FLAG;
			else if(event == TIMEOUT_E) *state = INIT;
		break;
		case S_FLAG:
			if(event == FLAG_E) *state = E_FLAG;
			else if(event == TIMEOUT_E) *state = INIT;
		break;
		case E_FLAG:
			if(event == TIMEOUT_E) *state = INIT;
		break;
	}
}

/*
  Helper function for printing a full frame
*/
void printFrame(size_t size) {
	
	size_t i;
	for(i = 0; i < size; i++) {
		LOG_MSG("|0x%02X", link_layer.frame[i]);
	}
	
	LOG_MSG("|\n");
}

/*
  Checks if alarm has happened and resets it if it has
*/
void checkAlarm() {
	
	if(link_layer.timeout_flag) {
		alarm(TIMEOUT_S);
		link_layer.timeout_flag = FALSE;
	}
}

/*
  Turns off alarm and resets flag and timeout count
*/
void downAlarm() {
	
	alarm(0);
	link_layer.timeout_flag = FALSE;
	link_layer.timeout_count = 0;
}

/*
  Checks the header BCC and returns true if it checks out
*/
int checkHeaderBCC() {
	
	if(link_layer.frame[BCC1_INDEX] != (link_layer.frame[ADDRESS_INDEX] ^ link_layer.frame[CONTROL_INDEX])) {
		return FALSE;
	} else return TRUE;
}

/*
  Reads a full frame from the port and checks BCC1 and the control field expected, returns error type or no error on success
*/
serialError readFrame(int fd, uint8_t control_expected) {
	
	int nread = 0;
	
	while(TRUE) {

      nread = read(fd, link_layer.frame + link_layer.current_index, 1);
	  
      if(nread > 0) {
		  
		LOG_MSG("Received[%03i]: 0x%02X | Before state: %i |", link_layer.current_index, link_layer.frame[link_layer.current_index], link_layer.state);
		
		if(link_layer.frame[link_layer.current_index] == FLAG) {
			llstate(&link_layer.state, FLAG_E);
		}
		
		LOG_MSG(" After state: %i\n", link_layer.state);

        link_layer.current_index++;
      }

      if(link_layer.state == E_FLAG) {
		downAlarm();
		if(!checkHeaderBCC()) return BCC1_ERR;
		if(link_layer.frame[CONTROL_INDEX] != control_expected) return CONTROL_ERR;
        return NO_ERR;
      } else if(link_layer.timeout_flag == TRUE) {
        return TIMEOUT_ERR;
      }
    }
}

/*
  Resets the linkLayer struct fields needed to run another write/read loop
*/
void resetFrame() {
	
	link_layer.state = INIT;
	link_layer.current_index = 0;
	bzero(&link_layer.frame, sizeof(link_layer.frame));
}

/*
  Connection establishment specific to the transmitter, returns 0 on success, negative number otherwise
*/
int llopen_transmit(int fd) {
	
	uint8_t success = FALSE;
	int nwrite;
	
	while(link_layer.timeout_count < TIMEOUT) {

		resetFrame();
		createSUFrame(A_TX, C_SET);
		
		nwrite = write(fd, link_layer.frame, SU_FRAME_SIZE);

		LOG_MSG("Sent (b%03i)  :", nwrite);
		printFrame(SU_FRAME_SIZE);

		checkAlarm();
		
		resetFrame();
		serialError status = readFrame(fd, C_UA);
		
		if(status == NO_ERR) {
			success = TRUE;
			break;
		}
		
		if(status == BCC1_ERR || status == CONTROL_ERR) {
			LOG_MSG("BCC1 or Control field was not as expected.\n");
			return -1;
		}
    }
  
	LOG_MSG("Received     :");
	printFrame(link_layer.current_index);
	
	if(success) return 0;
	else {
		LOG_MSG("Maximum number of retries reached.\n");
		return -1;
	}
}

/*
  Connection establishment specific to the receiver, returns 0 on success, negative number otherwise
*/
int llopen_receive(int fd) {
	
	resetFrame();
	serialError status = readFrame(fd, C_SET);
	
	LOG_MSG("Received     :");
	printFrame(link_layer.current_index);
	
	if(status == BCC1_ERR || status == CONTROL_ERR) {
		LOG_MSG("BCC1 or Control field was not as expected.\n");
		return -1;
	}

	int nwrite;
	
	resetFrame();
	createSUFrame(A_TX, C_UA);
	
	nwrite = write(fd, link_layer.frame, SU_FRAME_SIZE);
	
	LOG_MSG("Sent (b%03i)  :", nwrite);
	printFrame(SU_FRAME_SIZE);
	
	return 0;
}

/*
  Attempts to establish connection by sending or receiving SET / UA and returns the file descriptor for the connection
*/
int llopen(int port, serialStatus role) {

  int fd;

  snprintf(link_layer.port, PORT_NAME_SIZE + 1, "%s%d", "/dev/ttyS", port);
  
  fd = initSerial(BAUDRATE, role); //TODO check return, cleanup function on fail

  if(role == TRANSMIT) llopen_transmit(fd); //TODO check return, cleanup function on fail
  else if(role == RECEIVE) llopen_receive(fd); //TODO check return, cleanup function on fail

  return fd;
}

/*
  //TODO WIP only restores old settings for now
*/
int llclose(int fd) {
	
	if (tcsetattr(fd, TCSANOW, &link_layer.oldtio) == -1) {
		printf("llclose: couldn't set port settings on port %s.\n", link_layer.port);
		return -1;
	}
	
	return 0;
}

//TODO move to applayer
/*
  Prints program usage
*/
void printUsage(char* program_name) {
	
    printf("\tUsage  : %s <tx | rx> <port number [0,1]>\n", program_name);
    printf("\tExample: %s tx 0\n", program_name);
	printf("\tExample: %s rx 1\n", program_name);
}

//TODO move to applayer
/*
  Parses a string and converts it to unsigned long in specified base
*/
unsigned long parseULong(char* str, int base) {
	
	char *endptr;
	unsigned long val;
	int errsv; //Preserve errno

	//Convert string to unsigned long
	errno = 0; //Clear errno
	val = strtoul(str, &endptr, base);
    errsv = errno;

	//Check for conversion errors
    if (errsv != 0) {
		return ULONG_MAX;
	}

	if (endptr == str) {
		return ULONG_MAX;
	}

	//Successful conversion
	return val;
}

//TODO move to applayer
/*
  Program entry point, parses user input and continues if validation succeeded
*/
int main(int argc, char** argv) {

  if(argc < 3) {
    printf("%s: wrong number of arguments!\n", argv[0]);
	printUsage(argv[0]);
    exit(1);
  } else if((strcmp("tx", argv[1]) != 0) && (strcmp("rx", argv[1]) != 0)) {
	printf("%s: mode selected does not exist, tx or rx are the available modes.\n", argv[0]);
	printUsage(argv[0]);
	exit(1);
  } else if((strcmp("0", argv[2]) != 0) && (strcmp("1", argv[2]) != 0)) {
    printf("%s: port number must be 0 or 1!\n", argv[0]);
	printUsage(argv[0]);
    exit(1);
  }

  (void) signal(SIGALRM, timeoutAlarm);
  
  if(strcmp("tx", argv[1]) == 0) app_layer.fd = llopen(parseULong(argv[2], 10), TRANSMIT);
  if(strcmp("rx", argv[1]) == 0) app_layer.fd = llopen(parseULong(argv[2], 10), RECEIVE);

  if(app_layer.fd < 0) exit(1); //TODO add error message
  //TODO add message about establishing connection
  
  llclose(app_layer.fd);
  
  close(app_layer.fd);
  return 0;
}
