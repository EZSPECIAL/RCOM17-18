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

static char last_message[FTP_MSG_SIZE]; //Most recent message received
static FTPFile_t file_info;				//Information about data socket and local file path
static FTPArgument_t argument_info;		//Information needed for FTP handling

int main(int argc, char* argv[]) {

	/* Check program parameter count */
	if(argc != 2) {
		
		printf("download: Wrong number of arguments.\n");
		printf("download: usage: ./download ftp://[<user>:<password>@]<host>/<url-path>\n");
		exit(1);
	}

	/* Parse argument */
	if(parseArgument(argv[1]) != 0) {
		
		printf("download: Invalid argument.\n");
		printf("download: usage: ./download ftp://[<user>:<password>@]<host>/<url-path>\n");
		exit(1);
	}
	
	/* Default values for anonymous user */
	if(argument_info.anonymous_f == TRUE) {
		
		strncpy(argument_info.user, "USER ", strlen("USER "));
		strcpy(argument_info.user + strlen("USER "), "anonymous\r\n");
		
		strncpy(argument_info.password, "PASS ", strlen("PASS "));
		strcpy(argument_info.password + strlen("PASS "), "ei11056@fe.up.pt\r\n");
	}
	
	/* Get host IP address */
	struct hostent* host;
	host = getAddress(argument_info.host);

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
	FTPLogin(sockfd, argument_info.user, argument_info.password);

	/* Use BINARY mode */
	if(FTPCommand(sockfd, "TYPE I\r\n", FALSE) != 0) FTPAbort(sockfd, "download: TYPE command received 5XX code.\n");
	
	/* Enter passive mode and get data port */
	uint16_t data_port = FTPPassive(sockfd);
	file_info.datafd = getTCPSocket(inet_addr(inet_ntoa(*((struct in_addr*)host->h_addr))), data_port);
	
	if(file_info.datafd < 0) FTPAbort(sockfd, "download: Failure connecting to FTP server data port.\n");
	
	/* Send command to download file and download it */
	if(FTPCommand(sockfd, argument_info.path, TRUE) != 0) FTPAbort(sockfd, "download: RETR command received 5XX code.\n");
	
	close(file_info.datafd);
	close(sockfd);
	
	return 0;
}

/**
 * Parses path to file to get the file name, copies it to FTPFile_t struct
 *
 * @param path file path
 */
void parseFilePath(char* path) {
	
	size_t path_i = 0;
	size_t last_slash_i = 0;
	
	while(path[path_i] != '\0') {
		
		if(path[path_i] == '/') {
			last_slash_i = path_i;
		}
		
		path_i++;
	}
	
	file_info.path[0] = '.';
	strcpy(file_info.path + 1, path + last_slash_i);
}

/**
 * Parses command line argument and fills FTPArgument_t struct
 *
 * @param argument command line argument received
 * @return 0 on success
 */
int8_t parseArgument(char* argument) {
	
	/* Check that start of argument is as expected */
	if(strncmp(argument, "ftp://", FTP_USER_START) != 0) {
		printf("download: Could not find \"ftp://\" in argument.\n");
		return -1;
	}
	
	size_t arg_i = 0;
	size_t param_i = 0;
	uint8_t at_f = FALSE;
	uint8_t separator_f = FALSE;
	
	/* Search for @ symbol in argument */
	while(argument[arg_i] != '\0') {
		
		if(argument[arg_i] == '@') {
			at_f = TRUE;
			break;
		}
		
		arg_i++;
	}
	
	arg_i = FTP_USER_START;
	argument_info.anonymous_f = TRUE;
	
	/* Parse user and password if @ found */
	if(at_f) {
		
		argument_info.anonymous_f = FALSE;
		
		/* Parse user */
		strncpy(argument_info.user, "USER ", strlen("USER "));
		param_i += strlen("USER ");
		
		while(argument[arg_i] != '\0') {
			
			if(argument[arg_i] != ':') {
				argument_info.user[param_i] = argument[arg_i];
				arg_i++;
				param_i++;
			} else {
				arg_i++;
				separator_f = TRUE;
				break;
			}
		}
		
		if(!separator_f) {
			printf("download: Could not find \":\" separator in argument.\n");
			return -1;
		} else if(param_i <= strlen("USER ")) {
			printf("download: Could not find user in argument.\n");
			return -1;
		}
		
		strcpy(argument_info.user + param_i, "\r\n");
		separator_f = FALSE;
		param_i = 0;
		
		/* Parse password */
		strncpy(argument_info.password, "PASS ", strlen("PASS "));
		param_i += strlen("PASS ");
		
		while(argument[arg_i] != '\0') {
			
			if(argument[arg_i] != '@') {
				argument_info.password[param_i] = argument[arg_i];
				arg_i++;
				param_i++;
			} else {
				arg_i++;
				separator_f = TRUE;
				break;
			}
		}
		
		if(!separator_f) {
			printf("download: Could not find \"@\" separator in argument.\n");
			return -1;
		} else if(param_i <= strlen("PASS ")) {
			printf("download: Could not find password in argument.\n");
			return -1;
		}
		
		strcpy(argument_info.password + param_i, "\r\n");
		separator_f = FALSE;
		param_i = 0;
	}
	
	/* Parse host name */
	while(argument[arg_i] != '\0') {

		if(argument[arg_i] != '/') {
			argument_info.host[param_i] = argument[arg_i];
			arg_i++;
			param_i++;
		} else {
			separator_f = TRUE;
			break;
		}
	}

	if(!separator_f) {
		printf("download: Could not find \"/\" separator in argument for host.\n");
		return -1;
	} else if(param_i <= 0) {
		printf("download: Could not find host in argument.\n");
		return -1;
	}
	
	argument_info.host[param_i] = '\0';
	separator_f = FALSE;
	param_i = 0;
	
	/* Parse file path */
	strncpy(argument_info.path, "RETR ", strlen("RETR "));
	param_i += strlen("RETR ");
	
	while(argument[arg_i] != '\0') {

		argument_info.path[param_i] = argument[arg_i];
		arg_i++;
		param_i++;
	}

	if(param_i <= 0) {
		printf("download: Could not find file path in argument.\n");
		return -1;
	}

	argument_info.path[param_i] = '\0';
	
	/* Extract file name from path */
	parseFilePath(argument_info.path);
	
	strcpy(argument_info.path + param_i, "\r\n");
	
	return 0;
}

/**
 * Uses FTP data socket connection to download file requested
 */
void FTPDownload() {
	
	/* Open file with file name parsed previously */
	FILE* fd = fopen(file_info.path, "wb");
	
	char file_data[FTP_MSG_SIZE];
	int n;
	
	/* Read until no more data is returned */
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
	uint8_t success_f = FALSE;
	
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
			success_f = TRUE;
			
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
