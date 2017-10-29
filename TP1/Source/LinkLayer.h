#ifndef LINKLAYER_H
#define LINKLAYER_H

/*
Attempts to establish connection by sending or receiving SET / UA and returns the file descriptor for the connection
*/
int llopen(int port, serialStatus role);

/*
//TODO WIP only restores old settings for now
*/
int llclose(int fd);

/*
Attempts to send data_frame after byte stuffing it and appending a frame header
*/
int llwrite(int fd, uint8_t* data_frame, size_t length);

/*
Attempts to read a data_frame, byte destuffs it and validates it
*/
int llread(int fd, uint8_t* data_frame);

/*
Restores previous serial port settings
*/
void llreset(int fd);

#endif /* LINKLAYER_H */
