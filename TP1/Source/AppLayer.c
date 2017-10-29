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
#include <math.h>

#include "Globals.h"
#include "AppLayer.h"
#include "LinkLayer.h"

static applicationLayer app_layer;

/*
Prints program usage
*/
void printUsage(char* program_name) {

  printf("\tUsage  : %s tx <port number [0,1]> <filename>\n", program_name);
  printf("\tUsage  : %s rx <port number [0,1]>\n", program_name);
  printf("\tExample: %s tx 0 penguin.gif\n", program_name);
  printf("\tExample: %s rx 1\n", program_name);
}

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

/*
Helper function for printing a data frame
*/
void printDataFrame(size_t size) {

  size_t i;
  for(i = 0; i < size; i++) {
    LOG_MSG("|0x%02X", app_layer.data_frame[i]);
  }

  LOG_MSG("|\n");
}

/*
Helper function for printing a data frame as ASCII
*/
void printDataFrameASCII(size_t size) {

  size_t i;
  for(i = 0; i < size; i++) {
    LOG_MSG("|%c", app_layer.data_frame[i]);
  }

  LOG_MSG("|\n");
}

/*
Creates a start/end data frame for initiating or ending a file transfer, returns the size of the frame
*/
int createSEPacket(uint8_t control) {

  size_t index = 0;
  app_layer.data_frame[0] = control;

  /* 1st parameter, filesize */
  app_layer.data_frame[1] = T_SIZE;

  int filesize_length = (int) ((ceil(log10(app_layer.filesize)) + 1) * sizeof(char)); //TODO function?
  app_layer.data_frame[2] = filesize_length;

  char filesize[FILESIZE_MAX_DIGITS];
  snprintf(filesize, filesize_length, "%d", app_layer.filesize);

  size_t i;
  for(i = 0; i < filesize_length; i++) {

    app_layer.data_frame[3 + i] = filesize[i];
  }

  index = i + 3;

  /* 2nd parameter, filename */
  app_layer.data_frame[index] = T_NAME;
  app_layer.data_frame[index + 1] = strlen(app_layer.filename) + 1;

  for(i = 0; i < app_layer.data_frame[index + 1]; i++) {

    app_layer.data_frame[index + 2 + i] = app_layer.filename[i];
  }

  return index + 2 + i;
}

/*
Resets the data frame in the appLayer struct
*/
void resetDataFrame() {

  bzero(&app_layer.data_frame, sizeof(app_layer.data_frame));
}

/*
Opens specified file and creates data packets for sending the file
*/
int alsend(int port, char* filename) {

  strcpy(app_layer.filename, filename);

  FILE* fd = fopen(app_layer.filename, "rb");

  if(fd == NULL) {
    printf("%s not found!\n", app_layer.filename);
    exit(1);
  }

  app_layer.serial_fd = llopen(port, TRANSMIT);

  if(app_layer.serial_fd < 0) {
    printf("alsend: couldn't open serial port for communication.\n");
    exit(1);
  }

  printf("alsend: connection established!\n");

  app_layer.filesize = getFilesize(fd);

  LOG_MSG("Filesize: %d\n", app_layer.filesize);

  if(app_layer.filesize <= 0) {
    printf("alsend: file has size 0 or exceeds 2 147 483 647 bytes (~2.14GiB).\n");
    llreset(app_layer.serial_fd);
    exit(1);
  }

  resetDataFrame();
  int packet_size = createSEPacket(C_START);

  printDataFrame(packet_size);

  //TODO llwrite





  llclose(app_layer.serial_fd); //TODO DISC / UA -> error check

  close(app_layer.serial_fd);
  fclose(fd);
  return 0;
}

/*
Attempts to receive a file and writes it to disk
*/
int alreceive(int port) {

  app_layer.serial_fd = llopen(port, RECEIVE);

  if(app_layer.serial_fd < 0) {
    printf("alreceive: couldn't open serial port for communication.\n");
    exit(1);
  }

  printf("alreceive: connection established!\n");

  //TODO llread





  llclose(app_layer.serial_fd);

  close(app_layer.serial_fd);
  return 0;
}

/*
Get the filesize of a file with up to INT_MAX bytes
*/
int getFilesize(FILE* fd) {

  fseek(fd, 0, SEEK_END);
  int filesize = ftell(fd);
  fseek(fd, 0, SEEK_SET);

  if(filesize == INT_MAX) return -1;
  return filesize;
}

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
  } else if((strcmp("tx", argv[1]) == 0) && argc < 4) {
    printf("%s: missing filename after port number!\n", argv[0]);
    printUsage(argv[0]);
    exit(1);
  }

  if((strcmp("tx", argv[1]) == 0) && strlen(argv[3]) + 1 > FILENAME_SIZE) {
    printf("Filename too long, can only be %d chars long.\n", FILENAME_SIZE - 1);
    exit(1);
  }

  memset(&app_layer, 0, sizeof(app_layer));

  if(strcmp("tx", argv[1]) == 0) alsend(parseULong(argv[2], 10), argv[3]);
  if(strcmp("rx", argv[1]) == 0) alreceive(parseULong(argv[2], 10));

  return 0;
}
