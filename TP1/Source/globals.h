#ifndef GLOBALS_H
#define GLOBALS_H

/*
  Useful macros
*/

#define BIT(n)     (0x01 << n)
#define BCC1(a, b) (a ^ b)

/*
  Application layer constants
*/

#define PORT_NAME_SIZE 10

/*
  Link layer constants
*/

#define FLAG           0x7E                 /* Start / End frame flag */

#define A_TX           0x03                 /* Address field when sending commands as emitter or responses as receiver */
#define A_RX           0x01                 /* Address field when sending commands as receiver or responses as emitter */

#define C_SET          0x03                 /* Set up command */
#define C_DISC         0x0B                 /* Disconnect command */
#define C_UA           0x07                 /* Unnumbered acknowledgment response */
#define C_RR(n)        (0x05 | BIT(7 * n))  /* Receiver ready response */
#define C_REJ(n)       (0x01 | BIT(7 * n))  /* Reject response */

#define SU_FRAME_SIZE  5                    /* Expected supervision frame size */
#define BAUDRATE       38400
#define TIMEOUT        3                    /* Number of retries on transmission */
#define FRAME_MAX_SIZE 512
/*
  Enumerators
*/

typedef enum serialStatus {TRANSMIT, RECEIVE} serialStatus; /* Current role of the program */

/*
  Struct containing application layer data
*/

typedef struct applicationLayer {
  int fd;
  serialStatus status;
} applicationLayer;

/*
  Struct containing link layer data
*/

typedef struct linkLayer {
  char port[PORT_NAME_SIZE];
  uint32_t baudrate;
  uint8_t sequence_number;
  uint8_t timeout;
  uint8_t tx_counter;
  struct termios oldtio;
  struct termios newtio;
  uint8_t frame[FRAME_MAX_SIZE];
} linkLayer;

#endif /* GLOBALS_H */
