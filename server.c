#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>

#define PORT 6767
#define BUF_SIZE 1024

int main() 
{
    int sockfd;
    struct sockaddr_in srv_addr, cl_addr;
    socklen_t cl_len = sizeof(cl_addr);
    char buf[BUF_SIZE];

    //UDP Socket erstellen
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) 
    {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    //Server-Adresse konfigurieren
    memset(&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_addr.s_addr = INADDR_ANY;
    srv_addr.sin_port = htons(PORT);

    //Socket an Port binden
    if (bind(sockfd, (const struct sockaddr *)&srv_addr, sizeof(srv_addr)) < 0) 
    {
        perror("bind failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Server started on port %d\n", PORT);
    printf("CRTL+C to stop the server\n\n");

    //Main loop
    while (1)
    {
        //Checklist für Sockets
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);

        //kurzer Timeout für schnelle Antworten
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 10000;

        //select(), zum schauen ob Pakete in Warteschlage sind
        int activity = select(sockfd + 1, &readfds, NULL, NULL, &tv);

        if (activity < 0) {
            perror("select error");
            break;
        }

        //Wenn > 0 dann ist Paket da
        if (activity > 0 && FD_ISSET(sockfd, &readfds))
        {
            //Paket annehmen
            int n = recvfrom(sockfd, (char *)buf, BUF_SIZE - 1, 0, (struct sockaddr *)&cl_addr, &cl_len);

            if (n > 0) 
            {
                buf[n] = '\0'; //String terminieren

                //Ausgabe
                printf("[Packet from %s:%d] %s\n", inet_ntoa(cl_addr.sin_addr), ntohs(cl_addr.sin_port), buf);
            }
        } 
        
        //TODO: GAME LOGIK
    }

    close(sockfd);
    return 0;
}