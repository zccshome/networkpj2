#include "rtlib.h"
#include "rtgrading.h"
#include "csapp.h"
#include <stdlib.h>
#include "rtgrading.h"
#include "daemon.h"

/* Global variables */
u_long curr_nodeID;
rt_config_file_t   curr_node_config_file;  /* The config_file  for this node */
rt_config_entry_t *curr_node_config_entry; /* The config_entry for this node */
daemon_node curr_daemon_node;
int daemon_sockfd;
daemon_node d_node[max_node_num];
/*daemon_routes d_routes;*/
int graph[max_node_num][max_node_num];

int main( int argc, char *argv[] )
{
	rt_init(argc, argv);
    init_node(argc, argv);
    int daemon_port = curr_node_config_entry->routing_port;
    setup_daemon();
    setup_route();
	setup_socket(daemon_port);
    return 0;
}

void setup_daemon()
{
    bzero(&curr_daemon_node, sizeof(curr_daemon_node));
    bzero(&d_node, sizeof(daemon_node) * max_node_num);
    curr_daemon_node.version = 1;
    curr_daemon_node.TTL = 32;
    curr_daemon_node.type = 1;
    curr_daemon_node.sender_nodeID = curr_nodeID;
    curr_daemon_node.sequence_number = 1;
    curr_daemon_node.num_link_entries = curr_node_config_file.size;
    int i;
    for(i = 0; i < curr_daemon_node.num_link_entries; i++)
        curr_daemon_node.l_entries[i].node_entry = curr_node_config_file.entries[i];
}

void setup_route()
{
    bzero(&d_routes, sizeof(d_routes));
    graph[curr_nodeID][curr_nodeID] = 0;
    int i,j;
    for(i = 0; i < curr_daemon_node.num_link_entries; i++)
    {
        int nodeID = curr_daemon_node.l_entries[i].node_entry.nodeID;
        graph[curr_nodeID][nodeID] = 1;
        graph[nodeID][curr_nodeID] = 1;
    }
    for(i = 0; i < max_node_num; i++)
    {
        daemon_node recv_node = d_node[i];
        int sender_nodeID = recv_node.sender_nodeID;
        for(j = 0; j < recv_node.num_link_entries; j++)
        {
            int nodeID = recv_node.l_entries[j].node_entry.nodeID;
        }
    }
}

int setup_socket(int port)
{
	int listenfd, optval=1;
    struct sockaddr_in serveraddr;
  
    /* Create a socket descriptor */
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    return -1;

    /* Eliminates "Address already in use" error from bind. */
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, 
           (const void *)&optval , sizeof(int)) < 0)
    return -1;
    
    // getting current options
    if (0 > (optval = fcntl(listenfd, F_GETFL)))
        return -1;
    // modifying and applying
    optval = (optval | O_NONBLOCK);
    if (fcntl(listenfd, F_SETFL, optval))
        return -1;

    /* Listenfd will be an endpoint for all requests to port
       on any IP address for this host */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET; 
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY); 
    serveraddr.sin_port = htons((unsigned short)port); 
    if (bind(listenfd, (SA *)&serveraddr, sizeof(serveraddr)) < 0)
        return -1;
    socklen_t serveraddr_len = sizeof(serveraddr);
    while(1)
    {
        daemon_node recv_node;
        bzero(&recv_node, sizeof(recv_node));
        if(rt_recvfrom(listenfd, &recv_node, sizeof(recv_node), 0, (SA *)&serveraddr, &serveraddr_len) > 0)
        {
            int nodeID = recv_node.sender_nodeID;
            int sequence_number = recv_node.sequence_number;
            int i;
            for(i = 0; i < max_node_num; i++)
            {
                daemon_node temp_node = d_node[i];
                if(temp_node.version == 1)
                {
                    if(temp_node.sender_nodeID == nodeID && temp_node.sequence_number < sequence_number)
                    {
                        d_node[i] = recv_node;
                        break;
                    }
                    else if(temp_node.sender_nodeID == nodeID && temp_node.sequence_number > sequence_number)
                    {
                        rt_sendto(listenfd, &temp_node, sizeof(temp_node), 0, (SA *)&serveraddr, serveraddr_len);
                        break;
                    }
                }
            }
            if(i == max_node_num)
            {
                int j;
                for(j = 0; j < max_node_num; j++)
                {
                    daemon_node temp_node = d_node[i];
                    if(temp_node.version == 0)
                    {
                        d_node[j] = recv_node;
                        break;
                    }
                }
                if(j == max_node_num)
                {
                    printf("Too many clients!");
                }
            }
        }
        else
        {
            /* int i;
            for(i = 0; i < curr_node_config_file.size; i++)
            {
                struct rt_config_entry_s temp_entry = curr_node_config_file.entries[i];
                int route_port = temp_entry.routing_port;
                int client_fd = Open_clientfd("127.0.0.1", route_port);
                rt_sendto(client_fd, &curr_daemon_node, sizeof(curr_daemon_node), 0, NULL, 0);
                Close(client_fd);
            }*/

        }
    }
    return listenfd;
}

int init()
{
    while (1)
    {
        /* each iteration of this loop is "cycle"
        daemon_node node = wait_for_event();
        if(event == INCOMING_ADVERTISEMENT)
        {
            process_incoming_advertisements_from_neighbor();
        }
        else if(event == IT_IS_TIME_TO_ADVERTISE_ROUTES)
        {
            advertise_all_routes_to_all_neighbors();
            check_for_down_neighbors();
            expire_old_routes();
            delete_very_old_routes();
        }*/

        Select 
        {
            if(receiveCommandFromServer)
            {
                //handle command:
                if ( command == userTable / channelTable / nextHop / nextHops )
                {
                    if ( nodeTable not exists )
                        generate nodeTable ;
                    
                    query table and return record ;
                }
                else if ( command == addUser / addChannel / removeUser / removeChannel )
                {
                    modify local LSA entry ;
                    flood ;
                    delete the cache of nodeTable ;
                }

                if ( command == addUser / addChannel / removeUser / removeChannel )
                    flood LSA to all neighbors;
            }
            else(receiveLSA)
            {
                if ( the LSA is of local node and sequence_number > local sequence_number )
                    update sequence_number ;
                if ( node is found in LSA table )
                {
                    if ( TTL = 0 )
                    {
                        delete LSA of that node ;
                        flood out ;
                    } 
                    else if ( newer )
                    {
                        TTL -- ;
                        if ( TTL != 0 )
                        {
                            replace ;
                            flood to all neighbors except the sender ;
                        }
                    }
                      
                    else
                    {
                        send back LSA with higher sequence_number ;
                    }
                }
                else ( not found )
                {
                    if ( TTL == 0 )
                        silent ;

                    add to LSA table ;
                    flood to all neighbors except the sender ;
                }
            }
        }

        new thread
        {
            every X seconds: flood LSA of local node to all neighbors ;
        }

        new thread
        {
            every 10s : check for down nodes;
            if ( node A down )
            {
                delete LSA of node A locally ;
                send LSA of node A with TTL = 0 to all neighbors ;
            }
        }

        new thread : flood
        {
            flood ;
            wait for ack ;
        }


    }
    return 1;
}
void build_route()
{
}
/*
 * void init_node( int argc, char *argv[] )
 *
 * Takes care of initializing a node for an IRC server
 * from the given command line arguments
 */
void init_node( int argc, char *argv[] )
{
    int i;

    if( argc < 3 )
    {
        printf( "%s <nodeID> <config file>\n", argv[0] );
        exit( 0 );
    }

    /* Parse nodeID */
    curr_nodeID = atol( argv[1] );

    /* Store  */
    rt_parse_config_file(argv[0], &curr_node_config_file, argv[2] );

    /* Get config file for this node */
    for( i = 0; i < curr_node_config_file.size; ++i )
        if( curr_node_config_file.entries[i].nodeID == curr_nodeID )
             curr_node_config_entry = &curr_node_config_file.entries[i];

    /* Check to see if nodeID is valid */
    if( !curr_node_config_entry )
    {
        printf( "Invalid NodeID\n" );
        exit(1);
    }
}


/*
 * size_t get_msg( char *buf, char *msg )
 *
 * char *buf : the buffer containing the text to be parsed
 * char *msg : a user malloc'ed buffer to which get_msg will copy the message
 *
 * Copies all the characters from buf[0] up to and including the first instance
 * of the IRC endline characters "\r\n" into msg.  msg should be at least as
 * large as buf to prevent overflow.
 *
 * Returns the size of the message copied to msg.
 */
size_t get_msg(char *buf, char *msg)
{
    char *end;
    int  len;

    /* Find end of message */
    end = strstr(buf, "\r\n");

    if( end )
    {
        len = end - buf + 2;
    }
    else
    {
        /* Could not find \r\n, try searching only for \n */
        end = strstr(buf, "\n");
	if( end )
	    len = end - buf + 1;
	else
	    return -1;
    }

    /* found a complete message */
    memcpy(msg, buf, len);
    msg[end-buf] = '\0';

    return len;	
}

/*
 * int tokenize( char const *in_buf, char tokens[MAX_MSG_TOKENS][MAX_MSG_LEN+1] )
 *
 * A strtok() variant.  If in_buf is a space-separated list of words,
 * then on return tokens[X] will contain the Xth word in in_buf.
 *
 * Note: You might want to look at the first word in tokens to
 * determine what action to take next.
 *
 * Returns the number of tokens parsed.
 */
int tokenize( char const *in_buf, char tokens[MAX_MSG_TOKENS][MAX_MSG_LEN+1] )
{
    int i = 0;
    const char *current = in_buf;
    int  done = 0;

    /* Possible Bug: handling of too many args */
    while (!done && (i<MAX_MSG_TOKENS)) 
    {
        char *next = strchr(current, ' ');

    	if (next) 
        {

     	    memcpy(tokens[i], current, next-current);
    	    tokens[i][next-current] = '\0';
    	    current = next + 1;   /* move over the space */
    	    ++i;

    	    /* trailing token */
    	    if (*current == ':') 
            {
        	    ++current;
        		strcpy(tokens[i], current);
        		++i;
        		done = 1;
        	}
        } 
        else 
        {
            if ( strlen ( current ) == MAX_MSG_LEN )
            {
                printf(">> Error in tokenize! message is too long.\n");
                return -1 ;
            }
            else
            {
                strcpy(tokens[i], current);
                ++i;
                done = 1;
            }
        }
    }

    return i;
}