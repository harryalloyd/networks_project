#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

const unsigned char join_bytes = 0x00;
const unsigned char pub_bytes = 0x01;
const unsigned char search_bytes = 0x02;

struct peer_entry {
    uint32_t id;
    int sock;
    struct in_addr ip;
    uint16_t port;
    int file_cnt;
    char files[10][101];
};

static struct peer_entry peers[5];
static int peer_cnt = 0;

struct peer_entry *peer_by_socket ( int s ) {
    for ( int i=0; i<peer_cnt; i++ ) {
        if ( peers[i].sock== s ) {
            return &peers[i];
        }
    }
    return NULL;
}

struct peer_entry *file_lookup ( const char *name ) {
    for ( int i=0; i<peer_cnt; i++ ) {
        for ( int j=0; j<peers[i].file_cnt; j++ ) {
            if ( strcmp( peers[i].files[j],name ) == 0 ) {
                return &peers[i];
            }
        }
    }
    return NULL;
}

int m_listener ( const char *port ) {
    struct addrinfo hints ={ 0 }, *res;
    hints.ai_family =AF_INET;
    hints.ai_socktype =SOCK_STREAM;
    hints.ai_flags =AI_PASSIVE;

    if ( getaddrinfo( NULL,port,&hints,&res ) != 0 ) {
        perror( "getaddrinfo" );
        exit( 1 );
    }

    int s= socket( res->ai_family,res->ai_socktype,res->ai_protocol );
    if ( s<0 ) {
        perror( "socket" );
        exit( 1 );
    }

    int yes= 1;
    setsockopt( s,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof( yes ) );

    if ( bind( s,res->ai_addr,res->ai_addrlen )< 0 ) {
        perror( "bind" );
        exit( 1 );
    }

    if ( listen( s,5 )<0 ) {
        perror( "listen" );
        exit( 1 );
    }

    freeaddrinfo( res );
    return s;
}

void h_join ( int sd ) {
    unsigned char buf[4];
    if ( recv( sd,buf,4,0 ) != 4 ) {
        return;
    }

    uint32_t net_id;
    memcpy( &net_id,buf,4 );
    uint32_t id= ntohl( net_id );

    if ( peer_cnt==5 ) {
        return;
    }

    struct sockaddr_in addr;
    socklen_t alen= sizeof( addr );
    getpeername( sd,( struct sockaddr * ) &addr,&alen );

    struct peer_entry *p = &peers[peer_cnt++];
    p->id= id;
    p->sock= sd;
    p->ip= addr.sin_addr;
    p->port= ntohs( addr.sin_port );
    p->file_cnt = 0;

    printf( "TEST] JOIN %u\n",id );
    fflush( stdout );
}

void h_publish ( int sd ) {
    unsigned char hdr[4];
    if ( recv( sd,hdr,4,0 ) != 4 ) {
        return;
    }

    uint32_t net_cnt;
    memcpy( &net_cnt,hdr,4 );
    uint32_t cnt = ntohl( net_cnt );

    struct peer_entry *p = peer_by_socket( sd );
    if ( !p ||cnt == 0|| cnt>10 ) {
        return;
    }

    p->file_cnt = 0;
    for ( uint32_t i=0; i<cnt; i++ ) {
        char name[101];
        int idx= 0;
        char ch;
        while ( idx<101 && recv( sd,&ch,1,0 )==1 ) {
            name[idx++]= ch;
            if ( ch== '\0' ) {
                break;
            }
        }
        if ( idx== 101 ) {
            name[100]= '\0';
        }
        strncpy( p->files[p->file_cnt++],name,100 );
    }

    printf( "TEST] PUBLISH %d",p->file_cnt );
    for ( int i=0; i<p->file_cnt; i++ ) {
        printf(" %s",p->files[i] );
    }
    printf( "\n" );
    fflush( stdout );
}

void h_search ( int sd ) {
    char fname[101];
    int idx= 0;
    char ch;
    while ( idx<101 && recv( sd,&ch,1,0 )== 1 ) {
        fname[idx++] = ch;
        if ( ch == '\0' ) {
            break;
        }
    }
    if ( idx== 101 ) {
        fname[100]= '\0';
    }

    struct peer_entry *own =file_lookup( fname );
    unsigned char resp[10]= { 0 };
    uint32_t id_h= 0;
    uint16_t port_h= 0;
    struct in_addr ip= { 0 };

    if ( own ) {
        id_h= own->id;
        port_h= own->port;
        ip= own->ip;
        uint32_t id_n= htonl( id_h );
        uint16_t port_n= htons( port_h );
        memcpy( resp,&id_n,4 );
        memcpy( resp+4,&ip,4 );
        memcpy( resp+8,&port_n,2 );
    }

    send( sd,resp,10,0 );
    char ipbuf[INET_ADDRSTRLEN];
    inet_ntop( AF_INET,&ip,ipbuf,sizeof( ipbuf ) );
    if ( !own ) {
        strcpy( ipbuf,"0.0.0.0" );
    }
    printf("TEST] SEARCH %s %u %s:%u\n",fname,id_h,ipbuf,port_h );
    fflush( stdout );
}

int main ( int argc,char *argv[] ) {
    if ( argc != 2 ) {
        fprintf( stderr,"Usage: %s <port>\n",argv[0] );
        exit( 1 );
    }
    int listen_sd = m_listener( argv[1] );
    fd_set master,readfds;
    FD_ZERO( &master );
    FD_SET( listen_sd,&master );
    int fd_max= listen_sd;

    while ( true ) {
        readfds = master;
        if ( select( fd_max+1,&readfds,NULL,NULL,NULL )<0 ) {
            perror( "select" );
            exit( 1 );
        }
        for ( int sd=0; sd<=fd_max; sd++ ) {
            if ( !FD_ISSET( sd,&readfds ) ) {
                continue;
            }
            if ( sd==listen_sd ) {
                struct sockaddr_in cli;
                socklen_t clen= sizeof( cli );
                int new_sd= accept( listen_sd,( struct sockaddr * ) &cli,&clen );
                if ( new_sd<0 ) {
                    continue;
                }
                FD_SET( new_sd,&master );
                if ( new_sd >fd_max ) {
                    fd_max =new_sd;
                }
            } else {
                unsigned char op;
                int n = recv( sd,&op,1,0 );
                if ( n <= 0 ) {
                    close( sd );
                    FD_CLR( sd,&master );
                    for ( int i=0; i<peer_cnt; i++ ) {
                        if ( peers[i].sock==sd ) {
                            peers[i] =peers[--peer_cnt];
                            break;
                        }
                    }
                    continue;
                }
                if ( op==join_bytes ) {
                    h_join( sd );
                } else if ( op==pub_bytes ) {
                    h_publish( sd );
                } else if ( op==search_bytes ) {
                    h_search( sd );
                } else {
                    close( sd );
                    FD_CLR( sd,&master );
                }
            }
        }
    }
    return 0;
}
