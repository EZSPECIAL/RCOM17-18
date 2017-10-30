#ifndef GLOBALS_H
#define GLOBALS_H

/*
  Useful macros
*/

#define ENABLE_DEBUG 0

#if ENABLE_DEBUG
#define LOG_MSG printf
#else
#define LOG_MSG(...)
#endif

#define BIT(n)     (0x01 << n)
#define BCC1(a, b) (a ^ b)
#define FALSE       0
#define TRUE        1
#define ABS(x)     ((x) < 0 ? -(x) : (x))

/*
  Application layer constants
*/

#define PROGRESS_BAR_SIZE    50

#define PORT_NAME_SIZE       10
#define FILESIZE_MAX_DIGITS  11
#define FILENAME_SIZE        256
#define MAX_START_FRAME      272																		                                    /* Maximum number of bytes for a start/end data frame, allows for 256 bytes filenames */
#define PACKET_SIZE          128
#define DATA_FRAME_SIZE      ((PACKET_SIZE + 4 < MAX_START_FRAME) ? MAX_START_FRAME : PACKET_SIZE + 4)  /* Picks the highest value a data frame will ever possibly need */

#define C_NS(n)              BIT(6) * n
#define C_DATA               0x01
#define C_START              0x02
#define C_END                0x03

#define T_SIZE               0
#define T_NAME               1

#define DCONTROL_INDEX       0

/*
  Link layer constants
*/

#define FLAG           0x7E                     /* Start / End frame flag */
#define ESCAPE_CHAR    0x7D                     /* Escape character for byte stuffing */
#define XOCTET         0x20                     /* Octet to xor with for byte stuffing */

#define A_TX           0x03                     /* Address field when sending commands as emitter or responses as receiver */
#define A_RX           0x01                     /* Address field when sending commands as receiver or responses as emitter */

#define C_SET          0x03                     /* Set up command */
#define C_DISC         0x0B                     /* Disconnect command */
#define C_UA           0x07                     /* Unnumbered acknowledgment response */
#define C_RR(n)        (0x05 | BIT(7 * n))      /* Receiver ready response */
#define C_REJ(n)       (0x01 | BIT(7 * n))      /* Reject response */

#define SU_FRAME_SIZE  5                        /* Expected supervision frame size */
#define ADDRESS_INDEX  1					              /* Frame header address index */
#define CONTROL_INDEX  2                        /* Frame header control index */
#define BCC1_INDEX     3                        /* Frame header BCC index */
#define HEADER_SIZE    5                        /* Header size without BCC2 */

#define BAUDRATE       B38400
#define TIMEOUT        3                        /* Number of retries on transmission */
#define FRAME_MAX_SIZE DATA_FRAME_SIZE * 2 + 6  /* Maximum size for a frame, usually the maximum I frame possible */
#define TIMEOUT_S      3                        /* Number of seconds to wait per try */
#define CLOSE_WAIT     1                        /* Number of seconds to wait before closing serial port */

/*
  Enumerators
*/

typedef enum serialStatus {TRANSMIT, RECEIVE} serialStatus;                         /* Current role of the program */
typedef enum serialState {INIT, S_FLAG, E_FLAG} serialState;                        /* Link layer state machine states */
typedef enum serialEvent {FLAG_E, TIMEOUT_E} serialEvent;                           /* Link layer state machine events */
typedef enum serialError {NO_ERR, BCC1_ERR, CONTROL_ERR, TIMEOUT_ERR, FLAG_ERR} serialError;  /* Link layer possible errors */

/*
  Struct containing application layer data
*/

typedef struct applicationLayer {
  int serial_fd;
  serialStatus status;
  uint16_t tx_counter;
  int filesize;
  char filename[FILENAME_SIZE];
  uint8_t data_frame[DATA_FRAME_SIZE];
} applicationLayer;

/*
  Struct containing link layer data
*/

typedef struct linkLayer {
  char port[PORT_NAME_SIZE];
  uint32_t baudrate;
  uint8_t sequence_number;
  uint32_t length;
  volatile uint8_t timeout_count;
  volatile uint8_t timeout_flag;
  uint16_t current_index;
  uint8_t last_response;
  serialState state;
  struct termios oldtio;
  struct termios newtio;
  uint8_t frame[FRAME_MAX_SIZE];
} linkLayer;

#endif /* GLOBALS_H */
