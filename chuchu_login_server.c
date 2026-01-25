/*
 *
 * Copyright 2017 Shuouma <dreamcast-talk.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 * ChuChu Login Server for Dreamcast
 */

#include <stdlib.h> 
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "chuchu_common.h"
#include "chuchu_sql.h"
#include "chuchu_msg.h"

/*
 * Function:  auth_process 
 * --------------------
 * authenticates the player that is trying to join the server
 * 
 *  pl: pointer to player struct
 *  msg: pointer to send buffer
 *  buf: pointer to received buffer
 *  auth_state: flag for the authentication state 
 *
 *  returns: 0 => OK
 *          -1 => FAIL
 *
 */
int auth_process(player_t *pl, char* msg, char* buf, AUTH_PROCESS *auth_state) {
  char username[MAX_UNAME_LEN], passwd[MAX_PASSWD_LEN], dc_id[6];
  uint8_t msg_id, msg_flag;
  uint16_t msg_size=0, msg_len=0, port=0;
  uint32_t ip=0;
  struct sockaddr_in sa;
  int rc = 0;
  server_data_t *s = pl->data;
  memset(username, 0, sizeof(username));
  memset(passwd, 0, sizeof(passwd));
  memset(dc_id, 0, sizeof(dc_id));
  
  msg_id = (uint8_t)buf[0];
  msg_flag = (uint8_t)buf[1];
  msg_len = ntohs(char_to_uint16(&buf[2]));
  
  switch(msg_id) {
    case LOGIN_USER_INFO_MSG:
      if ((msg_flag != 0x00) || (msg_len != 0x3c)) {
	chuchu_error(LOGIN_SERVER,"Auth MSG is corrupt");
	*auth_state = AUTH_BROKEN;
	return -1;
      }
      //Store DC ID
      memcpy(dc_id, &buf[0x06], 6);
      chuchu_info(LOGIN_SERVER," <- DC-ID: [%02x%02x%02x%02x%02x%02x]",
		  (uint8_t)dc_id[0],(uint8_t)dc_id[1],(uint8_t)dc_id[2],(uint8_t)dc_id[3],(uint8_t)dc_id[4],(uint8_t)dc_id[5]);

#ifdef DISABLE_AUTH      
      // Force login instead of registration
      rc = 1;
#else
      rc = is_player_in_chuchu_db(s->chu_db_path, dc_id, 0);
#endif
      
      if(rc == 0) {
	chuchu_info(LOGIN_SERVER," <- New user joining...");	
	msg_size = create_chuchu_login_msg(msg, 0x02);
      } else if(rc == 1) {
	chuchu_info(LOGIN_SERVER," <- Registered user joining...");	
	msg_size = create_chuchu_login_msg(msg, 0x01);
      } else {
	chuchu_error(LOGIN_SERVER, "Error in DB");
	*auth_state = AUTH_BROKEN;
	return -1;
      }
      *auth_state = AUTH_STARTED;
      break;
      //If reg. user
    case RESENT_LOGIN_REQUEST_MSG:
      //If not reg user.
    case AUTH_MSG:
      if (msg_len != 0x34) {
	chuchu_error(LOGIN_SERVER,"Not following AUTH process");
	*auth_state = AUTH_BROKEN;
	return -1;
      }
      strlcpy(username,&buf[0x14],sizeof(username));
      if(strlen(username) == 0 || username[0] == '\0') {
	//Send server is full
	msg_size = create_chuchu_resent_login_request_msg(msg, 0x01, 4);
	return msg_size;
      }
      chuchu_info(LOGIN_SERVER," <- Username: %s is trying to join...", username);
      strlcpy(passwd, &buf[0x24], sizeof(passwd));
      memcpy(dc_id, &buf[0x06], 6);
      
      if (msg_id == AUTH_MSG) {
	//Check if username is taken
	if (is_username_taken(s->chu_db_path, username)) {
	  msg_size = create_chuchu_auth_msg(msg, 0x02, 4);
	  return msg_size;
	} else {
	  //Write player data
	  rc = write_player_to_chuchu_db(s->chu_db_path, dc_id, username, passwd); 
	  if (!rc) {
	    chuchu_error(LOGIN_SERVER, "Error in DB");
	    *auth_state = AUTH_BROKEN;
	    return -1;
	  }
	}
      } else {
	//Validate player login
	if(!validate_player_login(s->chu_db_path, username, passwd, dc_id)) {
	  msg_size = create_chuchu_resent_login_request_msg(msg, 0x03, 4);
	  return msg_size;
	}
      }
      //Done redirect to lobby
      msg_size = create_chuchu_auth_msg(msg, 0x01, 4);

      //Add redirect msg
      port = htons(s->chu_lobby_port);
      inet_pton(AF_INET, s->chu_lobby_ip, &(sa.sin_addr)); 
      ip = ntohl(sa.sin_addr.s_addr);
      msg_size = (uint16_t)(msg_size + create_chuchu_redirect_msg(ip, port, &msg[msg_size]));
      *auth_state = AUTH_DONE;
      break;
    default:
      chuchu_error(LOGIN_SERVER,"Not following AUTH process");
      *auth_state = AUTH_BROKEN;
      break;
    }
  
  return msg_size;
}

int main(int argc , char *argv[]) {
  int socket_desc , client_sock , c, optval;
  struct sockaddr_in server , client;
  server_data_t s_data;

  //Load chuch config
  if (!get_chuchu_config(&s_data, argc >= 2 ? argv[1] : "chuchu.cfg"))
    return 0;
  
  socket_desc = socket(AF_INET , SOCK_STREAM , 0);
  if (socket_desc == -1) {
    chuchu_info(LOGIN_SERVER,"Could not create socket");
  }
  chuchu_info(LOGIN_SERVER,"Socket created");

  optval = 1;
  setsockopt(socket_desc, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int));
  
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = INADDR_ANY;
  server.sin_port = htons( s_data.chu_login_port );
  
  if( bind(socket_desc,(struct sockaddr *)&server , sizeof(server)) < 0) {
    perror("Bind failed. Error");
    return 1;
  }
  chuchu_info(LOGIN_SERVER,"Bind done");
  
  listen(socket_desc , 3);

  chuchu_info(LOGIN_SERVER,"Waiting for incoming connections...");
  
  c = sizeof(struct sockaddr_in);
  pthread_t thread_id;
  
  while( (client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c)) ) {
    chuchu_info(LOGIN_SERVER,"Connection accepted from %s on socket %d", inet_ntoa(client.sin_addr), client_sock);
    //Store player data
    player_t *pl = (player_t *)malloc(sizeof(player_t));
    pl->addr = client;
    pl->sock = client_sock;
    memset(pl->username, 0, MAX_UNAME_LEN);
    memset(pl->dreamcast_id, 0, 6);
    pl->controllers = 1;
    pl->client_id = (uint32_t)(client_sock + 0x10000000);
    pl->menu_id = SERVER_MENU;
    pl->item_id = 0x00000000;
    pl->data = &s_data;

    if( pthread_create( &thread_id , NULL ,  chuchu_client_handler , (void*)pl) < 0) {
      perror("Could not create thread");
      return 1;
    }
   
    chuchu_info(LOGIN_SERVER,"Handler assigned");
    pthread_detach(thread_id);
  }
  
  if (client_sock < 0) {
    perror("Accept failed");
    return 1;
  }
  
  return 0;
}

/*
 * Function: chuchu_client_handler
 * --------------------
 * handles each player that join the server (TCP)
 * 
 *  void *data: ptr to server data
 *
 *  returns: 0 => OK and delete thread
 *
 */
void *chuchu_client_handler(void *data) {
  player_t *pl = (player_t *)data; 
  int sock = pl->sock; 
  AUTH_PROCESS a_state = AUTH_NOT_STARTED;
  ssize_t read_size=0, write_size=0;
  uint16_t index=0, n_index=0;
  char c_msg[MAX_PKT_SIZE], s_msg[MAX_PKT_SIZE];
  memset(c_msg, 0, sizeof(c_msg));
  memset(s_msg, 0, sizeof(s_msg));

  struct timeval tv;
  tv.tv_sec = 1800;       /* Timeout in seconds */
  tv.tv_usec = 0;
  setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO,(char *)&tv, sizeof(struct timeval));
  setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,(char *)&tv, sizeof(struct timeval));
  
  init_chuchu_crypt(pl);
  
  //Send inital message to the client
  write_size = create_chuchu_copyright_msg(pl,s_msg,LOGIN_SERVER);
  if (write_size != 0) {
    send_chuchu_msg(sock, s_msg , (int)write_size);
    memset(s_msg, 0, sizeof(s_msg));
  } else {
    free(pl);
    return 0;
  }
  
  a_state = AUTH_STARTED;

  while( (read_size = recv(sock , c_msg , sizeof(c_msg) , 0)) > 0 ) {
    decrypt_chuchu_msg(&pl->client_cipher, c_msg, (long unsigned int)read_size);
            
    index = 0;
    n_index = 0;
    while (read_size > 0) {
      if ((n_index = parse_chuchu_msg(&c_msg[index], (int)read_size)) > 0) {
	write_size = auth_process(pl, s_msg, c_msg, &a_state);
	if (write_size > 0) {
	  send_chuchu_crypt_msg(sock, &pl->server_cipher, s_msg, (int)write_size);
	}
	if (a_state == AUTH_BROKEN || (write_size < 0 && a_state != AUTH_DONE)) {
	  chuchu_error(LOGIN_SERVER,"Client with socket %d is not following protocol - Disconnecting", sock);
	  close(sock);
	  free(pl);
	  return 0;
	}
	if (a_state == AUTH_DONE) {
	  chuchu_info(LOGIN_SERVER,"Done, disconnecting socket %d", sock);
	  close(sock);
	  free(pl);
	  return 0;
	}
	//Decrease size
	read_size -= n_index;
	//Update pointer in recv buff
	index = (uint16_t)(index + n_index);
      } else {
	break;
      }
      memset(s_msg, 0, sizeof(s_msg)); 
    }
    memset(c_msg, 0, sizeof(c_msg));
  }
  
  if(read_size == 0) {
    chuchu_info(LOGIN_SERVER,"Client with socket %d [%s] disconnected", sock, inet_ntoa(pl->addr.sin_addr));
    close(sock);
  } else if(read_size == -1) {
    chuchu_info(LOGIN_SERVER,"recv failed");
    close(sock);
  }
  
  free(pl);
  return 0;
} 
