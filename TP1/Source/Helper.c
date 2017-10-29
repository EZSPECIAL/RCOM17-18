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

/*
Helper function for printing a generic array in hex format
*/
void printArrayAsHex(uint8_t* buffer, size_t size) {

  size_t i;
  for(i = 0; i < size; i++) {
    LOG_MSG("|0x%02X", buffer[i]);
  }

  LOG_MSG("|\n");
}
