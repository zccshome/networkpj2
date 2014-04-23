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

#define PORT 1501
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
	struct packet outgoingPacket, incomingPacket;
	struct hostent *h;

	sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if(sockfd < 0) {
		printf("error on socket() call");
		return -1;
	}

	memset(&serv_addr, '\0', sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(PORT);

	/* attempt to bind the socket */
	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
		printf("error binding socket\n");
		return -1;
	}

	// Lets set up the structure for who we want to contact
	if((h = gethostbyname("localhost"))==NULL) {
		printf("error resolving host\n");
		return -1;
	}

	memset(&cli_addr, '\0', sizeof(cli_addr));
	cli_addr.sin_family = AF_INET;
	cli_addr.sin_addr.s_addr = *(in_addr_t *)h->h_addr;
	cli_addr.sin_port = htons(1500);

	memset(&outgoingPacket, '\0', sizeof(outgoingPacket));

	outgoingPacket.seq_num=1;
	outgoingPacket.sourceNode=15441;
	outgoingPacket.numEntries=2;
	snprintf(outgoingPacket.entries[0], sizeof(outgoingPacket.entries[0]), "%s", "George");
	snprintf(outgoingPacket.entries[1], sizeof(outgoingPacket.entries[1]), "%s", "Dave");

	// NOTE: YOU MUST USE RT_RECVFROM INSTEAD
	clilen=sizeof(cli_addr);
	sendto(sockfd, &outgoingPacket, sizeof(outgoingPacket), 0, (struct sockaddr *)&cli_addr, sizeof(cli_addr));
	recvfrom(sockfd, &incomingPacket, sizeof(incomingPacket), 0, (struct sockaddr *)&cli_addr, &clilen);
	printf("\n\nClient got a message from port %d\n", ntohs(cli_addr.sin_port));
	printf("\tSequence_Num: %d\n\tsourceNode: %d\n\tnumEntries: %d\n\tPayload:\n",
			incomingPacket.seq_num, incomingPacket.sourceNode, incomingPacket.numEntries);
	for(i=0; i < incomingPacket.numEntries; i++)
		printf("\t\t%s\n", incomingPacket.entries[i]);

	return 1;
}
