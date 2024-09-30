#include <netdb.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <string.h>
#include <stdio.h>

int main()
{
    printf("Configuring local address... \n");
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_family   = AF_INET;
    hints.ai_flags    = AI_PASSIVE;

    struct addrinfo* localAddress;
    if( getaddrinfo(0, "8080", &hints, &localAddress) )
    {
        fprintf(stderr, "getaddrinfo() failed. errno: %d\n", errno);
        return 1;
    }

    printf("Creating socket ...\n");
    int socketListen = socket(localAddress->ai_family, localAddress->ai_socktype, localAddress->ai_protocol);
    if( socketListen < 0 )
    {
        fprintf(stderr, "socket() failed. errno: %d\n", errno);
        return 1;
    }

    printf("Binding socket to local address ...\n");
    if( bind(socketListen, localAddress->ai_addr, localAddress->ai_addrlen) )
    {
        fprintf(stderr, "bind() failed. errno: %d\n", errno);
        return 1;
    }
    freeaddrinfo(localAddress);

    struct sockaddr_storage clientAddress;
    socklen_t clientLength = sizeof(clientAddress);
    char read[1024];
    int bytesReceived = recvfrom(socketListen, read, 1024, 0, (struct sockaddr*) &clientAddress, &clientLength);

    printf("Connected with remote address: ");
    char addressBuffer[100], serviceBuffer[100];
    getnameinfo((struct sockaddr*) &clientAddress, clientLength, addressBuffer, sizeof(addressBuffer), serviceBuffer, sizeof(serviceBuffer), NI_NUMERICHOST | NI_NUMERICSERV);
    printf("%s  %s\n", addressBuffer, serviceBuffer);
    printf("Received %d bytes: %.*s\n", bytesReceived, bytesReceived, read);

    close(socketListen);

    return 0;
}
