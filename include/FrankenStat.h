// The serial will be send with each package that is transmitted to the
// Bit. As an extra check to verify we're receiving the right data.
#define BITSERIAL 62634
#define BURNING 1
#define IDLE 0

// Data set, which will be send over the 433MHz link to the BC.
typedef struct bit_ {
  uint16_t serial;
  bool burn;
} bit_t;

typedef struct schedule_ {
  unsigned char active: 1;
  unsigned char hour: 5;
  unsigned char quarters: 2;
  unsigned char temp;
} schedule_t;
