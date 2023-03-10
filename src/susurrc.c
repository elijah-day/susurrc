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

#include "gtk/gtk.h"
#include "SDL2/SDL.h"
#include "SDL2/SDL_net.h"
#include "sodium.h"
#include "src/err.h"
#include "src/init.h"
#include "src/net.h"
#include "src/susurrc.h"
#include "stdbool.h"
#include "stdlib.h"

static const char *HEADER_BAR_CONNECTED_TITLE = "Connected";
static const char *HEADER_BAR_DISCONNECTED_TITLE = "Disconnected";
static const char *MSG_SEND_ENTRY_PLACEHOLDER = "Send a message...";
static const char *SERVER_CONNECT_BUTTON_LABEL = "Connect";

static const char *SERVER_HOSTNAME_ENTRY_PLACEHOLDER =
	"Hostname or IP address";
	
static const char *SERVER_PORT_ENTRY_PLACEHOLDER = "Port";
static const char *WINDOW_TITLE = "SusurrC";
static const int BOX_PACK_PADDING = 2;
static const int BOX_SPACING = 4;
static const int DEFAULT_WINDOW_HEIGHT = 360;
static const int DEFAULT_WINDOW_WIDTH = 640;
static const int SOCKET_CHECK_TIMEOUT = 1;

static char msg_recv_buf[MAX_MSG_LEN * MAX_MSG_CNT];
static int ready_socket_cnt;
static IPaddress server_ip;
static msg_data_t msg_data;
static SDLNet_SocketSet socket_set;
static TCPsocket server_socket;
static unsigned char privkey[crypto_box_SECRETKEYBYTES];
static unsigned char pubkey[crypto_box_PUBLICKEYBYTES];

static GtkEntryBuffer *msg_send_entry_buffer;
static GtkEntryBuffer *server_hostname_entry_buffer;
static GtkEntryBuffer *server_port_entry_buffer;
static GtkTextBuffer *msg_recv_text_buffer;
static GtkWidget *control_box;
static GtkWidget *header_bar;
static GtkWidget *inner_box;
static GtkWidget *msg_box;
static GtkWidget *msg_recv_text_view;
static GtkWidget *msg_send_entry;
static GtkWidget *outer_box;
static GtkWidget *server_connect_button;
static GtkWidget *server_hostname_entry;
static GtkWidget *server_port_entry;
static GtkWidget *window;
static guint recv_msg_from_server_id;

static void append_to_msg_recv_buffer(const char *msg)
{
	/* Append the new message to the received message buffer */
	strcat(msg_recv_buf, msg);
	strcat(msg_recv_buf, "\n");
	
	/* Update text view with the new received message buffer */
	gtk_text_buffer_set_text
	(
		GTK_TEXT_BUFFER(msg_recv_text_buffer),
		msg_recv_buf,
		-1
	);
}

static void set_header_bar_title_and_subtitle
(
	const char *title,
	const char *subtitle
)
{
	gtk_header_bar_set_title(GTK_HEADER_BAR(header_bar), title);
	gtk_header_bar_set_subtitle(GTK_HEADER_BAR(header_bar), subtitle);
}

static void terminate_socket_connection(void)
{
	if(server_socket && socket_set)
		SDLNet_TCP_DelSocket(socket_set, server_socket);
		
	if(server_socket)
		SDLNet_TCP_Close(server_socket);
	
	if(socket_set)
		SDLNet_FreeSocketSet(socket_set);
		
	server_socket = NULL;
	socket_set = NULL;
		
	/* Possible TODO: Move the header bar setups from the socket
	connection/termination functions.  They aren't related to socket
	connections (Though, they make sense here logically based on when each
	function is called, since the header is going to be set
	simultaneously) */
	set_header_bar_title_and_subtitle
	(
		HEADER_BAR_DISCONNECTED_TITLE,
		""
	);
}

static bool init_socket_connection
(
	const char *server_hostname,
	int server_port
)
{	
	bool init_success;

	/* Terminate any active connections beforehand so the client doesn't
	accidentally fill up unnecessary slots */
	terminate_socket_connection();
	
	/* Setup the socket connection */
	init_success = setup_server_connection
	(
		&server_ip,
		&server_socket,
		server_hostname,
		server_port
	);
	
	/* Check if the connection could be made */
	if(init_success)
		init_success = init_client_socket_set(&socket_set, &server_socket);
	
	/* Setup the header on success */
	if(init_success)
		set_header_bar_title_and_subtitle
		(
			HEADER_BAR_CONNECTED_TITLE,
			server_hostname
		);
	
	/* Return */
	return init_success;
}

static void send_msg_to_server(GtkWidget *msg_send_entry, gpointer data)
{
	/* Avoid trying to send a message while there are no socket connections */
	if(server_socket == NULL)
	{
		print_err
		(
			"send_msg_to_server",
			"Could not send message.  No active connection"
		);
		
		return;
	}

	/* Send the message to the server */	
	char msg[MAX_MSG_LEN];
	strcpy(msg, gtk_entry_get_text(GTK_ENTRY(msg_send_entry)));
	
	send_msg
	(
		msg,
		&msg_data,
		&server_socket,
		privkey,
		pubkey
	);
	
	/* Clear the message entry */
	gtk_entry_set_text(GTK_ENTRY(msg_send_entry), "");
}

static gboolean recv_msg_from_server(gpointer data)
{
	/* Check for socket activity */
	ready_socket_cnt = SDLNet_CheckSockets(socket_set, SOCKET_CHECK_TIMEOUT);
	
	/* Receive the message on socket activity */
	if(ready_socket_cnt > 0)
		if(SDLNet_SocketReady(server_socket))
		{
			/* Receive the message */
			char msg[MAX_MSG_LEN];
		
			bool recv_success = recv_msg
			(
				msg,
				&msg_data,
				&server_socket,
				privkey,
				pubkey
			);
			
			if(strcmp(msg, "") != 0)
			{
				append_to_msg_recv_buffer(msg);
			}
			
			if(recv_success == false)
			{
				/* Close the connection and return false so that GLib stops
				attempting to receive messages */
				terminate_socket_connection();
				return FALSE;
			}
		}
		
	/* Return true so GLib doesn't remove this fucntion from the main loop */
	return TRUE;
}

static void connect_to_server(gpointer data)
{
	/* Get the hostname and port */
	const char *server_hostname =
		gtk_entry_buffer_get_text(GTK_ENTRY_BUFFER
		(server_hostname_entry_buffer));
	
	const int server_port =
		atoi(gtk_entry_buffer_get_text(GTK_ENTRY_BUFFER
		(server_port_entry_buffer)));

	/* Attempt to make the socket connection */
	bool init_success = init_socket_connection
	(
		server_hostname,
		server_port
	);
	
	if(init_success)
	{
		/* Generate the client's keypair */
		crypto_box_keypair(pubkey, privkey);
		
		/* Add message receiving to the GLib event loop */
		recv_msg_from_server_id = g_idle_add
		(
			(void *)recv_msg_from_server,
			NULL
		);
	}
	else
	{
		print_err("connect_to_server", "Could not connect to server");
		
		/* Remove message receiving from the GLib event loop.  Otherwise,
		it will try to receive messages on the terminated socket */
		if(recv_msg_from_server_id != 0)
			g_source_remove(recv_msg_from_server_id);
		
		/* Terminate the socket connection */
		terminate_socket_connection();
	}
}

static void setup_widgets(void)
{
	/* window */
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(window), WINDOW_TITLE);
	
	gtk_window_set_default_size
	(
		GTK_WINDOW(window),
		DEFAULT_WINDOW_WIDTH,
		DEFAULT_WINDOW_HEIGHT
	);
	
	g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
	
	/* outer_box */
	outer_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, BOX_SPACING);
	gtk_container_add(GTK_CONTAINER(window), outer_box);
	
	/* header_bar */
	header_bar = gtk_header_bar_new();
	
	gtk_box_pack_start
	(
		GTK_BOX(outer_box),
		header_bar,
		FALSE,
		FALSE,
		BOX_PACK_PADDING
	);
	
	/* inner_box */
	inner_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, BOX_SPACING);
	
	gtk_box_pack_start
	(
		GTK_BOX(outer_box),
		inner_box,
		TRUE,
		TRUE, 
		BOX_PACK_PADDING
	);
	
	/* control_box */
	control_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, BOX_SPACING);
	
	gtk_box_pack_start
	(
		GTK_BOX(inner_box),
		control_box,
		TRUE,
		TRUE,
		BOX_PACK_PADDING
	);
	
	/* server_hostname_entry */
	server_hostname_entry = gtk_entry_new();
	
	server_hostname_entry_buffer =
		gtk_entry_get_buffer(GTK_ENTRY(server_hostname_entry));
	
	gtk_entry_set_placeholder_text
	(
		GTK_ENTRY(server_hostname_entry),
		SERVER_HOSTNAME_ENTRY_PLACEHOLDER
	);
	
	gtk_box_pack_start
	(
		GTK_BOX(control_box),
		server_hostname_entry,
		FALSE,
		FALSE,
		BOX_PACK_PADDING
	);
	
	/* server_port_entry */
	server_port_entry = gtk_entry_new();
	
	server_port_entry_buffer =
		gtk_entry_get_buffer(GTK_ENTRY(server_port_entry));
		
	gtk_entry_set_placeholder_text
	(
		GTK_ENTRY(server_port_entry),
		SERVER_PORT_ENTRY_PLACEHOLDER
	);
	
	gtk_box_pack_start
	(
		GTK_BOX(control_box),
		server_port_entry,
		FALSE,
		FALSE,
		BOX_PACK_PADDING
	);
	
	/* server_connect_button */
	server_connect_button =
		gtk_button_new_with_label(SERVER_CONNECT_BUTTON_LABEL);
	
	gtk_box_pack_start
	(
		GTK_BOX(control_box),
		server_connect_button,
		FALSE,
		FALSE,
		BOX_PACK_PADDING
	);
	
	g_signal_connect
	(
		server_connect_button,
		"clicked",
		G_CALLBACK(connect_to_server),
		NULL
	);
	
	/* msg_box */
	msg_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, BOX_SPACING);
	
	gtk_box_pack_start
	(
		GTK_BOX(inner_box),
		msg_box,
		TRUE,
		TRUE,
		BOX_PACK_PADDING
	);
	
	/* msg_recv_text_view */
	msg_recv_text_view = gtk_text_view_new();
	
	msg_recv_text_buffer =
		gtk_text_view_get_buffer(GTK_TEXT_VIEW(msg_recv_text_view));
	
	gtk_box_pack_start
	(
		GTK_BOX(msg_box),
		msg_recv_text_view,
		TRUE,
		TRUE,
		BOX_PACK_PADDING
	);
	
	gtk_text_view_set_editable(GTK_TEXT_VIEW(msg_recv_text_view), FALSE);
	
	gtk_text_view_set_wrap_mode
	(
		GTK_TEXT_VIEW(msg_recv_text_view),
		GTK_WRAP_WORD_CHAR
	);
	
	/* msg_send_entry */
	msg_send_entry = gtk_entry_new();
	
	msg_send_entry_buffer =
		gtk_entry_get_buffer(GTK_ENTRY(msg_send_entry));
	
	gtk_entry_set_placeholder_text
	(
		GTK_ENTRY(msg_send_entry),
		MSG_SEND_ENTRY_PLACEHOLDER
	);
	
	g_signal_connect(msg_send_entry, "activate", G_CALLBACK(send_msg_to_server), NULL);
	
	gtk_box_pack_start
	(
		GTK_BOX(msg_box),
		msg_send_entry,
		FALSE,
		FALSE,
		BOX_PACK_PADDING
	);
}

int main(int argc, char *argv[])
{
	bool init_success = true;

	init_success = init_libsdl();
	init_success = init_libsdlnet();
	init_success = init_libsodium();
	
	server_socket = NULL;
	socket_set = NULL;
	
	if(init_success)
	{
		gtk_init(&argc, &argv);
		setup_widgets();
		gtk_widget_show_all(window);
		
		/* Called here to set the header bar to "disconnected" */
		terminate_socket_connection();
		
		gtk_main();
	}
	
	terminate_socket_connection();
	SDLNet_Quit();
	SDL_Quit();
	
	return 0;
}
