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

#endif /* LINKLAYER_H */
