#include <netdb.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>

int main()
{
    printf("Configuring local address ...\n");
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_family   = AF_INET;
    hints.ai_flags    = AI_PASSIVE;

    struct addrinfo* localAddr;
    if( getaddrinfo(0, "8087", &hints, &localAddr) )
    {
        fprintf(stderr, "getaddrinfo() failed. errno: %d\n", errno);
        return 1;
    }

    printf("Creating socket ...\n");
    int socketListen = socket(localAddr->ai_family, localAddr->ai_socktype, localAddr->ai_protocol);
    if( socketListen < 0 )
    {
        fprintf(stderr, "socket() failed. errno: %d", errno);
        return 1;
    }

    printf("Binding socket to local address ...\n");
    if( bind(socketListen, localAddr->ai_addr, localAddr->ai_addrlen) )
    {
        fprintf(stderr, "bind() failed. errno: %d", errno);
        return 1;
    }
    freeaddrinfo(localAddr);

    fd_set master;
    FD_ZERO(&master);
    FD_SET(socketListen, &master);
    int maxSocket = socketListen;
    printf("Waiting for connections ...\n");

    while( 1 )
    {
        fd_set reads = master;
        if( select(maxSocket+1, &reads, 0, 0, 0) < 0 )
        {
            fprintf(stderr, "select() failed. errno: %d", errno);
            return 1;
        }

        if( !FD_ISSET(socketListen, &reads) ) continue;

        struct sockaddr_storage clientAddr;
        socklen_t clientLen = sizeof(clientAddr);

        char read[1024];
        int bytesReceived = recvfrom(socketListen, read, 1024, 0, (struct sockaddr*) &clientAddr, &clientLen);
        if( bytesReceived  < 1 )
        {
            fprintf(stderr, "connection closed by client. errno: %d", errno);
            return 1;
        }

        char hostname[100];
        getnameinfo((struct sockaddr*) &clientAddr, 0, hostname, sizeof(hostname), 0, 0, NI_NUMERICHOST);
        printf("Client Address: %s\n", hostname);
        printf("Received data: %.*s\n", bytesReceived, read);
        for( int i = 0; i < bytesReceived; ++i ) read[i] = toupper(read[i]);
        sendto(socketListen, read, bytesReceived, 0, (struct sockaddr*) &clientAddr, clientLen);
    }

    printf("Closing listening socket ...\n");
    close(socketListen);

    return 0;
}
