// The serial will be send with each package that is transmitted to the
// Bit. As an extra check to verify we're receiving the right data.
#define BITSERIAL 2343436;
#define BURNING 1
#define IDLE 0

// Data set, which will be send over the 433MHz link to the BC.
typedef struct bit_ {
  unsigned int serial;
  bool burn;
} bit;
