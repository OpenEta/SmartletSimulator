
#ifndef MOCKDEVICE_
#define MOCKDEVICE_

#define MAXBUFSIZE 65536 // Max UDP Packet size is 64 Kbyte

#define MAX_CLIENTS 10
#define SMARTLET_PORT 9865

#define POWER_UPDATE_SIZE 11
#define DEVICE_STATE_SIZE 11

struct Header
{
    uint32_t nodeID;
    uint16_t len;
    uint8_t type;
};

struct Power_Update
{
    struct Header hdr;
    uint16_t top_power;
    uint16_t bot_power;
};

struct Device_State
{
    struct Header hdr;
    uint16_t timeout;
    uint8_t state;
};

void usage(int status);

void handle_args(int argc, char *argv[], int config_pass);

void set_defaults();

void signal_handler(int param);

int setup_listen_socket();

int deliverMessage(char* in, int size, int i);

int connectTCP();

int decode_packet(unsigned char *packet, int length);

int obtain_server();

void send_data();

int attempt_recieve();

#endif
