#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BUF_SIZE 1024

int main(int argc, char *argv[]) 
{
    int sockfd;
    struct sockaddr_in srv_addr;
    char buf[BUF_SIZE];

    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <server_ip> <Port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *srv_ip = argv[1];
    int srv_port = atoi(argv[2]);

    
    //UDP Socket erstellen
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) 
    {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    //Server-Adresse konfigurieren
    memset(&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(srv_port);
    
    if (inet_pton(AF_INET, srv_ip, &srv_addr.sin_addr) <= 0) 
    {
        perror("Invalid address/ Address not supported");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    
    printf("Client started. Connected to %s:%d\n", srv_ip, srv_port);
    printf("Type your message and press Enter to send. Type 'exit' to quit.\n\n");

    while (1)
    {
        printf("> ");
        memset(buf, 0, BUF_SIZE);

        if (fgets(buf, BUF_SIZE, stdin) != NULL)    
        {
            if (strncmp(buf, "exit", 4) == 0)   
            {
                printf("Exiting...\n");
                break;
            }

            int bytes_sent = sendto(sockfd, buf, strlen(buf), 0, (const struct sockaddr *)&srv_addr, sizeof(srv_addr));

            if (bytes_sent < 0) 
            {
                perror("sendto failed");
                break;
            }
        } 
    }
    
    close(sockfd);
    return 0;
}