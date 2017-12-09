#ifndef __GETIP_H
#define __GETIP_H

/**
 * Gets the hostname and IPv4 address of a given host
 *
 * @param address host string
 * @return pointer to hostent struct that stores host info
 */
struct hostent* getAddress(char* address);

#endif /* __GETIP_H */
