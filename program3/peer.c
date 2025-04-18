#include <stdio.h>
 #include <stdlib.h>
 #include <sys/types.h>
 #include <sys/socket.h>
 #include <netdb.h>
 #include <string.h>
 #include <unistd.h>
 #include <dirent.h>
 #include <arpa/inet.h>
 #include <stdint.h>  
 #include <stdbool.h>
 
 int lookup_and_connect( const char *host, const char *service );
 int send_data_to_soc( int s, const char *buf, int *len );
 int recv_data_from_soc( int s, char *buf, int *len );
 
 int main( int argc, char *argv[] ) {
    const unsigned char join_bytes = 0x00; 
    const unsigned char pub_bytes = 0x01; 
    const unsigned char search_bytes = 0x02; 
 
    // Check that we received exactly three arguments
    if ( argc != 4 ) {
        fprintf( stderr, "Invalid arguments provided\n" );
        exit( 1 );
    }
 
    const char *reg_host=argv[1]; //Registry host 
    const char *reg_port=argv[2]; //Registry port number
    int peer_id=atoi( argv[3] ); //Peer ID (string converted to int because command line argument is text and we need int)

    // Connect to the registry using the provided host and port.
    // This function deals with the host, creates a socket, and connects.
    // If it fails, it returns a negative value.
    int sock_dir=lookup_and_connect( reg_host, reg_port );
    if ( sock_dir<0 ) {
        fprintf( stderr,"Error: could not connect to registry.\n" );
        exit( 1 );
    }

    // Buffer for storing user commands input
    char user_input[256];

    while ( 1 ) {
        printf( "Enter a command: " );

        // Read a line from input
        // If fgets returns NULL, it means either it's at the end of the file or an error, so break out of the loop.
        if ( fgets( user_input,sizeof( user_input ),stdin ) == NULL ) {
            break;
        }

        // Remove the trailing newline character from the input.
        user_input[strcspn( user_input, "\n" )] = '\0';

        // exit section
        if ( strcmp( user_input, "EXIT" ) == 0 ) {
            close( sock_dir );
            break;
        }

        // join section
        else if ( strcmp( user_input,"JOIN" )==0 ) {
            // Builds the join packet
            char joinRequest[5];  
            joinRequest[0]=join_bytes;  // Set the byte (0x00 for join)

            // Convert the peer_id (int) to network byte order and copy it into the packet
            uint32_t peer_ID_net=htonl(( uint32_t )peer_id );
            memcpy( joinRequest+1,&peer_ID_net,sizeof( peer_ID_net ));

            // Prepare to send the 5-byte JOIN packet in one go
            int joinLength = 5;
            if ( send_data_to_soc( sock_dir,joinRequest,&joinLength )==-1 ) {
                perror( "send JOIN" );  // error if sending fails
                close( sock_dir );        // Close the socket if error
                exit( 1 );
            }
            printf( "JOIN request sent.\n" );
        }


        // publish section
        else if ( strcmp( user_input,"PUBLISH" )==0 ) {
            // 1 byte action, 4 bytes file count, then file names
            unsigned char request_from_pub[1200];
            request_from_pub[0]=pub_bytes;

            int count_position=1; // Position of file count in the packet
            int file_start=5;  // Where file names will start
            uint32_t count_file=0;

            // Open "SharedFiles" 
            DIR *dirctry = opendir( "SharedFiles" );
            if ( dirctry==NULL ) { //if it hits null then it fails
                perror( "Failed to open" );
                continue;
            }

            // Traverse the directory to find regular files
            struct dirent *dir_pointing_to;
            while (( dir_pointing_to=readdir( dirctry ))!=NULL ) {
                // Only consider regular files, i.e. ignoring 
                if ( dir_pointing_to->d_type==DT_REG ) {
                    // Copy the file name (plus null terminator) into the packet
                    int len_name = strlen( dir_pointing_to->d_name ) +1;
                    // Check to see if adding this filename won't exceed the packet's max size 
                    if ( file_start+len_name>1200 ) {
                        fprintf( stderr,"PUBLISH packet exceeds maximum size.\n" );
                        break;
                    }
                    // Copy the file name and its null terminator into the packet at the current index
                    snprintf(( char * )request_from_pub+file_start,sizeof( request_from_pub )-file_start,"%s%c",dir_pointing_to->d_name,'\0' );
                    file_start += len_name;
                    count_file++;
                }
            }
            closedir( dirctry );

            // Convert count_file to network order and store it in the packet
            uint32_t count_net_file=htonl( count_file );
            memcpy( request_from_pub+count_position, &count_net_file, sizeof( count_net_file ) );
            // The total size of the publish packet
            int length_of_publish = file_start;
            // Send the packet in one go
            if ( send_data_to_soc(sock_dir, ( const char * )request_from_pub,&length_of_publish )==-1 ) {
                perror( "send PUBLISH" );
                close( sock_dir );
                exit( 1 );
            }
            printf( "PUBLISH request sent with %u file(s).\n", count_file );
        }

        // SEARCH SECTION
        else if ( strcmp( user_input, "SEARCH" )==0 ) {
            // Prompt user for file name
            printf( "Enter a file name: " );
            // Buffer to store the file name
            char name_of_file[256];
            if ( fgets( name_of_file, sizeof( name_of_file ), stdin )==NULL ) { // If no input is received
                continue;
            }
            name_of_file[strcspn( name_of_file, "\n" )] = '\0'; // Remove the newline character

            // finding length of the file name
            int name_of_file_length = strlen( name_of_file ) + 1;
            int request_length = 1 + name_of_file_length; // Total length of the search packet
            unsigned char *search_packet = malloc( request_length ); // allocating memory for the search packet.
            if ( search_packet == NULL ) {
                perror( "malloc" );
                continue;
            }
            search_packet[0] = search_bytes; // Set the first byte to search action
            memcpy( search_packet+1, name_of_file, name_of_file_length ); //copy the file into the packet

            //send all of the search in one go
            if ( send_data_to_soc( sock_dir,( const char * )search_packet, &request_length )==-1 ) {
                perror( "send SEARCH" );
                free( search_packet );
                close( sock_dir );
                exit( 1 );
            }
            free( search_packet );

            // Expect 10-byte response: 4 bytes peer_id, 4 bytes IPv4, 2 bytes port
            char search_response[10];
            int length_response = 10;
            if ( recv_data_from_soc( sock_dir,search_response,&length_response )==-1 ) { // if the recieving search fails it returns -1
                perror( "recv SEARCH response" );
                continue;
            }
            if ( length_response<10 ) { // Check if the full response was received.
                fprintf( stderr, "Incomplete SEARCH response\n" );
                continue;
            }

            // Parse the response, Extract peer_id
            uint32_t peer_ID_of_res;
            memcpy( &peer_ID_of_res,search_response,4 );
            peer_ID_of_res = ntohl( peer_ID_of_res );

            struct in_addr addr; // Extract the IPv4 address
            memcpy( &addr,search_response+4,4 );

            uint16_t respPort; // Extract the port number
            memcpy( &respPort,search_response+8,2 );
            respPort = ntohs( respPort );

            char ipStr[INET_ADDRSTRLEN]; // Converts the IP to string
            inet_ntop( AF_INET,&addr,ipStr,sizeof( ipStr ) );

            // Check if file wasn't found
            if ( peer_ID_of_res==0 && addr.s_addr==0 && respPort==0 ) {
                printf( "File not indexed by registry.\n" );
            } else {
                printf( "File found at\nPeer %u\n%s:%u\n",peer_ID_of_res,ipStr,respPort );
            }
        }


        else if ( strcmp( user_input,"FETCH" ) ==0 ) {
            printf( "Enter a file: " );
            char user_file[100]; //buffer storing the user filename

            // If fgets returns NULL, it means there's no input or an error occurred,
            if ( !fgets( user_file, sizeof( user_file ), stdin )) { 
                continue;
            }
            user_file[strcspn( user_file, "\n" )] = '\0';
            unsigned char search_req[101]; // Building search request to send to registry:
            search_req[0] = 0x02; 
            int file_len = strlen( user_file ) +1; //gets the length of the filename, including its null terminator
            memcpy( search_req+1,user_file,file_len ); // Copy the filename into search request

            int s_len = 1 + file_len;
            // Send search request to the registry
            if ( send_data_to_soc( sock_dir, ( char* )search_req,&s_len ) ==-1 ) {
                perror("send SEARCH");
                continue;
            }

            char resp[10];
            int r_len = 10;
            //either fewer than 10 bytes read or there's an error
            if ( recv_data_from_soc( sock_dir,resp,&r_len ) ==-1||r_len<10 ) {
                fprintf( stderr, "SEARCH response error\n" );
                continue;
            }

            uint32_t peer_id; 
            memcpy( &peer_id, resp, 4 );
            //This extracts the peer ID
            peer_id = ntohl( peer_id );

            struct in_addr addr;
            //This extracts the IP address
            memcpy( &addr,resp+4,4 );

            uint16_t port;
            memcpy( &port,resp+8,2 );
            //This extracts the port
            port = ntohs( port );

            // If the peer_id, address, + port are all 0, the registry didn't find any peer with this file.
            if ( !peer_id && !addr.s_addr && !port ) {
                printf( "File not indexed by registry.\n" );
                continue;
            }

            // Converts the binary into text
            char ip_str[INET_ADDRSTRLEN];
            if ( !inet_ntop( AF_INET,&addr,ip_str,sizeof( ip_str ))) {
                perror( "inet_ntop" );
                continue;
            }

            // Converts the port into a string
            char port_strs[16];
            snprintf( port_strs,sizeof( port_strs ),"%u",port );

            // connects to peer 
            int peer =lookup_and_connect( ip_str, port_strs );
            if ( peer<0 ){
                fprintf( stderr, "Could not connect to peer" );
                continue;
            }

            // this builds the fetch request
            unsigned char fetch_reqst[101];
            fetch_reqst[0] =0x03; // Set first byte to 0x03, this indicate a fetch
            memcpy( fetch_reqst+1,user_file,file_len );
            int f_len =1+file_len;

            // Send the FETCH request to the connected peer 
            if ( send_data_to_soc( peer, ( char* )fetch_reqst,&f_len ) ==-1 ) {
                perror( "send the fetch out" );
                close( peer );
                continue;
            }

            unsigned char code;
            int c_len = 1;

            // Receive the response code from the peer; if there's an error or we got 0 bytes, report it
            if ( recv_data_from_soc( peer,( char* )&code,&c_len )==-1||c_len<1 ) {
                fprintf( stderr, "fetch response error\n" );
                close( peer );
                continue;
            }
            FILE *file_point = fopen( user_file,"wb" ); //opens the file

            if ( file_point==NULL ) {
                perror( "fopen" ); //if it fails
                close( peer );
                continue;
            }
        
            char buffer_for_peer[512];
            // This doesn't stop reading the data from socket till there's none left
            while ( true ) {
                int bytes_expected = sizeof( buffer_for_peer );
                //if there's no more data or a weird error happens then break out
                if (recv_data_from_soc( peer,buffer_for_peer,&bytes_expected ) ==-1 || bytes_expected ==0 )
                    break;
                fwrite( buffer_for_peer,1,bytes_expected,file_point ); // Whatever bytes we're successful, write it to the file
            }
        
            fclose( file_point );
            close( peer );
            printf("File found at Peer %u %s:%u. file saved \"%s\".\n",peer_id, ip_str, port, user_file );

            
            
            

        }



    }

    return 0;
}
 
 
 
 
 
 
 
 
 
 
 //code given
 int lookup_and_connect( const char *host, const char *service ) {
 	struct addrinfo hints;
 	struct addrinfo *rp, *result;
 	int s;
 	memset( &hints, 0, sizeof( hints ) );
 	hints.ai_family = AF_INET;
 	hints.ai_socktype = SOCK_STREAM;
 	hints.ai_flags = 0;
 	hints.ai_protocol = 0;
 	if ( ( s = getaddrinfo( host, service, &hints, &result ) ) != 0 ) {
 		fprintf( stderr, "stream-talk-client: getaddrinfo: %s\n", gai_strerror( s ) );
 		return -1;
 	}
 	for ( rp = result; rp != NULL; rp = rp->ai_next ) {
 		if ( ( s = socket( rp->ai_family, rp->ai_socktype, rp->ai_protocol ) ) == -1 ) {
 			continue;
 		}
 		if ( connect( s, rp->ai_addr, rp->ai_addrlen ) != -1 ) {
 			break;
 		}
 		close( s );
 	}
 	if ( rp == NULL ) {
 		perror( "stream-talk-client: connect" );
 		return -1;
 	}
 	freeaddrinfo( result );
 
 	return s;
 }
 
 // function used in last program
 int send_data_to_soc(int s, const char *buf, int *len) {
     int tot = 0;
     int bytes_left = *len;
     int num;
     while (tot < *len) {
         num = send(s, buf + tot, bytes_left, 0);
         if (num == -1)
             break;
         tot += num;
         bytes_left -= num;
     }
     *len = tot;
     if (num == -1) {
         return -1;
     } else {
         return 0;
     }
 }
 
 // function used in last program
 int recv_data_from_soc(int s, char *buf, int *len) {
     int tot = 0;
     int bytes_left = *len;
     int num;
     while (tot < *len) {
         num = recv(s, buf + tot, bytes_left, 0);
         if (num <= 0)
             break;
 		tot += num;
         bytes_left -= num;
     }
     *len = tot;
     if (num < 0) {
         return -1;
     } else {
         return 0;
     }
 }