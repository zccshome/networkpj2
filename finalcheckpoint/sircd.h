
typedef struct s_pool {
    int maxfd; 		// largest descriptor in sets
    fd_set read_set; 	// all active read descriptors
    fd_set ready_set;	// descriptors ready for reading
    fd_set write_set; 	// all active write descriptors      // do not need ? ///
    int nready;		// return of select()
    int clientfd[FD_SETSIZE];	// max index in client array // set of readfds ? 
    // might want to write this
    rio_t client_read_buf[FD_SETSIZE];	
    // what else might be helpful for project 1?
} pool;

typedef struct s_client{
	int clientfd;
	char nickname[16];
	char username[16];
	char hostname[16];
	char servername[16];
	char realname[16];
	int channel_id;			// the channel which the client is in
	//int nick_is_set;		// whether the nickname of this client is be set
	int user_is_set;		// whether this client is registered
} client;


typedef struct s_channel{
	char name[16];
	int channel_id;
	int connected_clients[FD_SETSIZE];	// the connected clients' clientfd in this channel
	//int clientfd_list[FD_SETSIZE];	// the clientfd in this channel
	//int isActive;		// whether this channel is active(!0) or not(0)
	int client_count;	// the client count in this channel
} channel;