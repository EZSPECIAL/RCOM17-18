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
#include "LinkLayer.h"
#include "Helper.h"

static linkLayer link_layer;

/*
Activates timeout flag and increments the timeout count
*/
void timeoutAlarm() {

  link_layer.timeout_flag = TRUE;
  link_layer.timeout_count++;
  signal(SIGALRM, timeoutAlarm);
}

/*
Creates a supervision or unnumbered frame from address and control field given (F | A | C | BCC | F)
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

  link_layer.newtio.c_cc[VTIME] = 1; /* Inter-character timer */
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
    LOG_MSG("Alarm #%d\n", link_layer.timeout_count);
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

  size_t nread = 0;
  size_t total_read = 0;

  while(TRUE) {

    nread = read(fd, link_layer.frame + link_layer.current_index, 1);

    if(nread > 0) {

      LOG_MSG("Received[%03i]: 0x%02X | Before state: %i |", link_layer.current_index, link_layer.frame[link_layer.current_index], link_layer.state);

      if(link_layer.frame[link_layer.current_index] == FLAG) {
        llstate(&link_layer.state, FLAG_E);
      }

      LOG_MSG(" After state: %i\n", link_layer.state);

      link_layer.current_index++;
      total_read += nread;
    }

    if(link_layer.state == E_FLAG) {
      downAlarm();
      if(!checkHeaderBCC()) return BCC1_ERR;
      if(link_layer.frame[CONTROL_INDEX] != control_expected) return CONTROL_ERR;
      link_layer.length = total_read;
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
Calculates the BCC corresponding to a data frame
*/
uint8_t calcBCC2(uint8_t* data_frame, size_t length) {

  uint8_t bcc = data_frame[0];

  size_t i;
  for(i = 1; i < length; i++) {

      bcc ^= data_frame[i];
  }

  return bcc;
}

/*
Searches for occurrences of flag or the escape character in the data frame and replaces them
*/
size_t byte_stuff(uint8_t* data_frame, size_t length) {

  char result[FRAME_MAX_SIZE];
  size_t add_length = 0;

  size_t i;
  for(i = 0; i < length; i++) {
    if(data_frame[i] == FLAG) {
      result[i + add_length] = ESCAPE_CHAR;
      result[i + 1 + add_length] = FLAG ^ XOCTET;
      add_length++;
    } else if(data_frame[i] == ESCAPE_CHAR) {
      result[i + add_length] = ESCAPE_CHAR;
      result[i + 1 + add_length] = ESCAPE_CHAR ^ XOCTET;
      add_length++;
    } else {
      result[i + add_length] = data_frame[i];
    }
  }

  bzero(data_frame, length);
  memcpy(data_frame, result, length + add_length);

  return (length + add_length);
}

/*
Searches for occurrences of the escape char and replaces it with the escaped char
*/
size_t byte_destuff(uint8_t* data_frame, size_t length) {

  char result[FRAME_MAX_SIZE];
  size_t new_length = 0;

  size_t i;
  for(i = 0; i < length; i++, new_length++) {
    if(data_frame[i] == ESCAPE_CHAR) {
      result[new_length] = data_frame[i + 1] ^ XOCTET;
      i++;
    } else result[new_length] = data_frame[i];
  }

  bzero(data_frame, length);
  memcpy(data_frame, result, new_length);

  return new_length;
}

/*
Extract data frame from a full frame with BCC2 included
*/
void extractDataFrame(uint8_t* data_frame, size_t length) {

  size_t i;
  for(i = 0; i < length - HEADER_SIZE; i++) {

    data_frame[i] = link_layer.frame[i + 4];
  }
}

/*
Creates an information frame from given parameters (F | A | C | BCC1 | D1..DN | BCC2 | F)
*/
void createIFrame(uint8_t address, uint8_t control, uint8_t* data_frame, size_t length) {

  link_layer.frame[0] = FLAG;
  link_layer.frame[1] = address;
  link_layer.frame[2] = control;
  link_layer.frame[3] = BCC1(address, control);

  memcpy(link_layer.frame + 4, data_frame, length);

  link_layer.frame[length + 4] = FLAG;
}

/*
Attempts to send data_frame after byte stuffing it and appending a frame header
*/
int llwrite(int fd, uint8_t* data_frame, size_t length) {

  /* Calculate BCC2 and append it to data_frame */
  uint8_t bcc_2 = calcBCC2(data_frame, length);
  data_frame[length] = bcc_2;

  /* Byte stuff the data_frame and update length */
  size_t stuffed_length = byte_stuff(data_frame, length + 1);
  length = stuffed_length;

  LOG_MSG("BCC2         : 0x%02X\n", bcc_2);
  LOG_MSG("Stuffed      :");
  printArrayAsHex(data_frame, length); //TODO remove stuffed debug

  uint8_t success = FALSE;
  int nwrite;

  signal(SIGALRM, timeoutAlarm);
  link_layer.timeout_flag = TRUE;

  while(link_layer.timeout_count <= TIMEOUT) {

    resetFrame();
    createIFrame(A_TX, C_NS(link_layer.sequence_number), data_frame, length);

    nwrite = write(fd, link_layer.frame, length + 5);

    LOG_MSG("Sent (b%03i)  :", nwrite);
    printFrame(length + 5); LOG_MSG("\n");

    checkAlarm();

    resetFrame();
    serialError status = readFrame(fd, C_RR(!link_layer.sequence_number));

    if(status == NO_ERR) {
      success = TRUE;
      break;
    }

    if(status == BCC1_ERR) //TODO no action?
    if(status == CONTROL_ERR) {
      LOG_MSG("BCC1 or Control field was not as expected.\n"); //TODO REJ
      return -1;
    }
  }

  LOG_MSG("Received     :");
  printFrame(link_layer.current_index);

  if(success) {
    link_layer.sequence_number = !link_layer.sequence_number;
    return nwrite;
  }
  else {
    LOG_MSG("Maximum number of retries reached.\n");
    return -1;
  }
}

/*
Attempts to read a data_frame, byte destuffs it and validates it
*/
int llread(int fd, uint8_t* data_frame) {

  resetFrame();
  serialError status = readFrame(fd, C_NS(link_layer.sequence_number));

  LOG_MSG("Received(%03d):", link_layer.length);
  printFrame(link_layer.current_index);

  /* Extract data frame with BCC2 included */
  extractDataFrame(data_frame, link_layer.length);

  /* Destuff data frame extracted */
  size_t destuffed_length = byte_destuff(data_frame, link_layer.length - HEADER_SIZE);
  link_layer.length = destuffed_length;

  if(status == BCC1_ERR || status == CONTROL_ERR) { //TODO handle
    LOG_MSG("BCC1 or Control field was not as expected.\n");
    return -1;
  }

  //TODO verify BCC2

  /* Send response */

  int nwrite;

  resetFrame();
  createSUFrame(A_TX, C_RR(!link_layer.sequence_number));

  nwrite = write(fd, link_layer.frame, SU_FRAME_SIZE);

  LOG_MSG("Sent (b%03i)  :", nwrite);
  printFrame(SU_FRAME_SIZE);

  link_layer.sequence_number = !link_layer.sequence_number; //Update sequence number

  return link_layer.length - 1;
}

/*
Connection establishment specific to the transmitter, returns 0 on success, negative number otherwise
*/
int llopen_transmit(int fd) {

  uint8_t success = FALSE;
  int nwrite;

  while(link_layer.timeout_count <= TIMEOUT) {

    resetFrame();
    createSUFrame(A_TX, C_SET);

    nwrite = write(fd, link_layer.frame, SU_FRAME_SIZE);

    LOG_MSG("Sent (%03d)   :", nwrite);
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

  LOG_MSG("Received(%03d):", link_layer.length);
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

  LOG_MSG("Received(%03d):", link_layer.length);
  printFrame(link_layer.current_index);

  if(status == BCC1_ERR || status == CONTROL_ERR) {
    LOG_MSG("BCC1 or Control field was not as expected.\n");
    return -1;
  }

  int nwrite;

  resetFrame();
  createSUFrame(A_TX, C_UA);

  nwrite = write(fd, link_layer.frame, SU_FRAME_SIZE);

  LOG_MSG("Sent (%03i)   :", nwrite);
  printFrame(SU_FRAME_SIZE);

  return 0;
}

/*
Attempts to establish connection by sending or receiving SET / UA and returns the file descriptor for the connection
*/
int llopen(int port, serialStatus role) {

  memset(&link_layer, 0, sizeof(link_layer));
  signal(SIGALRM, timeoutAlarm);

  int fd;

  snprintf(link_layer.port, PORT_NAME_SIZE + 1, "%s%d", "/dev/ttyS", port);

  fd = initSerial(BAUDRATE, role);

  if(fd < 0) return -1;

  if(role == TRANSMIT) {
    if(llopen_transmit(fd) != 0) {
      llreset(fd);
      return -1;
    }
  } else if(role == RECEIVE) {
    if(llopen_receive(fd) != 0) {
      llreset(fd);
      return -1;
    }
  }

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

/*
Restores previous serial port settings
*/
void llreset(int fd) {

  if (tcsetattr(fd, TCSANOW, &link_layer.oldtio) == -1) {
    printf("llreset: couldn't set port settings on port %s.\n", link_layer.port);
    exit(1);
  }
}
