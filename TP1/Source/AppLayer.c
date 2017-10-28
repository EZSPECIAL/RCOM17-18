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
	
	//TODO add message about establishing connection
	
	int filesize = get_filesize(fd);
	
	LOG_MSG("Filesize: %d\n", filesize);
	
	if(filesize <= 0) {
		printf("alsend: file has size 0 or exceeds 2 147 483 647 bytes (~2.14GiB).\n");
		exit(1); //TODO call close
	}
	
	llclose(app_layer.serial_fd);
	
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
	
	//TODO add message about establishing connection
	
	llclose(app_layer.serial_fd);
	
	close(app_layer.serial_fd);
	return 0;
}

/*
  Get the filesize of a file with up to INT_MAX bytes
*/
int get_filesize(FILE* fd) {
	
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

  if(strcmp("tx", argv[1]) == 0) alsend(parseULong(argv[2], 10), argv[3]);
  if(strcmp("rx", argv[1]) == 0) alreceive(parseULong(argv[2], 10));
  
  return 0;
}
