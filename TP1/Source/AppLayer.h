#ifndef APPLAYER_H
#define APPLAYER_H

/*
  Prints program usage
*/
void printUsage(char* program_name);

/*
  Parses a string and converts it to unsigned long in specified base
*/
unsigned long parseULong(char* str, int base);

/*
  Opens specified file and creates data packets for sending the file
*/
int alsend(int port, char* filename);

/*
  Attempts to receive a file and writes it to disk
*/
int alreceive(int port);

/*
  Get the filesize of a file with up to INT_MAX bytes
*/
int get_filesize(FILE* fd);

#endif /* APPLAYER_H */
