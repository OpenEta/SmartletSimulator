#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdlib.h>
#include <netdb.h>
#include <time.h>
#include <getopt.h>
#include <signal.h>
#include <math.h>

#include "mockdevice.h"
#include "util.h"

int num_clients;

struct sockaddr_in server_addr;
int client_tcp_sock;
int client_listen_sock;

struct ip_mreq mc_imreq;

char *progname;
bool daemonize;
int verbose;
int update_time;

/*
 * Display usages of the program
 */
void usage(int status)
{
    printf("\n\n");

    printf("\nUsage : %s [OPTION...]\n", progname);
    printf("\t[-s --server] [-h] [-v] [-d]\n\n");

    printf("-s, --server        \tSpecify the IP address of the server.\n");
    printf("                    \tRepresent the address in dotted decimal form (e.g. \"192.168.0.1\")\n");
    printf("-n, --num-clients   \tSpecify the number of Smartlet clients to simulate\n");
    printf("-t, --update-time   \tSpecify the time between power updates for the clients in milliseconds\n");
    printf("-v, --verbose       \tRun in verbose mode.\n");
    printf("-d, --daemon        \tRun the program as a daemon.\n");
    printf("-h, --help          \tDisplay this help and exit.\n\n\n");

    exit(status);
}

/*
 * Mock Smartlet command line  options
 */
static struct option longopts[] =
{
    { "help",               no_argument,            NULL, 'h'},
    { "daemon",             no_argument,            NULL, 'd'},

    { "verbose",            optional_argument,      NULL, 'v'},

    { "num-clients",        required_argument,      NULL, 'n'},
    { "server",             required_argument,      NULL, 's'},
    { "update-time",        required_argument,      NULL, 't'},

    // mark the end of the table
    { 0, 0, 0, 0 }
};

/*
 * Handles all command line arguments
 */
void handle_args(int argc, char *argv[], int config_pass)
{
    char *p;
    int ret = 0;

    // Preserve name for myself
    progname = ((p = strrchr (argv[0], '/')) ? ++p : argv[0]);

    // Check parameters
    if (argc < 1)
        usage(0);

    // Parse command line
    optind = 1;
    opterr = 1;
    if(config_pass) opterr = 0;
    while (ret != -1)
    {
        ret = getopt_long (argc, argv, "hds:n:t:v::", longopts, 0);

        // Pass through until a configure file option is found
        if (config_pass && ret !='f' && ret != 'h') continue;

        switch (ret)
        {
        case -1 :
            break;
        case 'h':
            usage(0);
            break;
        case 'd':
            daemonize = 1;
            break;
        case 'v':
            if(optarg)
                verbose = atoi(optarg);
            else
                ++verbose;
            break;
        case 'n':
            num_clients = atoi(optarg);
            if(num_clients > MAX_CLIENTS)
            {
                LOGERR("Number of desired clients is too large (%d), setting to maximum (%d)\n", num_clients, MAX_CLIENTS);
                num_clients = MAX_CLIENTS;
            }
            else
                LOGINF(2, "Number of clients set to %d\n", num_clients);
            break;
        case 't':
            update_time = atoi(optarg);
            LOGINF(2, "Update timer set to %d\n", update_time);
            break;
        case 's':
            if(!inet_pton(AF_INET, optarg, &(server_addr.sin_addr)))
            {
                LOGERR("Option '-s' (%s) does not represent a valid network address\n", optarg);
                usage(optopt);
            }
            break;
        case ':':
            printf("Option -%c requires an operand\n", optopt);
        case '?':	// unknown option
            usage(optopt);
            break;
        }
    }
}

/*
 * Initializes all variables
 */
void set_defaults()
{
    daemonize = 0;
    client_tcp_sock = -1;
    client_listen_sock = -1;
    update_time = 5000;

    bzero(&server_addr, sizeof(struct sockaddr_in));
}

/*
 * Handles the following signals: SIGINT, SIGTERM, SIGABRT
 * Safetly cleans up opens files and resets all system calls.
 */
void signal_handler(int param)
{
    LOGINF(2, "\nSmartlet Simulator is shutting down, cleaning up...\n");

    if(client_tcp_sock != -1) close(client_tcp_sock);
    if(client_listen_sock != -1) close(client_listen_sock);

    LOGINF(2, "Done!\n");
    exit(EXIT_SUCCESS);
}

int setup_listen_socket()
{
    int status;

    // set content of struct saddr and imreq to zero
    memset(&server_addr, 0, sizeof(struct sockaddr_in));
    memset(&mc_imreq, 0, sizeof(struct ip_mreq));

    // open a UDP socket
    client_listen_sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
    if ( client_listen_sock < 0 )
        printf("Error creating socket\n");

    server_addr.sin_family = PF_INET;
    server_addr.sin_port = htons(6585); // listen on port 4096
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); // bind socket to any interface
    status = bind(client_listen_sock, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_in));

    if ( status < 0 )
        printf("Error binding socket to interface\n");

    mc_imreq.imr_multiaddr.s_addr = inet_addr("224.0.0.120");
    mc_imreq.imr_interface.s_addr = INADDR_ANY; // use DEFAULT interface

    // JOIN multicast group on default interface
    status = setsockopt(client_listen_sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (const void *)&mc_imreq, sizeof(struct ip_mreq));

    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 100000;

    if (setsockopt(client_listen_sock, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) {
        printf("Error\n");
    }

    return 1;
}

int deliverMessage(char* in, int size, int i)
{
    int rc;
    rc = write(client_tcp_sock, in,size);//strlen(in)+1);

    if(rc <= 0)
    {
        LOGERRNO("write", "Unable to send packet to TCP server\n");
        return -1;
    }
    else
    {
        LOGINF(1, "Write success\n");
        return 1;
    }
}

int connectTCP()
{
    // Variable and structure definitions
    int rc;
    char *ip;
    struct hostent *hostp;

    ip = inet_ntoa(server_addr.sin_addr);

    // Get a socket descriptor
    client_tcp_sock = socket(AF_INET, SOCK_STREAM, 0);
    if(client_tcp_sock < 0)
    {
        LOGERRNO("socket", "Unable to create TCP socket to server %s\n", ip);
        return -1;
    }
    else
        LOGINF(1, "TCP socket created\n");

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SMARTLET_PORT);

    if(server_addr.sin_addr.s_addr == (unsigned long)INADDR_NONE)
    {
        LOGERR("Server IP address is invalid: %s\n", ip);
        return -1;
    }

    // Attempt to connect to server on TCP socket
    rc = connect(client_tcp_sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if(rc < 0)
    {
        LOGERRNO("connect", "Unable to connect to server\n");
        return -1;
    }
    else
        LOGINF(1, "Connection established...\n");

    return 1;
}

int decode_packet(unsigned char *packet, int length)
{
    int i;

    if(length < 11)
    {
        printf("Recevied odd length: %d\n", length);
        return -1;
    }

    char type = packet[0];
    uint16_t plength = (packet[1] << 8) + packet[2];
    uint32_t nodeID = (packet[3] << 24) + (packet[4] << 16) + (packet[5] << 8) + packet[6];
    uint16_t updateTimer = (packet[7] << 8) + packet[8];
    uint8_t topState = packet[9];
    uint8_t botState = packet[10];

    printf("type: %d\n", (int) type);
    printf("length: %d\n", (int) plength);
    printf("nodeID: %d\n", nodeID);
    printf("updateTimer: %d\n", updateTimer);
    printf("Top State: %d\n", (int) topState);
    printf("Bot State: %d\n", (int) botState);

    for(i = 0; i < length; ++i)
        printf("%.2X|", (uint8_t) packet[i]);
    printf("\n");
}

int obtain_server()
{
    int status;
    char buffer[MAXBUFSIZE];
    socklen_t sock_len = sizeof(struct sockaddr_in);

    while(1)
    {
        // receive from multicast channel
        status = recvfrom(client_listen_sock, buffer, MAXBUFSIZE, 0, (struct sockaddr *)&server_addr, &sock_len);

        if(status <= 0)
        {
            LOGINF(1, "Timeout waiting for broadcast packet\n");fflush(stdout);
        }
        else
        {
            char *ip = inet_ntoa(server_addr.sin_addr);

            LOGINF(1, "Got a broadcast packet, setting configuration\n");
            LOGINF(1, "%s\n", ip);fflush(stdout);

            break;
        }
    }
}

int attempt_recieve()
{
    char buffer[DEVICE_STATE_SIZE];
    int length;
    struct timeval start, end;
    long usec;
    double intpart;
    struct timeval timeout;

    timeout.tv_sec = update_time/1000;
    timeout.tv_usec = (update_time%1000) * 1000;

    gettimeofday(&start, NULL);
    while(1)
    {
        if (setsockopt (client_tcp_sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
        {
            LOGERRNO("setsockopt", "Failed to set timeout for recv socket\n");
            sleep(5);
        }

        length = recv(client_tcp_sock, buffer, 11, 0);
        if(length < 0)
        {
            if(errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            LOGERRNO("recv", "Error when attempt to receive on TCP socket\n");
        }
        else
        {
            decode_packet(buffer, length);
        }

        gettimeofday(&end, NULL);

        usec = (end.tv_sec - start.tv_sec)*1000000;
        usec += end.tv_usec - start.tv_usec;

        usec = (update_time*1000) - usec;

        if(usec < 0)
            break;

        timeout.tv_sec = (int) usec/1000000;
        timeout.tv_usec = (long) usec % 1000000;
    }
}

void send_data()
{
    int i;
    int rc;

    while(1)
    {
        for(i = 0; i < num_clients; ++i)
        {
            if(client_tcp_sock > 0)
            {
                int shift=0;
                char buffer[POWER_UPDATE_SIZE];
                struct Power_Update msg;

                msg.hdr.type=1;
                msg.hdr.len=htons(4);
                msg.hdr.nodeID = htonl(100 + i);

                memcpy(buffer+shift,&msg.hdr.type,1); shift+=1;
                memcpy(buffer+shift,&msg.hdr.len,2); shift+=2;
                memcpy(buffer+shift,&msg.hdr.nodeID,4); shift+=4;

                uint16_t power = (1000*i) + (rand() % 499);
                msg.top_power = htons(power);
                memcpy(buffer+shift,&msg.top_power,sizeof(uint16_t)); shift+=2;

                power = (1000*i) + (rand() % 499) + 500;
                msg.bot_power = htons(power);
                memcpy(buffer+shift,&msg.bot_power,sizeof(uint16_t));

                //send reading
                rc = deliverMessage(buffer, POWER_UPDATE_SIZE, i);
                if(rc < 0)
                    QUIT();
            }
        }

        attempt_recieve();
    }
}

int main(int argc, char **argv)
{
    int i;
    char buffer[MAXBUFSIZE];
    int done=0;

    set_defaults();
    handle_args(argc, argv, 0);
    srand(time(NULL));

    if(num_clients < 1)
        FATALERR("More than 1 node is required to run\n");

    // Capture exit signals to safetly close the program
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGABRT, signal_handler);

    // If there was no ip address set in the command line, listen for a broadcast from the server
    if(server_addr.sin_addr.s_addr == 0)
    {
        setup_listen_socket();
        obtain_server();
    }

    // Attempt to create a TCP connection to the server
    if(!connectTCP())
        QUIT();

    send_data();

    signal_handler(0);
}
