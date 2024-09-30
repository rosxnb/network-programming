#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <string.h>
#include <stdio.h>

int main()
{
    printf("Configuring local address ...\n");
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    struct addrinfo* bindAddress;
    if( getaddrinfo(0, "8080", &hints, &bindAddress) )
    {
        fprintf(stderr, "getaddrinfo() failed to get local address. errno: %d\n", errno);
        return 1;
    }

    printf("Creating socket ...\n");
    int socketListen = socket(bindAddress->ai_family, bindAddress->ai_socktype, bindAddress->ai_protocol);
    if( socketListen < 0 )
    {
        fprintf(stderr, "socket() failed to create new socket for local address. errno %d\n", errno);
        return 1;
    }

    printf("Binding socket to local address ... \n");
    if( bind(socketListen, bindAddress->ai_addr, bindAddress->ai_addrlen) )
    {
        fprintf(stderr, "bind() failed. errno: %d\n", errno);
        return 1;
    }
    freeaddrinfo(bindAddress);

    printf("Socket Listening ... \n");
    if( listen(socketListen, 10) < 0 )
    {
        fprintf(stderr, "listen() failed. errno: %d\n", errno);
        return 1;
    }

    fd_set master;
    FD_ZERO(&master);
    FD_SET(socketListen, &master);
    int maxSocket = socketListen;

    printf("Waiting for new connections ... \n");

    while( 1 )
    {
        fd_set reads;
        reads = master;
        if( select(maxSocket+1, &reads, 0, 0, 0) < 0 )
        {
            fprintf(stderr, "select() failed. errno: %d\n", errno);
            return 1;
        }

        for( int currentSocket = 1; currentSocket <= maxSocket; ++currentSocket )
        {
            if( FD_ISSET(currentSocket, &reads) )
            {
                if( currentSocket == socketListen )
                {
                    struct sockaddr_storage clientAddress;
                    socklen_t clientLen = sizeof(clientAddress);
                    int socketClient = accept(socketListen, (struct sockaddr*) &clientAddress, &clientLen);

                    if( socketClient < 0 )
                    {
                        fprintf(stderr, "accept() failed. errno: %d\n", errno);
                        return 1;
                    }

                    FD_SET(socketClient, &master);
                    if( socketClient > maxSocket) maxSocket = socketClient;

                    char addressBuffer[100];
                    getnameinfo((struct sockaddr*) &clientAddress, clientLen, addressBuffer, sizeof(addressBuffer), 0, 0, NI_NUMERICHOST);
                    printf("New connection from: %s\n", addressBuffer);
                }
                else
                {
                    char read[4096];
                    int bytesReceived = recv(currentSocket, read, 4096, 0);
                    if( bytesReceived < 1 )
                    {
                        FD_CLR(currentSocket, &master);
                        close(currentSocket);
                        continue;
                    }

                    for( int i = 0; i < bytesReceived; ++i)
                        read[i] = toupper(read[i]);

                    send(currentSocket, read, bytesReceived, 0);
                }
            }
        }
    }

    printf("Closing listening socket ...\n");
    close(socketListen);
    printf("Finished.\n");

    return 0;
}
