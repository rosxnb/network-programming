#include <netdb.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <string.h>
#include <stdio.h>

int main()
{
    printf("Configuring remote address ...\n");
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_DGRAM;

    struct addrinfo* remoteAddr;
    if( getaddrinfo("localhost", "8080", &hints, &remoteAddr) )
    {
        fprintf(stderr, "getaddrinfo() failed. errno: %d\n", errno);
        return 1;
    }

    printf("Remote addresss is: ");
    char addressBuffer[100], serviceBuffer[100];
    getnameinfo(remoteAddr->ai_addr, remoteAddr->ai_addrlen, addressBuffer, sizeof(addressBuffer), serviceBuffer, sizeof(serviceBuffer), NI_NUMERICHOST | NI_NUMERICSERV);
    printf("%s  %s\n", addressBuffer, serviceBuffer);

    int socketPeer = socket(remoteAddr->ai_family, remoteAddr->ai_socktype, remoteAddr->ai_protocol);
    if( socketPeer < 0 )
    {
        fprintf(stderr, "socket() failed. errno: %d\n", errno);
        return 1;
    }

    const char* message = "Hello says UDP socket peer";
    printf("Sending data: %s\n", message);
    int bytesSent = sendto(socketPeer, message, strlen(message), 0, remoteAddr->ai_addr, remoteAddr->ai_addrlen);
    printf("Sent %d of %lu data\n", bytesSent, strlen(message));

    freeaddrinfo(remoteAddr);
    close(socketPeer);
    printf("Finished.\n");

    return 0;
}
