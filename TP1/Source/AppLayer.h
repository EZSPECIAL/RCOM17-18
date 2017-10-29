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
int getFilesize(FILE* fd);

/*
Creates a start/end data frame for initiating or ending a file transfer
*/
int createSEPacket(uint8_t control);

/*
Creates a data packet with data read from file, returns the size of the frame
*/
int createDataPacket(FILE* fd);

/*
Helper function for printing a data frame
*/
void printDataFrame(size_t size);

/*
Extract filesize and filename from data_frame
*/
int extractFileInfo(uint8_t* data_frame, size_t length);

/*
Resets the data frame in the appLayer struct
*/
void resetDataFrame();

#endif /* APPLAYER_H */
