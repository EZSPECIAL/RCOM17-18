#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/**
 * Gets the hostname and IPv4 address of a given host
 *
 * @param address host string
 * @return pointer to hostent struct that stores host info
 */
struct hostent* getAddress(char* address) {

	struct hostent* host;

	if((host = gethostbyname(address)) == NULL) {
		herror("download");
		return NULL;
	}

	return host;
}
