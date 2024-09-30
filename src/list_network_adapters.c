#include <netdb.h>
#include <ifaddrs.h>
#include <stdio.h>


int main( int argv, char** argc )
{
    struct ifaddrs* addresses;

    if( getifaddrs( &addresses ) != 0 )
    {
        printf( "getifaddrs call failed\n" );
        return -1;
    }

    struct ifaddrs* address = addresses;
    printf( "Interface\tProtocol\t\tAddress\n" );
    printf( "---------\t--------\t\t-------\n" );
    while( address )
    {
        int family = address->ifa_addr->sa_family;
        if( family == AF_INET || family == AF_INET6 )
        {
            printf( "%s\t\t", address->ifa_name );
            printf( "%s\t\t", family == AF_INET ? "IPV4" : "IPV6" );

            char ap[100];
            const int family_size = family == AF_INET ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6);

            getnameinfo( address->ifa_addr, family_size, ap, sizeof(ap), 0, 0, NI_NUMERICHOST );
            printf( "\t%s\n", ap );
        }

        address = address->ifa_next;
    }

    freeifaddrs( addresses );
    return 0;
}
