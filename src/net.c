#include "SDL2/SDL_net.h"
#include "src/err.h"
#include "src/net.h"
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

void init_client_arr(client_t *client_arr, int client_cnt)
{
	for(int i = 0; i < client_cnt; i++)
	{
		client_arr[i].is_connected = false;
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

void recv_msg
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
		print_err("crypto_box_open_easy", "Failed to decrypt the message");
		
	strcpy(msg, (char *)umsg);
}
