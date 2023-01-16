#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

void error(const char *msg)
{
    perror(msg);
    exit(0);
}

int main(int argc, char *argv[])

{
    int sockfd, portno, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    char buffer[256];
    if (argc < 3)
    {
        fprintf(stderr, "usage %s hostname port\n", argv[0]);
        exit(0);
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");

    server = gethostbyname(argv[1]);

    if (server == NULL)
    {
        fprintf(stderr, "ERROR, no such host\n");
        exit(0);
    }

    portno = atoi(argv[2]);
    serv_addr.sin_family = AF_INET;
    inet_addr(argv[2], &serv_addr.sin_addr.s_addr);
    // serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    n = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));

    if (n == 0)
    {
        fprintf(stderr, "Connect successful!\n");
    }
    else
    {
        fprintf(stderr, "Could not connect to address!\n");
        close(sockfd);
        exit(1);
    }
    // Send client's name
    bzero(buffer, 256);
    n = read(sockfd, buffer, 255);
    fprintf(stderr, "%s", buffer);
    fgets(buffer, 255, stdin);

    buffer[strcspn(buffer, "\r\n")] = 0;
    n = write(sockfd, buffer, strlen(buffer));
    if (n < 0)
    {
        error("ERROR writing to socket");
    }

    while (1)
    {
        bzero(buffer, 256);
        n = read(sockfd, buffer, 255);
        fprintf(stderr, "%s", buffer);
        if (n < 0)
        {
            error("ERROR reading from socket");
        }
        if (n == 0)
        {
            printf("Server closed the connection.\n");
            exit(1);
        }

        // Send answer to server
        fprintf(stderr, "\nAnswer: ");
        bzero(buffer, 256);
        fgets(buffer, 255, stdin);
        buffer[strcspn(buffer, "\r\n")] = 0;
        n = write(sockfd, buffer, strlen(buffer));
        if (n < 0)
        {
            error("ERROR writing to socket");
        }
    }

    close(sockfd);
    return 0;
}
