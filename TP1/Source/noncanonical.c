/*Non-Canonical Input Processing*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#define BAUDRATE B38400
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

#define FLAG 0x7e
#define A 0x03
#define CUA 0x07
#define CSET 0x03

#define INIT_S 0
#define ADDRESS_S 1
#define CONTROL_S 2
#define BCC1_S 3
#define END_F 4
#define END_S 5

#define ENABLE_DEBUG 1
#if ENABLE_DEBUG
	#define LOG_MSG printf
#else
	#define LOG_MSG(...)
#endif


volatile int STOP=FALSE;

int main(int argc, char** argv)
{
    int fd,c, res;
    struct termios oldtio,newtio;
    uint8_t buf[255];
    uint8_t receive[1], state = INIT_S;
    int nread = 0;

    if ( (argc < 2) || 
  	     ((strcmp("/dev/ttyS0", argv[1])!=0) && 
  	      (strcmp("/dev/ttyS1", argv[1])!=0) )) {
      printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
      exit(1);
    }


  /*
    Open serial port device for reading and writing and not as controlling tty
    because we don't want to get killed if linenoise sends CTRL-C.
  */
  
    
    fd = open(argv[1], O_RDWR | O_NOCTTY );
    if (fd <0) {perror(argv[1]); exit(-1); }

    if ( tcgetattr(fd,&oldtio) == -1) { /* save current port settings */
      perror("tcgetattr");
      exit(-1);
    }

    bzero(&newtio, sizeof(newtio));
    bzero(&buf, sizeof(buf));	
    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    /* set input mode (non-canonical, no echo,...) */
    newtio.c_lflag = 0;

    newtio.c_cc[VTIME]    = 0;   /* inter-character timer unused */
    newtio.c_cc[VMIN]     = 1;   /* blocking read until 5 chars received */



  /* 
    VTIME e VMIN devem ser alterados de forma a proteger com um temporizador a 
    leitura do(s) próximo(s) caracter(es)
  */



    tcflush(fd, TCIOFLUSH);

    if ( tcsetattr(fd,TCSANOW,&newtio) == -1) {
      perror("tcsetattr");
      exit(-1);
    }

    printf("New termios structure set\n");


    while (STOP==FALSE) {
	res = read(fd, receive, 1);
	buf[nread] = receive[0];
	uint8_t BCC = A^CSET;
	LOG_MSG("before 0x%02X\n", state);
	  switch(state){
		case INIT_S:
			if(buf[nread] == FLAG) state = ADDRESS_S;
			else state = INIT_S;
			break;
		case ADDRESS_S:
			if (buf[nread] == A) state = CONTROL_S;
			else state = INIT_S;
			break;
		case CONTROL_S:
			if (buf[nread] == CSET) state = BCC1_S;
			else state = INIT_S;
			break;
		case BCC1_S:
			if (buf[nread] == BCC) state = END_F;
			else state = INIT_S;
			break;
		case END_F:
			if (buf[nread] == FLAG) state = END_S;
			else state = INIT_S;
			break;
		default:
			state = INIT_S;
			break;
	}
		
	nread += res;
	LOG_MSG("after 0x%02X\n", state);
	if (state == END_S) break;

     
    }
	int i;
	for (i = 0; i < nread; i++){
		printf("0x%02X\n", buf[i]);
	}
	    uint8_t super_frame[5];
	    super_frame[0] = FLAG;
	    super_frame[1] = A;     //Address Field
	    super_frame[2] = CUA;   //Control Field
	    super_frame[3] = A^CUA; //BCC
	    super_frame[4] = FLAG;


	    i = 0;
	    for(i; i < sizeof(super_frame); i++) {
	      printf("0x%02X\n", super_frame[i]);
	    }

	    res = write(fd, super_frame, sizeof(super_frame));
	    printf("%d bytes written\n", res);
	    sleep(3);


     
 



    tcsetattr(fd,TCSANOW,&oldtio);
    close(fd);
    return 0;
}
