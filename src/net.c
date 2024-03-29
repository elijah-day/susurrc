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

#include "SDL2/SDL_net.h"
#include "src/err.h"
#include "src/net.h"
#include "src/susurrc.h"
#include "stdbool.h"
#include "stdlib.h"
#include "string.h"

bool setup_server_connection
(
	IPaddress *server_ip,
	TCPsocket *server_socket,
	const char *hostname,
	int port
)
{
	bool setup_success = true; 
	
	/* Attempt to determine a remote host IP given a hostname OR create a
	server given a NULL hostname */
	if(SDLNet_ResolveHost(server_ip, hostname, port) != 0)
	{
		print_err("SDLNet_ResolveHost", "Could not resolve the host");
		setup_success = false;
	}
	
	*server_socket = NULL;
	
	/* Attempt to open the server socket if the host was successfuly
	resolved */
	if(setup_success)
		*server_socket = SDLNet_TCP_Open(server_ip);
		
	if(*server_socket == NULL)
	{
		print_err("SDLNet_TCP_Open", "Could not open the server socket");
		setup_success = false;
	}
	
	return setup_success;
}

bool init_client_socket_set
(
	SDLNet_SocketSet *socket_set,
	TCPsocket *server_socket
)
{
	bool init_success = true;

	*socket_set = NULL;
	*socket_set = SDLNet_AllocSocketSet(1);
	
	if(*socket_set == NULL)
	{
		print_libsdl_err("SDLNet_AllocSocketSet");
		init_success = false;
	}
	
	if(init_success)
		if(SDLNet_TCP_AddSocket(*socket_set, *server_socket) < 0)
		{
			print_libsdl_err("SDLNet_TCP_AddSocket");
			init_success = false;
		}
	
	return init_success;
}

bool init_server_socket_set
(
	SDLNet_SocketSet *socket_set,
	TCPsocket *server_socket
)
{
	bool init_success = true;
	
	/* One plus the max client count to account for the maximum number of
	clients AND the server socket */
	*socket_set = NULL;
	*socket_set = SDLNet_AllocSocketSet(1 + MAX_CLIENT_CNT);
	
	if(*socket_set == NULL)
	{
		print_libsdl_err("SDLNet_AllocSocketSet");
		init_success = false;
	}
	
	if(init_success)
		if(SDLNet_TCP_AddSocket(*socket_set, *server_socket) < 0)
		{
			print_libsdl_err("SDLNet_TCP_AddSocket");
			init_success = false;
		}
	
	return init_success;
}

void init_client_arr(client_t *client_arr, int client_cnt)
{
	for(int i = 0; i < client_cnt; i++)
	{
		client_arr[i].is_logged_in = false;
		strcpy(client_arr[i].username, "user");
		client_arr[i].socket = NULL;
	}
}

void send_msg
(
	const char *msg,
	msg_data_t *msg_data,
	TCPsocket *server_socket,
	unsigned char privkey[crypto_box_SECRETKEYBYTES],
	unsigned char pubkey[crypto_box_PUBLICKEYBYTES]
)
{
	/* Abort on an empty message (results in a strange looping behavior
	otherwise) */
	if(strcmp(msg, "") == 0) return;
	
	/* Create a dummy buffer for prompting the receiver */
	const char dummy_buf[MAX_MSG_LEN] = "DUMMY";
	
	/* Prompt the receiver to send its public key */
	SDLNet_TCP_Send
	(
		*server_socket,
		dummy_buf,
		MAX_MSG_LEN
	);
	
	/* Receive the receiver's public key */
	SDLNet_TCP_Recv
	(
		*server_socket,
		msg_data,
		sizeof(*msg_data)
	);
	
	/* Generate the nonce and encrypt the message.  Store it in the message
	data */
	randombytes_buf
	(
		msg_data->nonce,
		sizeof(msg_data->nonce)
	);
	
	int encryption_return = crypto_box_easy
	(
		msg_data->ciphertext,
		(unsigned char *)msg,
		MAX_MSG_LEN,
		msg_data->nonce,
		msg_data->pubkey,
		privkey
	);
	
	if(encryption_return != 0)
		print_err("crypto_box_easy", "Failed to encrypt the message");
	
	/* Copy the sender's public key to the message data */
	memcpy
	(
		msg_data->pubkey,
		pubkey,
		crypto_box_PUBLICKEYBYTES * sizeof(unsigned char)
	);
	
	/* Send the message data to the receiver */
	SDLNet_TCP_Send(*server_socket, msg_data, sizeof(*msg_data));
}

bool recv_msg
(
	char *msg,
	msg_data_t *msg_data,
	TCPsocket *server_socket,
	unsigned char privkey[crypto_box_SECRETKEYBYTES],
	unsigned char pubkey[crypto_box_PUBLICKEYBYTES]
)
{
	/* Clear the message string */
	memset(msg, 0, MAX_MSG_LEN);

	/* Create a dummy buffer for receiving the sender's dummy data */
	char dummy_buf[MAX_MSG_LEN];

	/* Receive the sender's prompt */
	SDLNet_TCP_Recv
	(
		*server_socket,
		dummy_buf,
		MAX_MSG_LEN
	);
	
	/* Send the receiver's public key to the sender */
	memcpy
	(
		msg_data->pubkey,
		pubkey,
		crypto_box_PUBLICKEYBYTES * sizeof(unsigned char)
	);
	
	SDLNet_TCP_Send
	(
		*server_socket,
		msg_data,
		sizeof(*msg_data)
	);
	
	/* Receiver the sender's message data */
	SDLNet_TCP_Recv(*server_socket, msg_data, sizeof(msg_data_t));
	
	/* Decrypt the message */
	unsigned char umsg[MAX_MSG_LEN];
	
	int decryption_return = crypto_box_open_easy
	(
		umsg,
		msg_data->ciphertext,
		CIPHERTEXT_LEN,
		msg_data->nonce,
		msg_data->pubkey,
		privkey
	);
	
	if(decryption_return != 0)
	{	
		print_err("crypto_box_open_easy", "Failed to decrypt the message");
		return false;
	}
	else
	{
		strcpy(msg, (char *)umsg);
		return true;
	}
}

void add_client_to_server
(
	SDLNet_SocketSet *socket_set,
	TCPsocket *socket,
	TCPsocket *server_socket,
	int *connected_client_cnt
)
{
	/* Reject a connection if the max number of clients is reached.  It will
	simply make the client wait until another person leaves.  Then it will
	autoconnect them */
	if(*connected_client_cnt >= MAX_CLIENT_CNT) return;
	
	/* Add the socket to the set if it is not NULL.  Close it otherwise */
	*socket = SDLNet_TCP_Accept(*server_socket);
	
	if(*socket)
	{
		SDLNet_TCP_AddSocket(*socket_set, *socket);
		*connected_client_cnt += 1;
	}
	else
	{
		SDLNet_TCP_Close(*socket);
	}
}

void remove_client_from_server
(
	SDLNet_SocketSet *socket_set,
	TCPsocket *socket,
	int *connected_client_cnt
)
{
	/* Remove the socket from the set and close the
	connection */
	SDLNet_TCP_DelSocket(*socket_set, *socket);
	SDLNet_TCP_Close(*socket);
	*connected_client_cnt -= 1;
	
	/* Set the socket to NULL so that false connections aren't detected */
	*socket = NULL;
}
