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
#include "ClientTCP.h"
#include "GetIP.h"

FTPReply_t ReadFTPLine(int sockfd) {

  char receiveBuff[1024];
  char receive[1];
  int n;
  size_t counter = 0;
  int state = 0;

  while((n = read(sockfd, receive, 1)) > 0) {

    if(receive[0] == '\r') state = 1;
    else if(receive[0] == '\n' && state == 1) state = 2;

    receiveBuff[counter] = receive[0];
    counter++;
    if(counter >= sizeof(receiveBuff) - 1);

    if(state == 2) {
      receiveBuff[counter] = '\0';
      printf("%s", receiveBuff);
      if(isdigit(receiveBuff[0]) && isdigit(receiveBuff[1]) && isdigit(receiveBuff[2]) && receiveBuff[3] == ' ') return REPLY_OVER;
      else return VALID_REPLY;
    }
  }
    // printf("%s\n", receiveBuff);

    // if()
    // if(receiveBuff[n - 2] == '\r' && receiveBuff[n - 1] == '\n')
    // return VALID_REPLY;
    // else return NON_TERMINATED;
}


  // char receiveBuff[1024];
  // int n;
  //
  // n = read(sockfd, receiveBuff, sizeof(receiveBuff) - 1);
  //
  // receiveBuff[n] = '\0';
  // printf("%s\n", receiveBuff);
  //
  // if(receiveBuff[n - 2] == '\r' && receiveBuff[n - 1] == '\n')
  // return VALID_REPLY;
  // else return NON_TERMINATED;


int main(int argc, char *argv[]) {

  struct hostent* host;
  host = getAddress(argv[1]);

  if(host == NULL) {
    printf("download: Couldn't get info on host provided, exiting...\n");
    exit(1);
  }

  printf("Host name  : %s\n", host->h_name);
  printf("IP Address : %s\n",inet_ntoa(*((struct in_addr *)host->h_addr)));

  int	sockfd;
  struct	sockaddr_in server_addr;
  // char	buf[] = "USER anonymous\r\n";
  // char receiveBuff[1024];
  // int	n;

  /*server address handling*/
  bzero((char*)&server_addr,sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = inet_addr(inet_ntoa(*((struct in_addr *)host->h_addr)));	/*32 bit Internet address network byte ordered*/
  server_addr.sin_port = htons(FTP_PORT);		/*server TCP port must be network byte ordered */

  /*open an TCP socket*/
  if ((sockfd = socket(AF_INET,SOCK_STREAM,0)) < 0) {
    perror("socket()");
    exit(0);
  }
  /*connect to the server*/
  if(connect(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
    perror("connect()");
    exit(0);
  }

  FTPReply_t reply;
  do {
    reply = ReadFTPLine(sockfd);
  } while(reply != REPLY_OVER);


  // n = read(sockfd, receiveBuff, sizeof(receiveBuff) - 1);
  // receiveBuff[n] = 0; //Null terminate answer
  // printf("reply: %s\n", receiveBuff);
  //
  // n = write(sockfd, buf, strlen(buf));

  // if(ReadFTPLine(sockfd) == NON_TERMINATED) printf("warning\n");
  //
  // printf("Wrote %d bytes\n", n);
  //
  // n = read(sockfd, receiveBuff, sizeof(receiveBuff) - 1);
  // receiveBuff[n] = 0; //Null terminate answer
  // printf("reply: %s\n", receiveBuff);

  close(sockfd);

  return 0;
}
