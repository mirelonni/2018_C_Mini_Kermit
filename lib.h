#ifndef LIB
#define LIB

typedef struct {
    int len;
    char payload[1400];
} msg;

typedef struct {
  char SOH;
  unsigned char LEN;
  char SEQ;
  char TYPE;
  char DATA[250];
  unsigned short CHECK;
  char MARK;
} kermit_pack;

#define JUST_DATA 7 //just the length of the data field
#define SEQ_MOD 64
#define TIMEOUT 5000
#define STD_EOL 14
#define PRINT_TAG 1 // change to print to stdout or not
#define SIZE 250
#define START 1

typedef struct {
  unsigned char MAXL;
  char TIME;
  char NPAD;
  char PADC;
  char EOL;
  char QCTL;
  char QBIN;
  char CHKT;
  char REPT;
  char CAPA;
  char R;
} s_data;

//makes a unsigned short from two unsigned chars
unsigned short make_crc(unsigned char crc1, unsigned char crc2) {
  return ((unsigned short)(crc1) << 8) + crc2;
}

void init(char* remote, int remote_port);
void set_local_port(int port);
void set_remote(char* ip, int port);
int send_message(const msg* m);
int recv_message(msg* r);
msg* receive_message_timeout(int timeout); //timeout in milliseconds
unsigned short crc16_ccitt(const void *buf, int len);

#endif
