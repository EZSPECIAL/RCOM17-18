#ifndef __DOWNLOAD_H
#define __DOWNLOAD_H

/******************************************************
                       MACROS
 ******************************************************/

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE  1
#endif
 
#define FTP_PORT	 21																//Default FTP port
#define FTP_MSG_SIZE 512															//FTP Message buffer size
#define R1XX		 1																//FTP Reply code hundreds digit
#define R2XX		 2																//FTP Reply code hundreds digit
#define R3XX		 3																//FTP Reply code hundreds digit
#define R4XX		 4																//FTP Reply code hundreds digit
#define R5XX		 5	 															//FTP Reply code hundreds digit

/******************************************************
                     ENUMERATORS
 ******************************************************/

typedef enum {MULTILINE, NON_TERMINATED, REPLY_OVER, REPLY_TOO_LONG} FTPLineType_t;	//FTP Reply types
typedef enum {WAIT_CR, WAIT_LF, OVER} FTPLineState_t;								//FTP Line states
typedef enum {WAIT_REPLY, CONTINUE, REPEAT, ABORT} FTPReplyState_t;					//FTP Reply code states

/******************************************************
                       STRUCTS
 ******************************************************/

//Stores data socket descriptor and file path needed for download
typedef struct {
	
	char* file_path;
	int datafd;
} FTPFile_t;
 
/******************************************************
                      FUNCTIONS
 ******************************************************/

/**
 * Gets the hostname and IPv4 address of a given host
 *
 * @param address host string
 * @return pointer to hostent struct that stores host info
 */
struct hostent* getAddress(char* address);

/**
 * Gets a TCP socket to specified address at specified port
 *
 * @param address 32 bit Internet address network byte ordered
 * @param port port number
 * @return TCP socket file descriptor
 */
int getTCPSocket(in_addr_t address, uint16_t port);

/**
 * Gets a TCP socket to specified address at specified port
 *
 * @param address 32 bit Internet address network byte ordered
 * @param port port number
 * @return TCP socket file descriptor
 */
FTPLineType_t readFTPLine(int sockfd, char* reply, size_t reply_size);

/**
 * Parses final line of FTP reply message for reply code
 *
 * @param reply reply message text
 * @return FTP reply code
 */
uint16_t parseFTPReplyCode(char* reply);

/**
 * Aborts program by closing the TCP socket and printing error message before exiting
 *
 * @param sockfd TCP socket file descriptor connected to FTP port
 * @param message error message to print
 */
void FTPAbort(int sockfd, char* message);

/**
 * Handles FTP reply code and returns action to take
 *
 * @param reply_code numeric value of the FTP reply code
 * @return enumerator specifying action caller should take
 */
FTPReplyState_t handleFTPReplyCode(uint16_t reply_code);

/**
 * Sends a command through the TCP connection, if command triggers download calls appropriate function
 *
 * @param sockfd TCP socket file descriptor connected to FTP port
 * @param command FTP command to send
 * @param download_f whether command will trigger a download from FTP server
 * @result 0 on success
 */
int8_t FTPCommand(int sockfd, char* command, uint8_t download_f);

/**
 * Performs FTP login command sequence
 *
 * @param sockfd TCP socket file descriptor connected to FTP port
 * @param user FTP USER command ready to send
 * @param password FTP PASS command ready to send
 * @return non 0 on failure
 */
int8_t FTPLogin(int sockfd, char* user, char* password);

/**
 * Reads full FTP message and parses the reply code
 *
 * @param sockfd TCP socket file descriptor connected to FTP port
 * @return FTP reply code
 */
uint16_t readFTPReply(int sockfd);

/**
 * Gets numeric representation of data port from FTP message containing it
 *
 * @param message FTP message containing data port
 * @return data port on success, -1 otherwise
 */
int32_t getDataPort(char* message);

/**
 * Sends PASV command and parses the data port received
 *
 * @param sockfd TCP socket file descriptor connected to FTP port
 * @return data port to use
 */
uint16_t FTPPassive(int sockfd);


void FTPDownload();

#endif /*__DOWNLOAD_H */