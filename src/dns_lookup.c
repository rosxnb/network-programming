#include <netdb.h>
#include <sys/errno.h>
#include <string.h>
#include <stdio.h>

int main(int argc, char * argv[])
{
    if (argc < 2)
    {
        perror("Usage:\f dns_lookup <hostname>\n");
        return 1;
    }

    printf("Resolving hostname: %s\n", argv[1]);
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_ALL;

    struct addrinfo * retAddr;
    if( getaddrinfo(argv[1], 0, &hints, &retAddr) )
    {
        fprintf(stderr, "getaddrinfo() failed. errno: %d\n", errno);
        return 1;
    }

    printf("Remote address(es) are: \n");
    struct addrinfo * peerAddr = retAddr;
    while( peerAddr )
    {
        char hostaddr[100];
        getnameinfo(peerAddr->ai_addr, peerAddr->ai_addrlen, hostaddr, sizeof(hostaddr), 0, 0, NI_NUMERICHOST);
        printf("\t %s\n", hostaddr);
        peerAddr = peerAddr->ai_next;
    }

    freeaddrinfo(retAddr);

    return 0;
}
