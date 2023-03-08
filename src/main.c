#include "gtk/gtk.h"
#include "SDL2/SDL.h"
#include "SDL2/SDL_net.h"
#include "sodium.h"
#include "src/init.h"
#include "src/net.h"
#include "stdbool.h"
#include "stdlib.h"

#define MAX_CLIENT_CNT 16
#define MAX_MSG_CNT 128

/*static client_t client_arr[MAX_CLIENT_CNT];*/
static int ready_socket_cnt;
static IPaddress server_ip;
static msg_data_t msg_data;
static SDLNet_SocketSet socket_set;
static TCPsocket server_socket;
static unsigned char privkey[crypto_box_SECRETKEYBYTES];
static unsigned char pubkey[crypto_box_PUBLICKEYBYTES];

static bool init_client(int argc, char *argv[])
{	
	bool init_success = false;

	crypto_box_keypair(pubkey, privkey);
	
	init_success = setup_server_connection
	(
		&server_ip,
		&server_socket,
		argv[2],
		atoi(argv[3])
	);
	
	if(init_success)
		init_success = init_client_socket_set(&socket_set, &server_socket);
}

static void terminate_client(void)
{
	if(server_socket && socket_set)
		SDLNet_TCP_DelSocket(socket_set, server_socket);
	
	SDLNet_TCP_Close(server_socket);
	server_socket = NULL;
	
	if(socket_set)
		SDLNet_FreeSocketSet(socket_set);
}

static const char *WINDOW_TITLE = "h";
static const int BOX_PACK_PADDING = 2;
static const int BOX_SPACING = 4;
static const int DEFAULT_WINDOW_HEIGHT = 360;
static const int DEFAULT_WINDOW_WIDTH = 640;

static GtkEntryBuffer *msg_send_text_buffer;
static char msg_recv_buf[MAX_MSG_LEN * MAX_MSG_CNT];
static GtkTextBuffer *msg_recv_text_buffer;
static GtkTextBuffer *user_list_text_buffer;
static GtkWidget *header_bar;
static GtkWidget *inner_box;
static GtkWidget *msg_box;
static GtkWidget *msg_recv_text_view;
static GtkWidget *msg_send_entry;
static GtkWidget *outer_box;
static GtkWidget *user_list_text_view;
static GtkWidget *window;

static void send_client_msg(GtkWidget *msg_send_entry, gpointer data)
{
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
	
	/* Add the message to the text view for received messages */
	strcat(msg_recv_buf, msg);
	strcat(msg_recv_buf, "\n");
	
	gtk_text_buffer_set_text
	(
		GTK_TEXT_BUFFER(msg_recv_text_buffer),
		msg_recv_buf,
		-1
	);
	
	/* Clear the message entry */
	gtk_entry_set_text(GTK_ENTRY(msg_send_entry), "");
}

static gboolean recv_client_msg(gpointer data)
{
	/* Check for socket activity */
	ready_socket_cnt = SDLNet_CheckSockets(socket_set, 1);
	
	/* Receive the message on socket activity */
	if(ready_socket_cnt > 0)
		if(SDLNet_SocketReady(server_socket))
		{
			/* Receive the message */
			char msg[MAX_MSG_LEN];
		
			recv_msg
			(
				msg,
				&msg_data,
				&server_socket,
				privkey,
				pubkey
			);
			
			/* Add the message to the text view for received messages */
			strcat(msg_recv_buf, msg);
			strcat(msg_recv_buf, "\n");
			
			gtk_text_buffer_set_text
			(
				GTK_TEXT_BUFFER(msg_recv_text_buffer),
				msg_recv_buf,
				-1
			);
		}
		
	/* Return true so GLib doesn't remove this fucntion from the main loop */
	return TRUE;
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
	
	/* user_list_text_view */
	user_list_text_view = gtk_text_view_new();
	
	user_list_text_buffer =
		gtk_text_view_get_buffer(GTK_TEXT_VIEW(user_list_text_view));
	
	gtk_box_pack_start
	(
		GTK_BOX(inner_box),
		user_list_text_view,
		TRUE,
		TRUE,
		BOX_PACK_PADDING
	);
	
	gtk_text_view_set_editable(GTK_TEXT_VIEW(user_list_text_view), FALSE);
	
	gtk_text_view_set_wrap_mode
	(
		GTK_TEXT_VIEW(user_list_text_view),
		GTK_WRAP_WORD_CHAR
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
	
	msg_send_text_buffer =
		gtk_entry_get_buffer(GTK_ENTRY(msg_send_entry));
	
	gtk_box_pack_start
	(
		GTK_BOX(msg_box),
		msg_send_entry,
		FALSE,
		FALSE,
		BOX_PACK_PADDING
	);
	
	g_signal_connect(msg_send_entry, "activate", G_CALLBACK(send_client_msg), NULL);
}

int main(int argc, char *argv[])
{
	bool init_success = true;

	init_success = init_libsdl();
	init_success = init_libsdlnet();
	init_success = init_libsodium();
	
	if(init_success)
	{
		if(init_client(argc, argv) == false) return 1;
	
		gtk_init(NULL, NULL);
		setup_widgets();
		gtk_widget_show_all(window);
		
		g_idle_add((void *)recv_client_msg, NULL);
		
		gtk_main();
		terminate_client();
	}
	
	SDLNet_Quit();
	SDL_Quit();
	
	return 0;
}
