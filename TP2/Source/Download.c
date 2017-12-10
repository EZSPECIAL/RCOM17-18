#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <ctype.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include "Download.h"

int main(int argc, char *argv[]) {

	/* Get host IP address */
	struct hostent* host;
	host = getAddress(argv[1]);

	if(host == NULL) {
		printf("download: Couldn't get info on host provided, exiting...\n");
		exit(1);
	}

	printf("Host Name  : %s\n", host->h_name);
	printf("IP Address : %s\n", inet_ntoa(*((struct in_addr*)host->h_addr)));

	/* Get TCP socket to host */
	int sockfd = getTCPSocket(inet_addr(inet_ntoa(*((struct in_addr*)host->h_addr))), FTP_PORT);

	if(sockfd == -1) FTPAbort(sockfd, "download: Failure connecting to server, exiting...\n");

	/* FTP */
	FTPLogin(sockfd, "USER anonymous\r\n", "PASS ei11056@fe.up.pt\r\n");
	
	close(sockfd);
	
	return 0;
}

/**
 * Aborts program by closing the TCP socket and printing error message before exiting
 *
 * @param sockfd TCP socket file descriptor connected to FTP port
 * @param message error message to print
 */
void FTPAbort(int sockfd, char* message) {
	
	printf("%s", message);
	close(sockfd);
	exit(1);
}

/**
 * Handles FTP reply code and returns action to take
 *
 * @param reply_code numeric value of the FTP reply code
 * @return enumerator specifying action caller should take
 */
FTPReplyState_t handleFTPReplyCode(uint16_t reply_code) {
	
	uint16_t hundreds = (reply_code / 100) % 100;
	
	switch(hundreds) {
		
		case R1XX:
			return WAIT_REPLY;
		break;
		case R2XX:
			return CONTINUE;
		break;
		case R3XX:
			return CONTINUE;
		break;
		case R4XX:
			return REPEAT;
		break;
		case R5XX:
			return ABORT;
		break;
		default:
			return ABORT;
		break;
	}
}

/**
 * Performs FTP login command sequence
 *
 * @param sockfd TCP socket file descriptor connected to FTP port
 * @param user FTP USER command ready to send
 * @param password FTP PASS command ready to send
 * @return non 0 on failure
 */
int8_t FTPLogin(int sockfd, char* user, char* password) {
	
	uint16_t reply_code;
	
	/* Read message of the day */
	reply_code = readFTPReply(sockfd);
	handleFTPReplyCode(reply_code);
	
	write(sockfd, user, strlen(user));
	
	reply_code = readFTPReply(sockfd);
	handleFTPReplyCode(reply_code);
	
	write(sockfd, password, strlen(password));
	
	reply_code = readFTPReply(sockfd);
	handleFTPReplyCode(reply_code);
	
	return 0;
}

/**
 * Reads full FTP message and parses the reply code
 *
 * @param sockfd TCP socket file descriptor connected to FTP port
 * @return FTP reply code
 */
uint16_t readFTPReply(int sockfd) {
	
	FTPLineType_t status;
	char message[512];
	
	do {
		status = readFTPLine(sockfd, message, sizeof(message));
		printf("%s", message);
	} while(status != REPLY_OVER);
	
	return parseFTPReplyCode(message);
}

/**
 * Parses final line of FTP reply message for reply code
 *
 * @param reply reply message text
 * @return FTP reply code
 */
uint16_t parseFTPReplyCode(char* reply) {
	
	/* Copy first 3 values of reply message which should be the reply code */
	char code[4];
	strncpy(code, reply, 3);
	code[3] = '\0';
	
	unsigned long number = strtoul(code, NULL, 10);

	return number;
}

/**
 * Reads a single FTP reply message line, waits for Telnet end-of-line
 *
 * @param sockfd TCP socket file descriptor connected to FTP port
 * @param reply reply message text filled in by the function
 * @param reply_size buffer size for reply message text
 * @return state of the reply message (finished, multiline, buffer overflow, non terminated reply)
 */
FTPLineType_t readFTPLine(int sockfd, char* reply, size_t reply_size) {

	FTPLineState_t state = WAIT_CR;
	char receive[1];
	size_t counter = 0;

	/* Read 1 byte at a time until Telnet end-of-line (CRLF) */
	while(read(sockfd, receive, 1) > 0) {

		/* State machine */
		if(receive[0] == '\r') state = WAIT_LF;
		else if(receive[0] == '\n' && state == WAIT_LF) state = OVER;

		reply[counter] = receive[0];
		counter++;

		/* Check if reply is larger than buffer */
		if(counter >= reply_size - 1) {
			reply[counter] = '\0';
			return REPLY_TOO_LONG;
		}

		/* Check if reply message is over by looking for "xyz " string at the start */
		if(state == OVER) {
			
			reply[counter] = '\0';
			
			if(isdigit(reply[0]) && isdigit(reply[1]) && isdigit(reply[2]) && reply[3] == ' ') return REPLY_OVER;
			else return MULTILINE;
		}
	}

	return NON_TERMINATED;
}

/**
 * Gets a TCP socket to specified address at specified port
 *
 * @param address 32 bit Internet address network byte ordered
 * @param port port number
 * @return TCP socket file descriptor
 */
int getTCPSocket(in_addr_t address, uint16_t port) {
	
	int	sockfd;
	struct sockaddr_in server_addr;

	/* Server address handling */
	bzero((char*) &server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = address;
	server_addr.sin_port = htons(port);

	/* Open TCP socket */
	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket()");
		return -1;
	}
	
	/* Connect to the server */
	if(connect(sockfd, (struct sockaddr*) &server_addr, sizeof(server_addr)) < 0) {
		perror("connect()");
		return -1;
	}
	
	return sockfd;
}

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
