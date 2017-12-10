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

static char last_message[FTP_MSG_SIZE];
static char file_path[] = "./test";

FTPFile_t file_info;

int main(int argc, char *argv[]) {

//ftp://[<user>:<password>@]<host>/<url-path>

	/* Check program parameters */
	if(argc != 2) {
		printf("download: Wrong number of arguments.\n");
		printf("download: usage: ./download ftp://[<user>:<password>@]<host>/<url-path>\n");
		exit(1);
	}

	
	

	/* Get host IP address */
	struct hostent* host;
	host = getAddress(argv[1]);

	if(host == NULL) {
		printf("download: Couldn't get info on host provided.\n");
		exit(1);
	}

	printf("Host Name  : %s\n", host->h_name);
	printf("IP Address : %s\n", inet_ntoa(*((struct in_addr*)host->h_addr)));

	/* Get TCP socket to host */
	int sockfd = getTCPSocket(inet_addr(inet_ntoa(*((struct in_addr*)host->h_addr))), FTP_PORT);

	if(sockfd < 0) FTPAbort(sockfd, "download: Failure connecting to FTP server control port.\n");

	/* Login to FTP server */
	FTPLogin(sockfd, "USER anonymous\r\n", "PASS ei11056@fe.up.pt\r\n");

	/* Use BINARY mode */
	if(FTPCommand(sockfd, "TYPE I\r\n", FALSE) != 0) FTPAbort(sockfd, "download: TYPE command received 5XX code.\n");
	
	/* Enter passive mode and get data port */
	uint16_t data_port = FTPPassive(sockfd);
	file_info.datafd = getTCPSocket(inet_addr(inet_ntoa(*((struct in_addr*)host->h_addr))), data_port);
	
	if(file_info.datafd < 0) FTPAbort(sockfd, "download: Failure connecting to FTP server data port.\n");
	
	/* Send command to download file and download it */
	FTPCommand(sockfd, "RETR /1MB.zip\r\n", TRUE);
	

	
	
	close(file_info.datafd);
	close(sockfd);
	
	return 0;
}

void FTPDownload() {
	
	FILE* fd = fopen(file_path, "wb");
	
	char file_data[FTP_MSG_SIZE];
	int n;
	
	while((n = read(file_info.datafd, file_data, FTP_MSG_SIZE)) > 0) {
		
		fwrite(file_data, 1, n, fd);
	}
	
	fclose(fd);
}

/**
 * Gets numeric representation of data port from FTP message containing it
 *
 * @param message FTP message containing data port
 * @return data port on success, -1 otherwise
 */
int32_t getDataPort(char* message) {
	
	size_t msg_i = 0;
	size_t number_i = 0;
	size_t comma_count = 0;
	char msb[6];
	char lsb[6];
	uint8_t success_f = 0;
	
	/* Process message up to null terminator */
	while(message[msg_i] != '\0') {
		
		/* Count commas up to data port values */
		if(message[msg_i] == ',') comma_count++;
		msg_i++;
		
		/* Next 2 number values will be the data port */
		if(comma_count >= 4) {
			
			/* Read MSB of data port */
			while(isdigit(message[msg_i])) {
				
				msb[number_i] = message[msg_i];
				number_i++;
				msg_i++;
			}
			
			msb[number_i] = '\0';
			number_i = 0;
			msg_i++;
			
			/* Read LSB of data port */
			while(isdigit(message[msg_i])) {
				
				lsb[number_i] = message[msg_i];
				number_i++;
				msg_i++;
			}
		
			lsb[number_i] = '\0';
			success_f = 1;
			
			break;
		}
	}
	
	if(!success_f) return -1;

	unsigned long msb_value = strtoul(msb, NULL, 10);
	unsigned long lsb_value = strtoul(lsb, NULL, 10);
	
	return (msb_value * 256 + lsb_value);
}

/**
 * Sends PASV command and parses the data port received
 *
 * @param sockfd TCP socket file descriptor connected to FTP port
 * @return data port to use
 */
uint16_t FTPPassive(int sockfd) {

	if(FTPCommand(sockfd, "PASV\r\n", FALSE) != 0) FTPAbort(sockfd, "download: PASV command received 5XX code.\n");
	
	int32_t data_port = getDataPort(last_message);
	
	if(data_port < 0) FTPAbort(sockfd, "download: data port could not be parsed.\n");
	
	return data_port;
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
 * Sends a command through the TCP connection, if command triggers download calls appropriate function
 *
 * @param sockfd TCP socket file descriptor connected to FTP port
 * @param command FTP command to send
 * @param download_f whether command will trigger a download from FTP server
 * @result 0 on success
 */
int8_t FTPCommand(int sockfd, char* command, uint8_t download_f) {
	
	FTPReplyState_t reply_state;
	
	/* Repeat until reply code signals completion */
	do {
	
		/* Write command and read response */
		write(sockfd, command, strlen(command));
		reply_state = handleFTPReplyCode(readFTPReply(sockfd));

		/* Handle reply code */
		if(reply_state == ABORT) return -1;
		else if(reply_state == WAIT_REPLY) {
			
			if(download_f) FTPDownload();
			
			reply_state = handleFTPReplyCode(readFTPReply(sockfd));
			if(reply_state == ABORT) return -1;
		}
		
	} while(reply_state != CONTINUE);
	
	return 0;
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
	
	FTPReplyState_t reply_state;
	
	/* Read message of the day */
	reply_state = handleFTPReplyCode(readFTPReply(sockfd));
	
	if(reply_state != CONTINUE) FTPAbort(sockfd, "download: Server is not ready.\n");
	
	/* Send USER command */
	if(FTPCommand(sockfd, user, FALSE) != 0) FTPAbort(sockfd, "download: USER command received 5XX code.\n");
	
	/* Send PASS command */
	if(FTPCommand(sockfd, password, FALSE) != 0) FTPAbort(sockfd, "download: PASS command received 5XX code.\n");

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
	char message[FTP_MSG_SIZE];
	
	do {
		status = readFTPLine(sockfd, message, sizeof(message));
		printf("%s", message);
	} while(status != REPLY_OVER);
	
	strcpy(last_message, message);
	
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
