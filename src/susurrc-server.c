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

#include "SDL2/SDL.h"
#include "SDL2/SDL_net.h"
#include "sodium.h"
#include "src/err.h"
#include "src/init.h"
#include "src/net.h"
#include "src/susurrc.h"
#include "stdbool.h"
#include "stdlib.h"

static const int SOCKET_CHECK_TIMEOUT = 1;

static client_t client_arr[MAX_CLIENT_CNT];
static int connected_client_cnt;
static int ready_socket_cnt;
static IPaddress server_ip;
static msg_data_t msg_data;
static SDLNet_SocketSet socket_set;
static TCPsocket server_socket;
static unsigned char privkey[crypto_box_SECRETKEYBYTES];
static unsigned char pubkey[crypto_box_PUBLICKEYBYTES];

static void modify_msg_with_info(char *msg, client_t *client)
{
	char new_msg[MAX_MSG_LEN];
	
	/* Start the modified message with the client's username */
	strcpy(new_msg, client->username);
	strcat(new_msg, ": ");
	
	/* Append the original message */
	strcat(new_msg, msg);
	
	/* Copy the modified message to the original message string */
	strcpy(msg, new_msg);
}

static bool init_server(int argc, char *argv[])
{	
	bool init_success = false;

	crypto_box_keypair(pubkey, privkey);
	init_client_arr(client_arr, MAX_CLIENT_CNT);
	connected_client_cnt = 0;
	
	init_success = setup_server_connection
	(
		&server_ip,
		&server_socket,
		NULL,
		atoi(argv[1])
	);
	
	if(init_success)
		init_success = init_server_socket_set(&socket_set, &server_socket);
}

static void terminate_server(void)
{
	if(server_socket && socket_set)
		SDLNet_TCP_DelSocket(socket_set, server_socket);
	
	SDLNet_TCP_Close(server_socket);
	server_socket = NULL;
	
	if(socket_set)
		SDLNet_FreeSocketSet(socket_set);
}

static void run_server(void)
{
	bool is_running = true;
	
	while(is_running)
	{	
		ready_socket_cnt = SDLNet_CheckSockets
		(
			socket_set,
			SOCKET_CHECK_TIMEOUT
		);
		
		if(ready_socket_cnt > 0)
			for(int i = 0; i < MAX_CLIENT_CNT; i++)
			{
				if(SDLNet_SocketReady(client_arr[i].socket))
				{
					/* Receive a message if activity is detected from the
					client's socket and the client is already connected */
					
					char msg[MAX_MSG_LEN];
					
					bool recv_success = recv_msg
					(
						msg,
						&msg_data,
						&client_arr[i].socket,
						privkey,
						pubkey
					);
					
					/* Drop a client from the server if they send an empty
					message (occurs on client disconnect) */
					if(recv_success == false)
					{
						remove_client_from_server
						(
							&socket_set,
							&client_arr[i].socket,
							&connected_client_cnt
						);
						
						/* Exit this iteration so we don't send empty
						messages to the other clients */
						continue;
					}
					
					modify_msg_with_info(msg, &client_arr[i]);
					
					for(int j = 0; j < MAX_CLIENT_CNT; j++)
						if(client_arr[j].socket != NULL)
							send_msg
							(
								msg,
								&msg_data,
								&client_arr[j].socket,
								privkey,
								pubkey
							);
				}
				else if
				(
					SDLNet_SocketReady(socket_set) &&
					!client_arr[i].socket
				)
				{
					/* Try to connect a client if activity is detected from
					the entire socket set and the current client's socket is
					disconnected/NULL */
					
					add_client_to_server
					(
						&socket_set,
						&client_arr[i].socket,
						&server_socket,
						&connected_client_cnt
					);
				}
			}
	}
}

int main(int argc, char *argv[])
{
	if(argc < 2)
		print_server_arg_err();
	else
	{
		init_server(argc, argv);
		run_server();
		terminate_server();
	}
}
