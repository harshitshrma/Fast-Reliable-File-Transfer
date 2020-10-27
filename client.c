#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/time.h>
#include <pthread.h>
#include <string.h>
#include <math.h>

#define _XOPEN_SOURCE 700
#define port1 51717
#define port2 51718
#define packet_size 60000
#define packet_count 17896
#define server_hostname "192.168.162.140"

struct packet
{
    int sequence_no;
    char data[packet_size];
};

struct nak
{
    int lost_sequence_no;
};

char file_data[packet_count][packet_size];

void *data_client(void *temp);
void *nak_client(void *temp);
void *openingFile();

void error(char *msg)
{
    perror(msg);
    exit(0);
}

int main(int argc, char **argv)
{
    pthread_t clientThread1, clientThread2;
    openingFile();
    pthread_create(&clientThread1, NULL, data_client, NULL);
    pthread_create(&clientThread2, NULL, nak_client, NULL);
    pthread_join(clientThread2, NULL);
    pthread_join(clientThread1, NULL);
}

// reads file in the form of chunks
void *openingFile()
{
    int i = 0, bytes_read;
    int bytes;

    FILE *fd = fopen("data.bin", "rb");

    while (i < packet_count && fgets(file_data[i], sizeof(file_data[0]), fd)) {
        file_data[i][strlen(file_data[i]) - 1] = '\0';
        i++;
    }
    fclose(fd);
}

// sends data
void *data_client(void *temp)
{
    int sockfd;
    struct hostent *hostent;
    in_addr_t in_addr;
    struct sockaddr_in sockaddr_in;
    struct stat *file_size;
    char buffer[1024];
    struct timeval start, end;
    struct packet *temp_packet = malloc(sizeof(struct packet));
    double t_trans, throughput;

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
    sockaddr_in.sin_port = htons(port1);
    printf("Connected to server \n");

    // start the timer to check the time taken to send 1bg of data
    gettimeofday(&start, NULL);
    printf("start timer\n");
    // send data
    for (int i = 0; i < packet_count; i = i + 1) {
        temp_packet->sequence_no = i;
        memcpy(temp_packet->data, file_data[i], packet_size);
        sendto(sockfd, temp_packet, sizeof(struct packet), 0, (struct sockaddr *)&sockaddr_in, sizeof(sockaddr_in));
        usleep(1);
    }

    printf("send temp data \n");
    for (int j = 0; j < 10; j++) {
        sendto(sockfd, temp_packet, 0, 0, (struct sockaddr *)&sockaddr_in, sizeof(sockaddr_in));
    }
    
    // get the time when all data was received from server
    int read_end;
    int fromlen = sizeof(sockaddr_in);

    do {
        read_end = recvfrom(sockfd, &end, sizeof(struct timeval), 0, (struct sockaddr *)&sockaddr_in, &fromlen);
    } while (read_end);
    printf("received data \n");
    // stop timer and calculate the throughput
    t_trans = (end.tv_sec * pow(10, 6) - start.tv_sec * pow(10, 6)) + (end.tv_usec - start.tv_usec);
    stat("data.bin", file_size);
    throughput = pow(2, 30) / t_trans;
    printf("Throughput : %f\n", throughput);
    pthread_exit(&sockfd);
}

// receives ACKs and retransmits lost packets
void *nak_client(void *temp)
{
    int sockfd, enable = 1;
    struct hostent *hostent;
    in_addr_t in_addr;
    struct sockaddr_in sockaddr_in;
    struct stat *file_size;
    char buffer[1024];
    struct timeval start, end;
    struct packet *temp_packet = malloc(sizeof(struct packet));
    double t_trans, throughput;
    int read_return, rv;
    fd_set readfds;
    struct timeval tv;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        error("error: opening socket");
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0) {
        error("error; setsockopt(SO_REUSEADDR) failed");
    }

    sockaddr_in.sin_addr.s_addr = htonl(INADDR_ANY);
    sockaddr_in.sin_family = AF_INET;
    sockaddr_in.sin_port = htons(port2);
    socklen_t fromlen = sizeof(struct sockaddr_in);
    if (bind(sockfd, (struct sockaddr *)&sockaddr_in, sizeof(sockaddr_in)) == -1) {
        error("error: binding socket");
    }
    printf("nak client connected\n");
    struct nak *lost = malloc(sizeof(struct nak));
    int q = 0;
    read_return = recvfrom(sockfd, lost, sizeof(struct nak), 0, (struct sockaddr *)&sockaddr_in, &fromlen);

    while (read_return)
    {
        temp_packet->sequence_no = lost->lost_sequence_no;
        memcpy(temp_packet->data, file_data[temp_packet->sequence_no], packet_size);

        for (int i = 0; i < 2; i++) {
            sendto(sockfd, temp_packet, sizeof(struct packet), 0, (struct sockaddr *)&sockaddr_in, sizeof(sockaddr_in));
        }
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);

        tv.tv_sec = 2.5;
        tv.tv_usec = 0;

        rv = select(sockfd + 1, &readfds, NULL, NULL, &tv);
        if (rv == 1) {
            read_return = recvfrom(sockfd, lost, sizeof(struct nak), 0, (struct sockaddr *)&sockaddr_in, &fromlen);
        } else {
            break;
        }
    }
    printf("Connection to server from nak client ended \n");
    pthread_exit(&sockfd);
}
