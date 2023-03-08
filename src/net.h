#ifndef NET_H
#define NET_H

#include "SDL2/SDL_net.h"
#include "sodium.h"
#include "stdbool.h"

#define MAX_MSG_LEN 256
#define MAX_USERNAME_LEN 16

#define CIPHERTEXT_LEN MAX_MSG_LEN + crypto_box_MACBYTES

/* Struct for encrypted messages and their accompanying data (both sending
and receiving) */
typedef struct msg_data_t
{
	unsigned char pubkey[crypto_box_PUBLICKEYBYTES];
	unsigned char nonce[crypto_box_NONCEBYTES];
	unsigned char ciphertext[CIPHERTEXT_LEN];	
}
msg_data_t;

/* Struct for each client's data (including the socket) */
typedef struct client_t
{
	bool is_connected;
	char username[MAX_USERNAME_LEN];
	TCPsocket socket;
}
client_t;

bool setup_server_connection
(
	IPaddress *server_ip,
	TCPsocket *server_socket,
	const char *hostname,
	int port
);

bool init_client_socket_set
(
	SDLNet_SocketSet *socket_set,
	TCPsocket *server_socket
);

void init_client_arr(client_t *client_arr, int client_cnt);

void send_msg
(
	const char *msg,
	msg_data_t *msg_data,
	TCPsocket *server_socket,
	unsigned char privkey[crypto_box_SECRETKEYBYTES],
	unsigned char pubkey[crypto_box_PUBLICKEYBYTES]
);

void recv_msg
(
	char *msg,
	msg_data_t *msg_data,
	TCPsocket *server_socket,
	unsigned char privkey[crypto_box_SECRETKEYBYTES],
	unsigned char pubkey[crypto_box_PUBLICKEYBYTES]
);

#endif /* NET_H */

