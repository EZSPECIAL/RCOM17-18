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

    printf("\tUsage  : %s <tx | rx> <port number [0,1]>\n", program_name);
    printf("\tExample: %s tx 0\n", program_name);
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

  if(strcmp("tx", argv[1]) == 0) app_layer.fd = llopen(parseULong(argv[2], 10), TRANSMIT);
  if(strcmp("rx", argv[1]) == 0) app_layer.fd = llopen(parseULong(argv[2], 10), RECEIVE);

  if(app_layer.fd < 0) exit(1); //TODO add error message
  //TODO add message about establishing connection

  llclose(app_layer.fd);

  close(app_layer.fd);
  return 0;
}
