#ifndef LINKLAYER_H
#define LINKLAYER_H

/*
Attempts to establish connection by sending or receiving SET / UA and returns the file descriptor for the connection
*/
int llopen(int port, serialStatus role);

/*
Attempts to close connection by sending DISC, receiving DISC and sending UA, returns whether it succeeded
*/
int llclose_transmit(int fd);

/*
Attempts to close connection by receiving DISC, sending DISC and waiting on UA, returns whether it succeeded
*/
int llclose_receive(int fd);

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
