/* Copyright (C) 2023 Elijah Day

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the “Software”), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE. */

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

