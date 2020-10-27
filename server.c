#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <pthread.h>
#include <string.h>
#include <math.h>

#define _XOPEN_SOURCE 700
#define port1 51717
#define port2 51718

#define sizeofpacket 60000
#define server_hostname "192.168.162.140"

struct packet
{
    int seq_no;
    char data[sizeofpacket];
};

struct nak
{
    int lost_seq_no;
};

int packet_count, fast_seq_no, lost_packets = 0;
char **file_data;
pthread_mutex_t mutex;
int lost_count = 0;

void *data_server(void *temp);
void *nak_server(void *temp);
void *create_file();
void *write_file();
void *lostCountCalculator();

void error(char *msg)
{
    perror(msg);
    exit(0);
}

int main(int argc, char **argv)
{
    pthread_t serverThread1, serverThread2;
    printf("Server running \n");
    create_file();
    pthread_create(&serverThread1, NULL, data_server, NULL);
    pthread_create(&serverThread2, NULL, nak_server, NULL);
    pthread_join(serverThread2, NULL);
    pthread_join(serverThread1, NULL);
    lostCountCalculator();
    printf("Lost Count after NAK : %d\n", lost_count);
    if (lost_count == 0) {
        write_file();
    }
}

// counts the number of packets lost
void *lostCountCalculator()
{
    int temp_count = 0;
    for (int i = 0; i < packet_count; i++) {
        if (strcmp(file_data[i], "EMPTY") == 0) {
            temp_count += 1;
        }
    }
    lost_count = temp_count;
}

// creates a data structure to receive file data
void *create_file()
{
    packet_count = pow(2, 30) / sizeofpacket;
    file_data = malloc(sizeof(char *) * packet_count);
    for (int i = 0; i < packet_count; i++) {
        file_data[i] = malloc(sizeof(char) * sizeofpacket);
        memcpy(file_data[i], "EMPTY", 5);
    }
}

// stores the file contents received in a local file
void *write_file()
{
    int fd;
    fd = open("received_Data.bin", O_CREAT | O_WRONLY | O_TRUNC);
    for (int i = 0; i < packet_count; i++) {
        write(fd, file_data[i], sizeofpacket);
    }
    close(fd);
}

// receives the file chunks
void *data_server(void *temp)
{
    struct hostent *hostent;
    in_addr_t in_addr;
    struct sockaddr_in sockaddr_in;
    struct timeval end;
    struct timeval tv;
    tv.tv_sec = 10;
    char buffer[1024];
    int sockfd, enable = 1;
    int read_return, temp_1 = 0;
    struct packet *temp_packet = malloc(sizeof(struct packet));

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    if (sockfd < 0) {
        error("error: opening socket");
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0) {
        error("error: setsockopt(SO_REUSEADDR) failed");
    }

    sockaddr_in.sin_addr.s_addr = htonl(INADDR_ANY);
    sockaddr_in.sin_family = AF_INET;
    sockaddr_in.sin_port = htons(port1);
    socklen_t fromlen = sizeof(struct sockaddr_in);

    if (bind(sockfd, (struct sockaddr *)&sockaddr_in, sizeof(sockaddr_in)) == -1) {
        error("error: binding socket");
    }

    read_return = recvfrom(sockfd, temp_packet, sizeofpacket, 0, (struct sockaddr *)&sockaddr_in, &fromlen);
    printf("Connected to client \n");

    // start receiving the file data
    while (read_return) {
        fast_seq_no = temp_packet->seq_no;
        memcpy(file_data[temp_packet->seq_no], temp_packet->data, sizeofpacket);
        read_return = recvfrom(sockfd, temp_packet, sizeofpacket, 0, (struct sockaddr *)&sockaddr_in, &fromlen);

        if (temp_packet->seq_no - fast_seq_no != 1) {
            lost_count = lost_count + (temp_packet->seq_no - fast_seq_no);
        }
    }

    // capture the end time
    gettimeofday(&end, NULL);
    printf("start timer \n");
    
    for (int j = 0; j < 10; j++) {
        sendto(sockfd, &end, sizeof(struct timeval), 0, (struct sockaddr *)&sockaddr_in, fromlen);
    }

    
    for (int k = 0; k < 10; k++) {
        sendto(sockfd, temp_packet, 0, 0, (struct sockaddr *)&sockaddr_in, fromlen);
    }

    printf("File received\n");
    pthread_exit(&sockfd);
}

// sends ACKs and receives lost packets
void *nak_server(void *temp)
{
    struct hostent *hostent;
    in_addr_t in_addr;
    struct sockaddr_in sockaddr_in;
    struct timeval end;
    int sockfd, enable = 1;
    int read_return, temp_1 = 0;
    char buffer[1024];
    struct timeval tv;
    tv.tv_sec = 10;
    struct nak *lost = malloc(sizeof(struct nak));
    struct packet *temp_packet = malloc(sizeof(struct packet));

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    if (sockfd < 0) {
        error("error: opening socket");
    }

    hostent = gethostbyname(server_hostname);

    if (hostent == NULL) {
        error("error: failed to get server_hostname\n");
    }

    in_addr = inet_addr(inet_ntoa(*(struct in_addr *)*(hostent->h_addr_list)));

    if (in_addr == (in_addr_t)-1) {
        error("error: failed to get inet address\n");
    }

    sockaddr_in.sin_addr.s_addr = in_addr;
    sockaddr_in.sin_family = AF_INET;
    sockaddr_in.sin_port = htons(port2);
    int fromlen = sizeof(sockaddr_in);
    int slow_seq_no = 0;
    int once = 0;
    int rv;
    fd_set readfds;

    printf("Connected to nak_client\n");

    while (1){
        while ((slow_seq_no < fast_seq_no && slow_seq_no != packet_count)) {
            if (strcmp(file_data[slow_seq_no], "EMPTY") == 0) {
                printf("Lost packet : %d\n", slow_seq_no);
                lost->lost_seq_no = slow_seq_no;

                for (int i = 0; i < 10; i++) {
                    sendto(sockfd, lost, sizeof(struct nak), 0, (struct sockaddr *)&sockaddr_in, fromlen);
                }

            recv_again:
                FD_ZERO(&readfds);
                FD_SET(sockfd, &readfds);

                tv.tv_sec = 2.5;
                tv.tv_usec = 0;

                rv = select(sockfd + 1, &readfds, NULL, NULL, &tv);

                if (rv == 1) {
                    read_return = recvfrom(sockfd, temp_packet, sizeof(struct packet), 0, (struct sockaddr *)&sockaddr_in, &fromlen);
                    if (temp_packet->seq_no == lost->lost_seq_no) {
                        memcpy(file_data[temp_packet->seq_no], temp_packet->data, sizeofpacket);
                        lost_count--;
                        printf("Received packet : %d\n", temp_packet->seq_no);
                    } else {
                        goto recv_again;
                    }
                } else {
                    goto term;
                }
            }
            slow_seq_no += 1;
        }

        if (slow_seq_no == packet_count - 1) {
        term:
            printf("nak packets sent and data received\n");
            for (int i = 0; i < 5; i++) {
                sendto(sockfd, lost, 0, 0, (struct sockaddr *)&sockaddr_in, fromlen);
            }
            break;
        }
    }
    pthread_exit(&sockfd);
}