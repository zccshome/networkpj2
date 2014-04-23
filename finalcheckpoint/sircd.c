/* To compile: gcc sircd.c rtlib.c rtgrading.c csapp -> c -lpthread -osircd */

#include "rtlib.h"
#include "rtgrading.h"
#include "csapp.h"
#include <stdlib.h>
#include "csapp.h"
#include "sircd.h"

/* Macros */
#define MAX_MSG_TOKENS 10
#define MAX_MSG_LEN 512

/* Global variables */
u_long curr_nodeID;
rt_config_file_t   curr_node_config_file;  /* The config_file  for this node */
rt_config_entry_t *curr_node_config_entry; /* The config_entry for this node */

client *client_list[FD_SETSIZE];	// Global client list
channel *channel_list[FD_SETSIZE];	// Global channel list
int client_sum;				// Number of global clients
int channel_sum;			// Number of global channels
char *default_client_name ; // Nickname and username of client by default

pool *p ;                    // Global pool struct

int daemon_sockfd ;         // sockfd to connect with daemon

/* Function prototypes */
void init_node( int argc, char *argv[] );
size_t get_msg( char *buf, char *msg );
int tokenize( char const *in_buf, char tokens[MAX_MSG_TOKENS][MAX_MSG_LEN+1] );
void add_client(int isock ) ; // add a connection to pool
void check_clients() ; // handle the connection
channel *get_channel_by_name ( char *channel_name ) ;
int client_leave_channel ( int isock , char *channel_name ) ;

// commander handler
void parse_message ( int isock , char buffer[MAX_MSG_LEN] );
void nick_handler ( int isock , char tokens[MAX_MSG_TOKENS][MAX_MSG_LEN + 1] , int commLen );
void user_handler ( int isock , char tokens[MAX_MSG_TOKENS][MAX_MSG_LEN + 1] , int commLen );
void quit_handler ( int isock , char tokens[MAX_MSG_TOKENS][MAX_MSG_LEN + 1] , int commLen );
void join_handler ( int isock , char tokens[MAX_MSG_TOKENS][MAX_MSG_LEN + 1] , int commLen );
void part_handler ( int isock , char tokens[MAX_MSG_TOKENS][MAX_MSG_LEN + 1] , int commLen );
void list_handler ( int isock , char tokens[MAX_MSG_TOKENS][MAX_MSG_LEN + 1] , int commLen );
void privmsg_handler ( int isock , char tokens[MAX_MSG_TOKENS][MAX_MSG_LEN + 1] , int commLen );
void who_handler ( int isock , char tokens[MAX_MSG_TOKENS][MAX_MSG_LEN + 1] , int commLen );
// commander handler for pj2
void user_forwarding_handler ( char tokens[MAX_MSG_TOKENS][MAX_MSG_LEN + 1] , int commLen );
void channel_forwarding_handler ( char tokens[MAX_MSG_TOKENS][MAX_MSG_LEN + 1] , int commLen );
void forward_to_user (char *source_user_nickname , char *target_user_nickname , int nexthop , char *msg ) ;
void forward_to_channel( int source_nodeID , char *source_user_nickname , char *target_channel_name , int nexthop , char *msg ) ;

/* Main */
int main( int argc, char *argv[] )
{
    init_node( argc, argv );
    unsigned short port = curr_node_config_entry->irc_port ;
    printf( "I am node %lu and I listen on port %d for new users\n", curr_nodeID, port );

    // Get port to connect with local daemon
    unsigned short local_port = curr_node_config_entry->local_port ;
    printf( "I connect with local daemon on port %d \n", local_port );
    
    /* my work */
    
    client_sum = 0 ;
    channel_sum = 0 ;
    default_client_name = Malloc ( 20 ) ;
    memset ( default_client_name , 0 , sizeof ( default_client_name ) );
    sprintf( default_client_name , "Tourist" );

    struct sockaddr caddr;
    int sockfd, clen, isock ; // sockfd = listenfd , isock = connfd
    clen = sizeof(caddr) ;
   
    sockfd = Open_listenfd(port);

    // Open connect with local daemon
    daemon_sockfd = Open_clientfd("127.0.0.1",local_port);
    //printf("hehemiaowuwuwu  %d \n",daemon_sockfd);

    // Init pool
    p = Malloc ( sizeof ( pool ) ) ;
    memset ( p , 0 , sizeof ( pool ) );
    int i ;
    for ( i = 0 ; i < FD_SETSIZE ; i ++ )
      p -> clientfd[i] = -1;
    // at first we only listen on current sockfd
    p -> maxfd = sockfd + 1;
    // Setup your read_set with FD_ZERO and the server socket descriptor
    FD_ZERO(& p -> read_set);
    FD_SET(sockfd, & p -> read_set);    

    while (1) {
        p -> ready_set = p -> read_set;
        p -> nready = Select( p -> maxfd + 1 , & p -> ready_set , & p -> write_set , NULL , NULL );
        if (FD_ISSET(sockfd, & p -> ready_set)) {
	       isock = Accept ( sockfd , ( struct sockaddr * ) &caddr, (socklen_t * ) &clen ) ;
	       add_client(isock);
        }
        check_clients();
    }
    // close it up down here
    
    return 0;
}

void add_client(int isock )
{
    p -> nready -- ;   
    int i ;
    for ( i = 0 ; i < FD_SETSIZE ; i ++ )
    {
    	if ( p -> clientfd[i] < 0 )
    	{
    	    p -> clientfd[i] = isock ;
    	    Rio_readinitb ( & p -> client_read_buf[i] , isock ) ;
    	    FD_SET ( isock , & p -> read_set ) ;
    	    if ( isock > p -> maxfd )
    	    {
    		  p -> maxfd = isock ;		
    	    }
    	    printf( "New client of fd %d had been added!\n" , isock );
    	    //Write( isock , ">> Welcome, tourist！\n" , strlen(">> Welcome, tourist！\n") );
            break ;
    	}
    }

    client *curClient = Malloc ( sizeof ( client ) ) ;
    memset( curClient , 0 , sizeof( client ) ) ;
    curClient -> clientfd = isock ;
    strcpy ( curClient -> nickname , default_client_name );
    strcpy ( curClient -> username , default_client_name );
    //strcpy ( curClient -> hostname , "" );     // ?????????????????????????????????????
    //strcpy ( curClient -> servername , "" );   // ?????????????????????????????????????
    //strcpy ( curClient -> realname , "" );     // ?????????????????????????????????????
    curClient -> channel_id = -1 ;
    //curClient -> nick_is_set = 0 ;             // ?????????????????????????????????????
    //curClient -> user_is_set = 0 ;             // ?????????????????????????????????????

    client_list[isock] = curClient ;
    client_sum ++ ;

    if ( i == FD_SETSIZE )
    {
	   printf( "Cannot add the client! Too many connections." );
    }
}

void check_clients()
{
    int i , isock , len ;
    char buffer[MAX_MSG_LEN] ;
    rio_t rio ;
    
    for ( i = 0 ; i < FD_SETSIZE && ( p -> nready > 0 ) ; i ++ )
    {
    	isock = p -> clientfd[i] ;
    	if ( ( isock > 0 ) && ( FD_ISSET ( isock , & p -> ready_set ) ) ) 
    	{
    	    p -> nready -- ;
    	    rio = p -> client_read_buf[i] ;
    	    if ( ( len = Rio_readlineb ( &rio , buffer , MAX_MSG_LEN ) ) != 0 )
    	    {
                printf ("Server received %d bytes from fd %d : %s" , len , isock , buffer ) ;
                //Rio_writen ( isock , buffer , len ) ;   // echo

                // Deal with multiple commands in one packet
                char *list = buffer ;
                char *curCommand ;
                char preCommand[MAX_MSG_LEN] ;
                int count = 0 ;
                while ( ( curCommand = strsep ( &list , ";" ) )  != NULL )
                {
                    //printf("%s\n", curCommand);
                    if ( count != 0 )
                    {
                        sprintf(preCommand, "%s\n", preCommand );
                        parse_message ( isock , preCommand ) ;
                    }
                    memcpy( preCommand , curCommand , strlen( curCommand ) );
                    count ++ ;              
                }
                parse_message ( isock , preCommand ) ;
    	    }
    	    // If EOF is read, which indicates that the client has use "Ctrl + C" to leave brutally
    	    else
    	    {
        		Close ( isock ) ;
        		FD_CLR ( isock , & p -> read_set ) ;
        		p -> clientfd[i] = -1 ;

                Free ( client_list[isock] ) ;
                client_sum -- ;
                printf( "Client %d has leaved brutally!\n" , isock );

                // Tell the daemon
                client *curClient = client_list[isock] ;
                if ( strcmp ( curClient -> nickname , default_client_name ) != 0 )
                {
                    char daemon_buffer[MAX_MSG_LEN] ;
                    sprintf ( daemon_buffer , "REMOVEUSER %s\n" , curClient -> nickname );
                    Write( daemon_sockfd , daemon_buffer , strlen ( daemon_buffer ) );
                }
    	    }
    	}
    }
}

void parse_message ( int isock , char buffer[MAX_MSG_LEN] ) 
{//*
    char message [MAX_MSG_LEN] ;
    if ( get_msg ( buffer , message ) == -1 )
    {
        printf( ">> Error! Message is too long!\n" );
        char buffer[128] ;
        sprintf ( buffer , ">> Error! Your message is too long! Your input length is limited to 510 byte.\n" ) ;
        Write( isock , buffer , strlen ( buffer ) );
        return ;
    }
//*
    char tokens [MAX_MSG_TOKENS][MAX_MSG_LEN + 1];
             
    int commLen = tokenize(message, tokens) ;   
    if ( commLen == -1 )
    {
        printf(">> Error! Message is too long!\n" );
        char buffer[128] ;
        sprintf ( buffer , ">> Error! Your message is too long! Your input length is limited to 510 byte.\n" ) ;
        Write( isock , buffer , strlen ( buffer ) );
        return ;
    }
    else if ( commLen == 0){
        printf("Cannot token the message from fd %d!\n" , isock );
        char buffer[128] ;
        sprintf ( buffer , ">> Error! Your message cannot be token.\n" ) ;
        Write( isock , buffer , strlen ( buffer ) );
        return ;
    }
    //printf("hehe\n");
//*
    char *firstToken = tokens[0] ;

    //printf( "   see : \"%s\" \n" , firstToken ) ;

    if ( strncasecmp( firstToken , "NICK" , sizeof("NICK") ) == 0 )
        nick_handler ( isock , tokens , commLen ) ;
    else if ( strncasecmp( firstToken , "USER" , sizeof("USER") ) == 0 )
        user_handler ( isock , tokens , commLen ) ;
    else if ( strncasecmp( firstToken , "QUIT" , sizeof("QUIT") ) == 0 )
        quit_handler ( isock , tokens , commLen ) ;
    else if ( strncasecmp( firstToken , "JOIN" , sizeof("JOIN") ) == 0 )
        join_handler ( isock , tokens , commLen ) ;
    else if ( strncasecmp( firstToken , "PART" , sizeof("PART") ) == 0 )
        part_handler ( isock , tokens , commLen ) ;
    else if ( strncasecmp( firstToken , "LIST" , sizeof("LIST") ) == 0 )
        list_handler ( isock , tokens , commLen ) ;
    else if ( strncasecmp( firstToken , "PRIVMSG" , sizeof("PRIVMSG") ) == 0 )
        privmsg_handler ( isock , tokens , commLen ) ;
    else if ( strncasecmp( firstToken , "WHO" , sizeof("WHO") ) == 0 )
        who_handler ( isock , tokens , commLen ) ;
    else if ( strncasecmp( firstToken , "USERFORWARD" , sizeof("USERFORWARD") ) == 0 )
        user_forwarding_handler ( tokens , commLen ) ;
    else if ( strncasecmp( firstToken , "CHANNELFORWARD" , sizeof("CHANNELFORWARD") ) == 0 )
        channel_forwarding_handler ( tokens , commLen ) ;
    else    // Unknow commands
    {
        char response[MAX_MSG_LEN] ;
        sprintf( response , "%s: ERR UNKNOWNCOMMAND\n", message);
        Write( isock , response , strlen(response) );
    }//*/
}

void nick_handler ( int isock , char tokens[MAX_MSG_TOKENS][MAX_MSG_LEN + 1] , int commLen )
{
    // If the format of command is illegal, send err info back
    if ( commLen == 1 )
    {   
        char buffer[128] ;
        sprintf ( buffer , ">> The formant neet to be : NICK <nickname>.\n" ) ;
        Write( isock , buffer , strlen ( buffer ) );
        return ;
    }
    
    char *newNickname = tokens[1] ;

    if ( strlen ( newNickname ) > 16 )
    {
        char buffer[128] ;
        sprintf ( buffer , ">> Error! Some argument is too long.\n" ) ;
        Write( isock , buffer , strlen ( buffer ) );
        return ;
    }

    // Check whether the nickname has been already used
    int i ;
    int duplicated = 0 ;
    for ( i = 0 ; i < FD_SETSIZE ; i ++ )
    {
        if ( ( client_list[i] != NULL ) && ( strcmp ( client_list[i] -> nickname , newNickname ) == 0 ) )
        {
            duplicated = 1 ;
            break ;
        }
    }
    
    // If the nickname is duplicated
    if ( duplicated == 1 )
    {
        char buffer[MAX_MSG_LEN] ;
        sprintf ( buffer , ">> Sorry, the nickname \"%s\" has been used!\n" , newNickname ) ;
        Write( isock , buffer , strlen ( buffer ) );
        return ;
    }
    
    // If the nickname is not duplicated
    client *curClient = client_list[isock] ;

    // Check whether the client is recorded
    if ( curClient == NULL )
    {
        char buffer[MAX_MSG_LEN] ;
        sprintf ( buffer , ">> Error! Cannot find the client record!\n");
        Write( isock , buffer , strlen ( buffer ) );
        return ;
    }
    
    char buffer[MAX_MSG_LEN] ;

    // If the previous nickname is the default "Tourist"
    if ( ( strcmp ( curClient -> nickname , default_client_name ) ) == 0 )
    {                
        sprintf ( buffer , ">> OK! Your nickname has been changed to \"%s\" successfully!\n" , newNickname ) ;
    }
    // If the client has set its nickname before
    else
    {
        sprintf ( buffer , ">> OK! Your nickname has been changed to \"%s\" successfully! Your previous nickname is \"%s\".\n" , newNickname , curClient -> nickname ) ;
    }

    // Update nickname


        // Tell the daemon
    char daemon_buffer[MAX_MSG_LEN] ;
    sprintf ( daemon_buffer , "REMOVEUSER %s\n" , curClient -> nickname );
    Write( daemon_sockfd , daemon_buffer , strlen ( daemon_buffer ) );

    strcpy ( curClient -> nickname , newNickname ) ;  

    //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
    if ( curClient -> user_is_set == 1 )
    {
        char buf[1024];
        char *hostname = Malloc(sizeof(char) *255);
        gethostname(hostname, 255);
        sprintf(buf,":%s 375 %s :- <server> Message of the day -.\n", hostname, curClient->nickname);
        sprintf(buf,"%s:%s 372 %s :- Send motd. \n",buf, hostname, curClient->nickname);
        sprintf(buf,"%s:%s 376 %s :End of /MOTD command.\n", buf, hostname, curClient->nickname);
        Write(isock, buf, strlen(buf));
        printf( "@@\n%d@@\n" , strlen(curClient->nickname) );
    }
    //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

    // Response to the client
    //Write( isock , buffer , strlen ( buffer ) );    
    
    // Tell the daemon
    sprintf ( daemon_buffer , "ADDUSER %s\n" , newNickname );
    Write( daemon_sockfd , daemon_buffer , strlen ( daemon_buffer ) );
}

void user_handler ( int isock , char tokens[MAX_MSG_TOKENS][MAX_MSG_LEN + 1] , int commLen )
{
    // If the format of command is illegal, send err info back
    if ( commLen < 5 )  // username, hostname, servername, realname 
    {
        char buffer[128] ;
        sprintf ( buffer , ">> The formant neet to be : USER <username> <hostname> <servername> <realname>.\n" ) ;
        Write( isock , buffer , strlen ( buffer ) );
        return ;
    }

    if ( ( strlen ( tokens[1] ) > 16 )
            ||( strlen ( tokens[2] ) > 16 )
            ||( strlen ( tokens[3] ) > 16 )
            ||( strlen ( tokens[4] ) > 16 )
            )
    {
        char buffer[128] ;
        sprintf ( buffer , ">> Error! Some argument is too long.\n" ) ;
        Write( isock , buffer , strlen ( buffer ) );
        return ;
    }
   
    client *curClient = client_list[isock] ;

    // Check whether the client is recorded
    if ( curClient == NULL )
    {
        char buffer[MAX_MSG_LEN] ;
        sprintf ( buffer , ">> Error! Cannot find the client record!\n");
        Write( isock , buffer , strlen ( buffer ) );
    }
    // Check whether the user info has already been set
    else if ( curClient -> user_is_set == 1 )
    {
        char buffer[MAX_MSG_LEN] ;
        sprintf ( buffer , ">> Error! You have already set your user info!\n");
        Write( isock , buffer , strlen ( buffer ) );
    }
    else
    {

        // if ( strlen ( tokens[1] ) > 16 )
        //     memcpy ( curClient -> username , tokens[1] , 16 ) ;    
        // else
        //     strcpy ( curClient -> username , tokens[1] ) ;

        // if ( strlen ( tokens[2] ) > 16 )
        //     memcpy ( curClient -> hostname , tokens[2] , 16 ) ;    
        // else
        //     strcpy ( curClient -> hostname , tokens[2] ) ;

        // if ( strlen ( tokens[3] ) > 16 )
        //     memcpy ( curClient -> servername , tokens[3] , 16 ) ;    
        // else
        //     strcpy ( curClient -> servername , tokens[3] ) ;

        // if ( strlen ( tokens[4] ) > 16 )
        //     memcpy ( curClient -> realname , tokens[4] , 16 ) ;    
        // else
        //     strcpy ( curClient -> realname , tokens[4] ) ;

        strcpy ( curClient -> username , tokens[1] ) ;
        strcpy ( curClient -> hostname , tokens[2] ) ;
        strcpy ( curClient -> servername , tokens[3] ) ;
        strcpy ( curClient -> realname , tokens[4] ) ;

        curClient -> user_is_set = 1 ;

        char buffer[128] ;
        sprintf ( buffer , ">> OK! You have successfully set your user info!\n" ) ;
        //Write( isock , buffer , strlen ( buffer ) );

        if ( strcmp ( curClient -> nickname , default_client_name ) != 0 )
        {
            //printf("miaowuwuwuwu!\n");
            // Tell the daemon
            char daemon_buffer[MAX_MSG_LEN] ;
            sprintf ( daemon_buffer , "ADDUSER %s\n" , curClient -> nickname );
            //printf("zcczcc %d %s",daemon_sockfd, daemon_buffer);
            Write( daemon_sockfd , daemon_buffer , strlen ( daemon_buffer ) );
            //printf("miaowuwuwuwu!\n");
        }
    }
    
}

void quit_handler ( int isock , char tokens[MAX_MSG_TOKENS][MAX_MSG_LEN + 1] , int commLen )
{
    // If the format of command is illegal, send err info back
    // if ( tokens[1] == NULL )
    // {
    //     char buffer[128] ;
    //     sprintf ( buffer , ">> The formant neet to be : QUIT <msg>.\n" ) ;
    //     Write( isock , buffer , strlen ( buffer ) );
    // }
    // Check whether the client is recorded
    // else 
    // {

    client *curClient = client_list[isock] ;

    if ( curClient == NULL )
    {
        char buffer[MAX_MSG_LEN] ;
        sprintf ( buffer , ">> Error! Cannot find the client record!\n");
        Write( isock , buffer , strlen ( buffer ) );
        return ;
    }
    
    channel *curChannel ;

    // If the client belongs to a channel, then leave the channel
    if ( ( curClient -> channel_id != -1 )
        && ( ( curChannel = channel_list[ curClient -> channel_id ] ) != NULL ) )
    {
        // Leave channel
        client_leave_channel ( isock , 
            channel_list [ curChannel -> channel_id ] -> name ) ;
    }

    // Log out of pool and client_list
    FD_CLR ( isock , & p -> read_set ) ;
    p -> clientfd[isock] = -1 ;
    client_list[isock] = NULL ;     // ?????????????????????????????
    client_sum -- ;                 // ?????????????????????????????
    Close ( isock ) ;
    printf( ">> Client of fd %d has quit!\n" , isock );

    if ( strcmp ( curClient -> nickname , default_client_name ) != 0 )
    {
        // Tell the daemon
        char daemon_buffer[MAX_MSG_LEN] ;
        sprintf ( daemon_buffer , "REMOVEUSER %s\n" , curClient -> nickname );
        Write( daemon_sockfd , daemon_buffer , strlen ( daemon_buffer ) );
    }
    //}
}

void join_handler ( int isock , char tokens[MAX_MSG_TOKENS][MAX_MSG_LEN + 1] , int commLen )
{
    // If the format of command is illegal, send err info back
    if ( commLen == 1 )
    {   
        char buffer[128] ;
        sprintf ( buffer , ">> The formant neet to be : JOIN <channel_name>.\n" ) ;
        Write( isock , buffer , strlen ( buffer ) );
        return ;
    }

    channel *joinChannel = get_channel_by_name ( tokens[1] ) ;
    client *curClient = client_list[isock] ;

    if ( strlen ( tokens[1] ) > 16 )
    {
        char buffer[128] ;
        sprintf ( buffer , ">> Error! Some argument is too long.\n" ) ;
        Write( isock , buffer , strlen ( buffer ) );
        printf("\n%s\n" , buffer) ;
        return ;
    }

    // If the user is in some channel at present
    if ( curClient -> channel_id != -1 ) 
    {
        // If the user is already in the channel he want to join, then refuse his requirement
        if ( ( joinChannel != NULL ) && ( curClient -> channel_id == joinChannel -> channel_id ) )
        {
            char buffer[MAX_MSG_LEN] ;
            sprintf ( buffer , ">> Error! You are already in the channel you want to join : %s\n" , tokens[1] ) ;
            Write( isock , buffer , strlen ( buffer ) );
            return ;
        }
        
        //Else, make the user leave its current channel
        char buffer[MAX_MSG_LEN] ;
        if ( client_leave_channel ( isock , channel_list [ curClient -> channel_id ] -> name ) != 1 )
        {                        
            sprintf ( buffer , ">> Error! Failed to leave your current channel : %s\n" , channel_list [ curClient -> channel_id ] -> name ) ;           
            return ;
        }
        else
        {
            sprintf ( buffer , ">> You have been forced to leave your current channel.\n" ) ;      
        }
        Write( isock , buffer , strlen ( buffer ) );

        // Clean
        curClient -> channel_id = -1 ;

    }

    // Then add the user to new channel

    char buffer[MAX_MSG_LEN] ;  // The return message

    // If the requested channel already exists    
    if ( joinChannel != NULL ) 
    {
        curClient -> channel_id = joinChannel -> channel_id ;
        joinChannel -> connected_clients [ isock ] = 1 ;
        joinChannel -> client_count ++ ;
        
        sprintf ( buffer , ">> OK! You have joined the channel : %s\n" , tokens[1] ) ;     

        // Say hello to all the others in this channel
        char buffer2 [MAX_MSG_LEN] ;
        sprintf ( buffer2 , ">> %s ( nickname : %s ) has joined this channel!\n" , curClient -> username , curClient -> nickname );
        int j ;
        for ( j = 0 ; j < FD_SETSIZE ; j ++ )
        {
            if ( ( j != isock )
                && ( joinChannel -> connected_clients[j] != 0 )
                && ( client_list[j] != NULL ) )
            {
                //Write( j , buffer2 , strlen ( buffer2 ) );
            }
        }  

    }
    // Else, no such channel at present, and then create it
    else
    {
        // Create a new channel and initiate it
        channel *newChannel = Malloc ( sizeof ( channel ) ) ;
        memset( newChannel , 0 , sizeof( channel ) ) ;
        newChannel -> connected_clients [ isock ] = 1 ;
        // If the name is too long, then cut it
        // if ( strlen ( tokens[1] ) > 16 )
        //     memcpy ( newChannel -> name , tokens[1] , 16 ) ;    
        // else
        //     strcpy ( newChannel -> name , tokens[1] ) ;    

        strcpy ( newChannel -> name , tokens[1] ) ;

        newChannel -> client_count = 1 ;

        // Add the new channel to channel_list
        int i ;
        for ( i = 0 ; i < FD_SETSIZE ; i ++ )
        {
            if ( channel_list[i] == NULL )
            {
                channel_list[i] = newChannel ;
                newChannel -> channel_id = i ;
                curClient -> channel_id = i ;
                channel_sum ++ ;
                break ;
            }
        }

        sprintf ( buffer , ">> OK! You have joined a NEW channel : %s\n" , tokens[1] ) ;
        joinChannel = newChannel ;

        // Tell the daemon
        char daemon_buffer[MAX_MSG_LEN] ;
        sprintf ( daemon_buffer , "ADDCHAN %s\n" , newChannel -> name );
        Write( daemon_sockfd , daemon_buffer , strlen ( daemon_buffer ) );
    }

    //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
    char hostname[255];
    gethostname(hostname, 255);
    char buf[2014];
    char buf2[2014];
    sprintf(buf, ":%s JOIN %s\n", curClient->nickname, joinChannel->name);
    sprintf(buf2,"%s:%s 353 %s = %s : %s\n", buf, hostname, curClient->nickname, joinChannel->name, curClient->nickname);
    sprintf(buf2,"%s:%s 366 %s %s :End of /NAMES list\n", buf2, hostname, curClient->nickname, joinChannel->name);
    Write(isock, buf2, strlen(buf2));
    int j ;
    for(j = 0; j < FD_SETSIZE; j++){
        if( j != isock && joinChannel->connected_clients[j] != 0){
            Write(j, buf, strlen(buf));
        }
    }
    //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

    //Write( isock , buffer , strlen ( buffer ) );
}

void part_handler ( int isock , char tokens[MAX_MSG_TOKENS][MAX_MSG_LEN + 1] , int commLen )
{
    // If the format of command is illegal, send err info back
    if ( commLen == 1 )
    {   
        char buffer[128] ;
        sprintf ( buffer , ">> The formant neet to be : PART <name1>,<name2>, ... ,<nameN>.\n" ) ;
        Write( isock , buffer , strlen ( buffer ) );
        return ;
    }    

    client *curClient = client_list[isock] ;

    // Check if the client is recorded
    if ( curClient == NULL )
    {
        char buffer[MAX_MSG_LEN] ;
        sprintf ( buffer , ">> Error! Cannot find the client record!\n");
        Write( isock , buffer , strlen ( buffer ) );
        return ;
    }

    // Analyze each channel, and leave them one by one
    char *channel_names_list = tokens[1] ;
    char *channel_name ;
    while ( ( channel_name = strsep ( &channel_names_list , "," ) ) != NULL )
    {
        channel *curChannel = get_channel_by_name ( channel_name ) ;

        // If the channel does not exist
        if ( curChannel == NULL )
        {
            char buffer[MAX_MSG_LEN] ;
            //sprintf ( buffer , ">> Error! No such channel : %s\n" , channel_name );
            sprintf(buffer, ":%s!%s@%s QUIT :ERR_NOSUCHCHANNEL: %s does not exist.\n", curClient->nickname, curClient->username, curClient->hostname, channel_name);
            Write( isock , buffer , strlen ( buffer ) );
        }
        // Else if the channel exists but the client is not in it
        else if ( curChannel -> channel_id != curClient -> channel_id )
        {
            char buffer[MAX_MSG_LEN] ;
            //sprintf ( buffer , ">> Error! You are not in this channel : %s\n" , channel_name );
            sprintf(buffer, ":%s!%s@%s QUIT :The client is not in the %s.\n", curClient->nickname, curClient->username, curClient->hostname, channel_name);
            Write( isock , buffer , strlen ( buffer ) );
        }
        // Else, the channel exists and the client is in it
        else
        {
            char buffer[MAX_MSG_LEN] ;
            sprintf(buffer, ":%s!%s@%s QUIT :The client is not in the %s.\n", curClient->nickname, curClient->username, curClient->hostname, channel_name);
            Write( isock , buffer , strlen ( buffer ) );

            if ( client_leave_channel ( isock , channel_name ) != 1 )
            {
                char buffer2[MAX_MSG_LEN] ;
                sprintf ( buffer2 , ">> Error! Failed to leave this channel : %s\n" , channel_name ) ;
                Write( isock , buffer2 , strlen ( buffer2 ) );
            }

            curClient -> channel_id = -1 ;
        }
    }
}

void list_handler ( int isock , char tokens[MAX_MSG_TOKENS][MAX_MSG_LEN + 1] , int commLen )
{
    client *curClient = client_list[isock] ;

    // Check if the client is recorded
    if ( curClient == NULL )
    {
        char buffer[MAX_MSG_LEN] ;
        sprintf ( buffer , ">> Error! Cannot find the client record!\n");
        Write( isock , buffer , strlen ( buffer ) );
        return ;
    }

    char buffer [MAX_MSG_LEN] ;

    sprintf( buffer , ">> The list of channels and its user number:\n" );
    int i ;
    for ( i = 0 ; i < FD_SETSIZE ; i ++ )
    {
        if ( channel_list[i] != NULL )
        {
            // If the client is in this channel, add a "*" at last
            if ( channel_list[i] -> channel_id == curClient -> channel_id )
            {
                sprintf( buffer , "%s# %s\t%d\t*\n" , buffer , channel_list[i] -> name , channel_list[i] -> client_count );
            }
            else
            {
                sprintf( buffer , "%s# %s\t%d\n" , buffer , channel_list[i] -> name , channel_list[i] -> client_count );
            }
        }
    }

    // Add an end of list
    sprintf( buffer , "%s>> END OF LIST\n" , buffer );

    //Write( isock , buffer , strlen ( buffer ) );

    //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
    char buf[1024];
    char hostname[255];
    gethostname(hostname, 255);

    // Write head
    sprintf(buf, ":%s 321 %s Channel :Users Name\n", hostname, curClient->nickname);
    int j;

    /* List all existing channels on the local server. */
    for(j = 0; j < FD_SETSIZE; j++){    
        if(channel_list[j] != NULL){
            sprintf(buf, "%s:%s 322 %s %s :%d\n", buf, hostname, curClient->nickname, channel_list[j]->name, channel_list[j]->client_count);
        }
    }

    /* List all existing channels on the remote servers. */
    rio_t rio;
    Rio_readinitb ( &rio , daemon_sockfd ) ;
    int len ;
    // Ask the daemon for channel table
    char daemon_buffer[MAX_MSG_LEN] ;
    sprintf ( daemon_buffer , "CHANTABLE\n" );
    Write( daemon_sockfd , daemon_buffer , strlen ( daemon_buffer ) );
    // Receive the response from daemon
    if ( ( len = Rio_readlineb ( &rio , daemon_buffer , MAX_MSG_LEN ) ) != 0 )
    {
        char daemon_message [MAX_MSG_LEN] ;
        if ( get_msg ( daemon_buffer , daemon_message ) == -1 )
        {
            printf( ">> Error! Daemon_message is too long!\n" );
            return ;
        }
        char daemon_tokens [MAX_MSG_TOKENS][MAX_MSG_LEN + 1];                 
        int daemon_commLen = tokenize(daemon_message, daemon_tokens) ;
        if ( daemon_commLen != 2 )
        {
            printf( ">> Error! Daemon_message is in valid format!\n" );
            return ;
        }
        char *ok = daemon_tokens[0] ;
        if ( strncasecmp( ok , "OK" , sizeof("OK") ) != 0 )
        {
            printf( ">> Error! Daemon_message is not start with \"OK\"!\n" );
            return ;
        }

        int size = atoi(daemon_tokens[1]) ;
        int j ;
        for ( j = 0 ; j < size ; j ++ )
        {
            if ( ( len = Rio_readlineb ( &rio , daemon_buffer , MAX_MSG_LEN ) ) != 0 )
            {
                get_msg ( daemon_buffer , daemon_message ) ; 
                tokenize(daemon_message, daemon_tokens) ;
                sprintf(buf, "%s:%s 322 %s %s :?\n", buf, daemon_tokens[1], curClient->nickname, daemon_tokens[0]);        
            }
        }        
    }

    // Write tail
    sprintf(buf, "%s:%s 323 %s :End of /LIST\n", buf, hostname, curClient->nickname);
    Write(isock, buf, strlen(buf));
    //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
}

void privmsg_handler ( int isock , char tokens[MAX_MSG_TOKENS][MAX_MSG_LEN + 1] , int commLen )
{
    // If the format of command is illegal, send err info back
    if ( commLen <= 2 )
    {   
        char buffer[128] ;
        sprintf ( buffer , ">> The formant neet to be : PRIVMSG <target>,<target>, ... ,<target> <msg>.\n" ) ;
        Write( isock , buffer , strlen ( buffer ) );
        return ;
    }    

    client *curClient = client_list[isock] ;

    // Check if the client is recorded
    if ( curClient == NULL )
    {
        char buffer[MAX_MSG_LEN] ;
        sprintf ( buffer , ">> Error! Cannot find the client record!\n");
        Write( isock , buffer , strlen ( buffer ) );
        return ;
    }

    // Analyze each target, and send message to them one by one
    char *message = tokens[2] ;        
    char *targets_list = tokens[1] ;
    char *target ;  
    while ( ( target = strsep ( &targets_list , "," ) )  != NULL )
    {
        // Search all nicknames which is equal to target locally
        int i ;
        for ( i = 0 ; i < FD_SETSIZE ; i ++ )
        {
            if ( ( client_list[i] != NULL )
                && ( strcmp ( client_list[i] -> nickname , target ) == 0 ) )
            {
                char buffer[MAX_MSG_LEN] ;
                sprintf ( buffer , ":%s PRIVMSG %s :%s\n" , curClient -> nickname , target , message );
                Write( i , buffer , strlen ( buffer ) );
            }
        }

        // Search all nicknames in remote servers
        rio_t rio;
        Rio_readinitb ( &rio , daemon_sockfd ) ;
        int len ;
        char daemon_buffer[MAX_MSG_LEN] ;
        sprintf ( daemon_buffer , "NEXTHOP %s\n" , target );
        Write( daemon_sockfd , daemon_buffer , strlen ( daemon_buffer ) );
        // Receive the response from daemon
        if ( ( len = Rio_readlineb ( &rio , daemon_buffer , MAX_MSG_LEN ) ) != 0 )
        {
            char daemon_message [MAX_MSG_LEN] ;
            if ( get_msg ( daemon_buffer , daemon_message ) == -1 )
            {
                printf( ">> Error! Daemon_message is too long!\n" );
                return ;
            }
            char daemon_tokens [MAX_MSG_TOKENS][MAX_MSG_LEN + 1];                 
            tokenize(daemon_message, daemon_tokens) ;
            char *ok = daemon_tokens[0] ;
            // If find
            if ( strncasecmp( ok , "OK" , sizeof("OK") ) != 0 )
            {
                int nexthop = atoi(daemon_tokens[1]) ;
                // Send user_forwarding command to remote user
                forward_to_user ( curClient -> nickname , target , nexthop , message ) ;
            }
        }

        // Search all channel names which is equal to target locally
        channel *curChannel = get_channel_by_name ( target ) ;
        if ( curChannel != NULL )
        {
            int j ;
            for ( j = 0 ; j < FD_SETSIZE ; j ++ )
            {
                if ( ( j != isock )
                    && ( curChannel -> connected_clients [j] == 1 ) )
                {
                    char buffer[MAX_MSG_LEN] ;
                    sprintf ( buffer , ":%s PRIVMSG %s :%s\n" , curClient -> nickname , target , message );
                    Write( j , buffer , strlen ( buffer ) );
                }
            }
        }

        // Search all channel names which is equal to target in remote servers
        sprintf ( daemon_buffer , "NEXTHOPS %lu %s\n" , curr_nodeID , target );
        Write( daemon_sockfd , daemon_buffer , strlen ( daemon_buffer ) );
        // Receive the response from daemon
        if ( ( len = Rio_readlineb ( &rio , daemon_buffer , MAX_MSG_LEN ) ) != 0 )
        {
            char daemon_message [MAX_MSG_LEN] ;
            if ( get_msg ( daemon_buffer , daemon_message ) == -1 )
            {
                printf( ">> Error! Daemon_message is too long!\n" );
                return ;
            }
            char daemon_tokens [MAX_MSG_TOKENS][MAX_MSG_LEN + 1];                 
            int daemon_commLen = tokenize(daemon_message, daemon_tokens) ;
            char *ok = daemon_tokens[0] ;
            // If find
            if ( strncasecmp( ok , "OK" , sizeof("OK") ) != 0 )
            {
                int j ;
                for ( j = 1 ; j < daemon_commLen ; j ++ )
                {
                    int nexthop = atoi(daemon_tokens[j]) ;
                    // Send channel_forwarding command to remote channel
                    forward_to_channel( (int)curr_nodeID , curClient -> nickname , target , nexthop , message ) ;
                }    
            }
        }
    }

}

void who_handler ( int isock , char tokens[MAX_MSG_TOKENS][MAX_MSG_LEN + 1] , int commLen )
{
    // If the format of command is illegal, send err info back
    if ( commLen == 1 )
    {   
        char buffer[128] ;
        sprintf ( buffer , ">> The formant neet to be : WHO <channel_name>.\n" ) ;
        Write( isock , buffer , strlen ( buffer ) );
        return ;
    }
    
    client *curClient = client_list[isock] ;

    if ( curClient == NULL )
    {
        char buffer[MAX_MSG_LEN] ;
        sprintf ( buffer , ">> Error! Cannot find the client record!\n");
        Write( isock , buffer , strlen ( buffer ) );
        return ;
    }
    
    channel *curChannel = get_channel_by_name ( tokens[1] ) ;
    
    // If the client belongs to a channel, then list the channel
    if ( curChannel != NULL ) 
    {
        char buffer[MAX_MSG_LEN] ;
        sprintf ( buffer , ">> List of users in this channel:\n");

        int i ;
        for ( i = 0 ; i < FD_SETSIZE ; i ++ )
        {
            if ( curChannel -> connected_clients[i] != 0 )
            {
                sprintf ( buffer , "%s# %s ( nickname: %s )\n" , buffer , client_list[i] -> username , client_list[i] -> nickname );
            }
        }

        // Add an end of list
        sprintf( buffer , "%s>> END OF LIST\n" , buffer );

        //Write( isock , buffer , strlen ( buffer ) );

        //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        char hostname[255];
        gethostname(hostname, 255);
        char buf[1024];
        sprintf(buf, ":%s 352 %s %s please look out: ", hostname, curClient->nickname, curChannel->name);
        int j;
        client *tmp = NULL;
        for(j = 0; j < FD_SETSIZE; j++){
            if(curChannel->connected_clients[j] != 0){
                tmp = client_list[j];
                sprintf(buf, "%s %s", buf, tmp->nickname);
            }
        }
        sprintf(buf,"%s H :0 The MOTD\n", buf); /* The message is come from the ruby script */
        sprintf(buf,"%s:%s 315 %s %s :End of /WHO list\n", buf, hostname, curClient->nickname, curChannel->name);
        Write(isock, buf, strlen(buf));
        //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

    }
    else
    {
        char buffer[MAX_MSG_LEN] ;
        sprintf ( buffer , ">> Error! Cannot find the channel locally: %s\n" , tokens[1] );
        Write( isock , buffer , strlen ( buffer ) );
    }

}

// commander handler for pj2

/*
 * Handle USERFORWARD command
 * Format: USERFORWARD <source_user_nickname> <target_user_nickname> <msg>
 */
void user_forwarding_handler ( char tokens[MAX_MSG_TOKENS][MAX_MSG_LEN + 1] , int commLen )
{
    // If the format of command is illegal, be silent
    if ( commLen <= 3 )
    {   
        printf ( ">> The formant neet to be : USERFORWARD <source_user_nickname> <target_user_nickname> <msg>.\n" ) ;
        return ;
    }  
    else
    {
        char *source_user_nickname = tokens[1] ;
        char *target_user_nickname = tokens[2] ;
        char *message = tokens[3] ;

        // Search all nicknames which is equal to target locally, if find, send message to it and return 
        int i ;
        for ( i = 0 ; i < FD_SETSIZE ; i ++ )
        {
            if ( ( client_list[i] != NULL )
                && ( strcmp ( client_list[i] -> nickname , target_user_nickname ) == 0 ) )
            {
                char buffer[MAX_MSG_LEN] ;
                sprintf ( buffer , ":%s PRIVMSG %s :%s\n" , source_user_nickname , target_user_nickname , message );
                Write( i , buffer , strlen ( buffer ) );
                return ;
            }
        }

        // Search all nicknames in remote servers
        rio_t rio;
        Rio_readinitb ( &rio , daemon_sockfd ) ;
        int len ;
        char daemon_buffer[MAX_MSG_LEN] ;
        sprintf ( daemon_buffer , "NEXTHOP %s\n" , target_user_nickname );
        Write( daemon_sockfd , daemon_buffer , strlen ( daemon_buffer ) );
        // Receive the response from daemon
        if ( ( len = Rio_readlineb ( &rio , daemon_buffer , MAX_MSG_LEN ) ) != 0 )
        {
            char daemon_message [MAX_MSG_LEN] ;
            if ( get_msg ( daemon_buffer , daemon_message ) == -1 )
            {
                printf( ">> Error! Daemon_message is too long!\n" );
                return ;
            }
            char daemon_tokens [MAX_MSG_TOKENS][MAX_MSG_LEN + 1];                 
            tokenize(daemon_message, daemon_tokens) ;
            char *ok = daemon_tokens[0] ;
            // If find
            if ( strncasecmp( ok , "OK" , sizeof("OK") ) != 0 )
            {
                int nexthop = atoi( daemon_tokens[1] ) ;
                // Send user_forwarding command to remote user                
                forward_to_user ( source_user_nickname , target_user_nickname , nexthop , message ) ;
            }
        }
    }
}

/*
 * Handle CHANNELFORWARD command
 * Format: CHANNELFORWARD <source_nodeID> <source_user_nickname> <target_channel_name> <msg>
 */
void channel_forwarding_handler ( char tokens[MAX_MSG_TOKENS][MAX_MSG_LEN + 1] , int commLen )
{
    // If the format of command is illegal, be silent
    if ( commLen <= 4 )
    {   
        printf ( ">> The formant neet to be : CHANNELFORWARD <source_nodeID> <source_user_nickname> <target_channel_name> <msg>.\n" ) ;
        return ;
    }  
    else
    {
        int source_nodeID = atoi(tokens[1]) ;
        char *source_user_nickname = tokens[2] ;
        char *target_channel_name = tokens[3] ;
        char *message = tokens[4] ;

        // Search all channel names which is equal to target locally
        channel *curChannel = get_channel_by_name ( target_channel_name ) ;
        if ( curChannel != NULL )
        {
            int j ;
            for ( j = 0 ; j < FD_SETSIZE ; j ++ )
            {
                if ( curChannel -> connected_clients [j] == 1 )
                {
                    char buffer[MAX_MSG_LEN] ;
                    sprintf ( buffer , ":%s PRIVMSG %s :%s\n" , source_user_nickname , target_channel_name , message );
                    Write( j , buffer , strlen ( buffer ) );
                }
            }
        }

        // Search all channel names which is equal to target in remote servers
        rio_t rio;
        Rio_readinitb ( &rio , daemon_sockfd ) ;
        int len ;
        char daemon_buffer[MAX_MSG_LEN] ;
        sprintf ( daemon_buffer , "NEXTHOPS %d %s\n" , source_nodeID , target_channel_name );
        Write( daemon_sockfd , daemon_buffer , strlen ( daemon_buffer ) );
        // Receive the response from daemon
        if ( ( len = Rio_readlineb ( &rio , daemon_buffer , MAX_MSG_LEN ) ) != 0 )
        {
            char daemon_message [MAX_MSG_LEN] ;
            if ( get_msg ( daemon_buffer , daemon_message ) == -1 )
            {
                printf( ">> Error! Daemon_message is too long!\n" );
                return ;
            }
            char daemon_tokens [MAX_MSG_TOKENS][MAX_MSG_LEN + 1];                 
            int daemon_commLen = tokenize(daemon_message, daemon_tokens) ;
            char *ok = daemon_tokens[0] ;
            // If find
            if ( strncasecmp( ok , "OK" , sizeof("OK") ) != 0 )
            {
                int j ;
                for ( j = 1 ; j < daemon_commLen ; j ++ )
                {
                    int nexthop = atoi( daemon_tokens[j] ) ;
                    // Send channel_forwarding command to remote channel
                    forward_to_channel( source_nodeID , source_user_nickname , target_channel_name , nexthop , message ) ;
                }    
            }
        }
    }
}

/*
 * forward a message to a remote user
 */
void forward_to_user ( char *source_user_nickname , char *target_user_nickname , int nexthop , char *msg ) 
{
    //u_long curr_nodeID;
    //rt_config_file_t   curr_node_config_file;  /* The config_file  for this node */
    //rt_config_entry_t *curr_node_config_entry; /* The config_entry for this node */

    rt_config_entry_t *nexthop_config_entry; /* The config_entry for node of next hop*/

    int i ;
    for( i = 0; i < curr_node_config_file.size; ++i )
    {    
        if( (int)curr_node_config_file.entries[i].nodeID == nexthop )
        {            
            nexthop_config_entry = &curr_node_config_file.entries[i];
            char ip_temp[20];
            sprintf(ip_temp,"%d", (int)(nexthop_config_entry -> ipaddr) );   
            int nexthop_sockfd = Open_clientfd( ip_temp , nexthop_config_entry -> local_port );

            // Generate message which will be sent to next hop
            char buffer[MAX_MSG_LEN] ;
            sprintf ( buffer , "USERFORWARD %s %s %s\n" , source_user_nickname , target_user_nickname , msg );
            Write( nexthop_sockfd , buffer , strlen ( buffer ) );

            break ;
        }
    }
}

/*
 * forward a message to a channel on a remote server
 */
void forward_to_channel( int source_nodeID , char *source_user_nickname , char *target_channel_name , int nexthop , char *msg )
{
    rt_config_entry_t *nexthop_config_entry; /* The config_entry for node of next hop*/

    int i ;
    for( i = 0; i < curr_node_config_file.size; ++i )
    {    
        if( (int)curr_node_config_file.entries[i].nodeID == nexthop )
        {            
            nexthop_config_entry = &curr_node_config_file.entries[i];
            char ip_temp[20];
            sprintf(ip_temp,"%d", (int)(nexthop_config_entry -> ipaddr) );   
            int nexthop_sockfd = Open_clientfd( ip_temp , nexthop_config_entry -> local_port );

            // Generate message which will be sent to next hop
            char buffer[MAX_MSG_LEN] ;
            sprintf ( buffer , "CHANNELFORWARD %d %s %s %s\n" , (int)curr_nodeID ,
                    source_user_nickname , target_channel_name , msg );
            Write( nexthop_sockfd , buffer , strlen ( buffer ) );

            break ;
        }
    }
}

// get a channel struct by its channel_name
channel *get_channel_by_name ( char *channel_name )
{
    int i ;
    for ( i = 0 ; i < FD_SETSIZE ; i ++ )
    {
        if ( channel_list[i] != NULL ) 
        {
            if ( strcmp ( channel_list[i] -> name , channel_name ) == 0 )
            {
                return channel_list[i] ;
            }
        }
    }
    return NULL ;
}

/* 
delete the client from the channel. Then check if the channel is empty, if so, delete the channel from the global channel_list. If not, then say bodbye to all others.
*/
int client_leave_channel ( int isock , char *channel_name ) 
{
    client *curClient = client_list[isock] ;
    channel *curChannel = get_channel_by_name ( channel_name ) ;

    // check whether the client and channel are exist
    if ( ( curClient == NULL ) || ( curChannel == NULL ) )
    {
        return 0 ;
    }

    // delete the client from the channel
    curChannel -> connected_clients[isock] = 0 ;
    curChannel -> client_count -- ;

    // if then the channel is empty, then delete the channel
    if ( curChannel -> client_count == 0 )
    {
        int i ;
        for ( i = 0 ; i < FD_SETSIZE ; i ++ )
        {
            if ( ( channel_list[i] != NULL ) 
                && ( channel_list[i] -> channel_id == curChannel -> channel_id ) )
            {
                channel_list[i] = NULL ;
                channel_sum -- ;
                break ;
            }
        }

        // Tell the daemon
        char daemon_buffer[MAX_MSG_LEN] ;
        sprintf ( daemon_buffer , "REMOVECHAN %s\n" , channel_name );
        Write( daemon_sockfd , daemon_buffer , strlen ( daemon_buffer ) );
    }
    // else, there are others in channel, then say godbye
    else
    {
        // Say goodbye to other members of current channel
        char buffer[MAX_MSG_LEN] ;
        //sprintf ( buffer , ">> %s ( nickname : %s ) has left this channel!\n" , curClient -> username , curClient -> nickname );
        sprintf(buffer, ":%s!%s@%s QUIT :The client is not in the %s.\n", curClient->nickname, curClient->username, curClient->hostname, channel_name);
        int i ;
        for ( i = 0 ; i < FD_SETSIZE ; i ++ )
        {
            if ( ( curChannel -> connected_clients[i] != 0 )
                && ( client_list[i] != NULL ) )
            {
                Write( i , buffer , strlen ( buffer ) );
            }
        }
    }

    return 1 ;
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
