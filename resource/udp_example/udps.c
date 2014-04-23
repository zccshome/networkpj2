/*
 * 15-441:  Example UDP client/server
 *
 * Includes: extra hints on how to do LSA packets :P
 *
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#define PORT 1500
#define MAX_ENTRIES 6 // something small for now, not realistic

struct packet {
	int seq_num;
	int sourceNode;
	int numEntries;
	char entries[MAX_ENTRIES][16];
};

int main() {
	int sockfd, clilen, i;
	struct sockaddr_in serv_addr, cli_addr;
	struct packet incomingPacket;  // Temporary place to store incoming packet

	// setup the socket
	sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if(sockfd < 0) {
		printf("error on socket() call");
		return -1;
	}

	// Setup the server address and bind to it
	memset(&serv_addr, '\0', sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(PORT);
	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
		printf("error binding socket\n");
		return -1;
	}

	// Clear the memory in the incoming packet
	memset(&incomingPacket, '\0', sizeof(incomingPacket));

	clilen=sizeof(cli_addr);

	// NOTE: YOU MUST USE RT_RECVFROM INSTEAD
	recvfrom(sockfd, &incomingPacket, sizeof(incomingPacket), 0, (struct sockaddr *)&cli_addr, &clilen);

	// Parse the message
	printf("\n\nServer got a message from port %d\n", ntohs(cli_addr.sin_port));
	printf("\tSequence_Num: %d\n\tsourceNode: %d\n\tnumEntries: %d\n\tPayload:\n",
			incomingPacket.seq_num, incomingPacket.sourceNode, incomingPacket.numEntries);
	for(i=0; i < incomingPacket.numEntries; i++)
		printf("\t\t%s\n", incomingPacket.entries[i]);

	// Lets echo it back for fun...
	sendto(sockfd, &incomingPacket, sizeof(incomingPacket), 0, (struct sockaddr *)&cli_addr, sizeof(cli_addr));

	return 1;
}
