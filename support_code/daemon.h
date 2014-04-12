/* Macros */
#define MAX_MSG_TOKENS 10
#define MAX_MSG_LEN 512

#define max_node_num 1024
#define max_username_num 16
#define max_channelname_num 16

/*
 * Each link entry contains the nodeID of a node that is directly connected to the sender. This field is 4 bytes.
 */
typedef struct link_entries_struct
{
	rt_config_entry_t node_entry;
} link_entries;

/*
 * Each user entry contains the name of the destination user as a null terminated string. 
 * Since the IRC RFC indicates that nicknames should be at most 9 characters and 
 * we have added the constraint that channels can be at most 9 characters (including & or #), 
 * it should definitely fit within 16 (the unused bytes will be ignored).
 */
typedef struct user_entries_struct
{
	char username[max_username_num];
} user_entries;

/*
 * Same as a user entry, above.
 */
typedef struct channel_entries_struct
{
	char channelname[max_channelname_num];
} channel_entries;

/*
 * A structure for the daemon to exchange LSAs.
 */
typedef struct daemon_node_struct
{
	int version; // the protocol version, always set to 1
	int TTL; // the time to live of the LSA. It is decremented each hop during flooding, and is initially set to 32.
	int type; //Advertisement packets should be type 0 and Acknowledgement packets should be type 1.
	int sender_nodeID; //The nodeID of the sender of the message, not the immediate sender.
	int sequence_number; //The sequence number given to each message
	int num_link_entries; //The number of link table entries in the message.
	int num_user_entries; //The numbers of users announced in the message
	int num_channel_entries; //The number of channels announced in the message
	link_entries l_entries[max_node_num];
	user_entries u_entries[max_node_num];
	channel_entries channel_entries[max_node_num];
} daemon_node;

typedef struct daemon_route_struct
{
	int end_nodeID;
	//int path[max_node_num];
	int nexthop;
} daemon_route;

typedef struct daemon_routes_struct
{
	int start_nodeID;
	daemon_route d_route[max_node_num];
} daemon_routes;

void setup_daemon();
void setup_route();
void build_route();
int setup_socket(int port);
void init_node( int argc, char *argv[] );
size_t get_msg( char *buf, char *msg );
int tokenize( char const *in_buf, char tokens[MAX_MSG_TOKENS][MAX_MSG_LEN+1] );
int init();
//daemon_node wait_for_event();