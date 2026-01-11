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
 * ChuChu MSG functions for Dreamcast
 */

#include "chuchu_common.h"
#include "chuchu_sql.h"

/*
 * Function:  create_chuchu_notify_msg
 * --------------------
 * 0x01 - Notify MSG
 *
 * Used for notifying users, little box with text
 * 
 *  *msg: pointer to msg buffer
 *  msg_id: msg id mapped to notify text
 *
 *  returns: packet size
 *
 */
uint16_t create_chuchu_notify_msg(char *msg, int msg_id) {
  uint16_t pkt_size = 12;
  const char *notify_msg = "?";

  switch(msg_id) {
  case 0x01:
    notify_msg = "You are all alone....\nWait for more players!";
    break;
  case 0x02:
    notify_msg = "The game room is full...\nPlease wait!";
    break;
  case 0x03:
    notify_msg = "Something went wrong\nwhile uploading"; 
    break;
  case 0x04:
    notify_msg = "Upload completed"; 
    break;
  case 0x05:
    notify_msg = "Something went wrong\nwhile downloading"; 
    break;
  case 0x06:
    notify_msg = "Puzzle already exists!";
    break;
  case 0x07:
    notify_msg = "Game room created!";
    break;
  case 0x08:
    notify_msg = "Invalid password!";
    break;
  case 0x09:
    notify_msg = "Already created a game room\nfor this session";
    break; 
  }
  strcpy(&msg[pkt_size], notify_msg);
  pkt_size = (uint16_t)(pkt_size + strlen(notify_msg));
  //Padding
  pkt_size = (uint16_t)(pkt_size + 4);
  create_chuchu_hdr(msg, 0x01, 0x01, pkt_size);

  return pkt_size;
}

/*
 * Function:  create_chuchu_copyright_msg
 * --------------------
 * 0x02,0x17 - Copyright msg
 * 
 * First to be sent, is uncrypted and contains server and client encrypt. vectors
 *
 *  *pl: pointer to player struct
 *  *msg: point to msg buffer
 *  server_type: login or lobby server, sends different copyright txt.
 *
 *  returns: packet size
 *
 */
uint16_t create_chuchu_copyright_msg(player_t *pl, char * msg, uint8_t server_type) {
  uint16_t pkt_size = 0;
  uint32_t server_vector=0, client_vector=0;
  const char *copyright;

  // Login server = 0x01
  if (server_type == LOGIN_SERVER) {
    copyright = "DreamCast Port Map. Copyright SEGA Enterprises. 1999";
    // Else Lobby server
  } else {
    copyright = "DreamCast Lobby Server. Copyright SEGA Enterprises. 1999";
  }
  
  strlcpy(&msg[4], copyright, 64);

  server_vector = htonl(pl->server_seed);
  client_vector = htonl(pl->client_seed);

  uint32_to_char(server_vector, &msg[68]);
  uint32_to_char(client_vector, &msg[72]);

  pkt_size = 76;
  if (server_type == LOGIN_SERVER) {
    create_chuchu_hdr(msg, 0x17, 0x00, pkt_size);
  } else {
    create_chuchu_hdr(msg, 0x02, 0x00, pkt_size);
  }
  
  return pkt_size;
}

/*
 * Function:  create_chuchu_auth_msg
 * --------------------
 * 0x03 - Authentication msg
 * 
 * Is sent when a new user tries to registrate on the server
 *
 *  *msg: point to msg buffer
 *  flag: different flags to the chuchu TCP hdr
 *        flag: 0x01 => Save to VMU, OK
 *        flag: 0x02 => User is already registrated on the server
 *  pkt_size: The size of the msg that needs to be stored in the chuchu hdr
 *
 *  returns: packet size
 */
uint16_t create_chuchu_auth_msg(char *msg, uint8_t flag, uint16_t pkt_size) {
  create_chuchu_hdr(msg, 0x03, flag, pkt_size);
  
  return pkt_size;
}

/*
 * Function:  create_chuchu_resent_login_request_msg
 * --------------------
 * 0x04 - Resent of Login request
 * 
 * Used in the lobby server to validate user login, crucial for gameplay, the DC ID 
 * is sent back in this msg to the DC and that value is used during start of game.
 *
 *  *msg: point to msg buffer
 *  flag: different flags to the chuchu TCP hdr
 *        flag: 0x00 => Login OK
 *        flag: 0x01 => Server is full
 *        flag: 0x03 => Wrong password
 *  pkt_size: The size of the msg that needs to be stored in the chuchu hdr
 *
 *  returns: packet size
 */
uint16_t create_chuchu_resent_login_request_msg(char *msg, uint8_t flag, uint16_t pkt_size) {
  create_chuchu_hdr(msg, 0x04, flag, pkt_size);
  return pkt_size;
}

/*
 * Function:  create_chuchu_chat_msg
 * --------------------
 * 0x06,0x1A - Chat msg and Whisper msg
 * 
 * Used to send chat msg in the main chat or whisper to a specific user
 *
 *  *pl:  pointer to player struct
 *  *buf: pointer to incoming client msg  
 *  *msg: pointer to outgoing client msg
 *
 *  returns: packet size
 */
uint16_t create_chuchu_chat_msg(player_t *pl, char* buf, char* msg) {
  uint8_t hdr_len = (uint8_t)buf[2];  
  uint16_t pkt_size = 0, str_len = 0;
  uint32_t menu_id = char_to_uint32(&buf[4]);
  uint32_t item_id = char_to_uint32(&buf[8]);
  int i=0;
  char tmp_chat_msg[1024];
  char snd_username[MAX_UNAME_LEN];
  server_data_t *s = (server_data_t*)pl->data;
  int max_client = s->m_cli;

  memset(snd_username, 0, sizeof(snd_username));
  memset(tmp_chat_msg, 0, sizeof(tmp_chat_msg));
    
  str_len = (uint16_t)(hdr_len - 0x0c);
  if (str_len >= sizeof(tmp_chat_msg)) {
    chuchu_error(LOBBY_SERVER,"Very large chat msg...");
    return 0;
  }

  //Get username from the user that sent the msg
  strlcpy(snd_username, pl->username, sizeof(snd_username));
  
  //Lobby chat, goes to all
  if (item_id == 0x00) {
    pkt_size = (uint16_t)(pkt_size + 4);
    pkt_size = (uint16_t)(pkt_size + uint32_to_char(menu_id, &msg[pkt_size]));
    pkt_size = (uint16_t)(pkt_size + uint32_to_char(item_id, &msg[pkt_size]));
    
    sprintf(tmp_chat_msg, "[%s]:\t", snd_username);
    strncat(tmp_chat_msg, &buf[12], str_len);
    strcpy(&msg[pkt_size], tmp_chat_msg);
    pkt_size = (uint16_t)(pkt_size + strlen(tmp_chat_msg));
    
    //Padding
    pkt_size = (uint16_t)(pkt_size + 2);
    
    //Create header
    create_chuchu_hdr(msg, 0x06, 0x00, pkt_size);
    //Send to all
    for(i=0;i<max_client;i++)
      if(s->p_l[i])
	send_chuchu_crypt_msg(s->p_l[i]->sock, &s->p_l[i]->server_cipher, msg, pkt_size);
    
    return 0;
  }
  
  //Check after specfic user, whisper
  for(i=0;i<max_client;i++) {
    if(s->p_l[i]) {
      if (s->p_l[i]->menu_id == menu_id && s->p_l[i]->client_id == item_id) {
	//Create whisper to user msg
	snprintf(tmp_chat_msg, sizeof(tmp_chat_msg), "Message from '%s'\n\n", snd_username);
	strncat(tmp_chat_msg, &buf[12], str_len);
	strcpy(&msg[4], tmp_chat_msg);
	pkt_size = (uint16_t)(4 + strlen(tmp_chat_msg));
	//Padding
	pkt_size = (uint16_t)(pkt_size + 4);
	create_chuchu_hdr(msg, 0x1a, 0x00, pkt_size);  
	send_chuchu_crypt_msg(s->p_l[i]->sock, &s->p_l[i]->server_cipher, msg, pkt_size);
	return 0;
      }
    }
  }
  
  return 0;
}


/*
 * Function:  create_chuchu_start_game_msg
 * --------------------
 * 0x0E - Starts the actual game
 * 
 * Used to tell the DC clients that the game is starting
 *
 *  *gr:  pointer to game room struct
 *
 *  How the pkt is filled (What -> Position in pkt):
 *
 *   0. Dreamcast ID -> 0x04
 *   0. IP -> 0x0c
 *   0. Name -> 0x14
 *   1. Dreamcast ID -> 0x24
 *   1. IP -> 0x2c
 *   1. Name -> 0x34
 *   2. Dreamcast ID -> 0x44
 *   2. IP -> 0x4c
 *   2. Name -> 0x54
 *   3. Dreamcast ID -> 0x64
 *   3. IP -> 0x6c
 *   3. Name -> 0x74
 *
 *  returns: packet size
 */
uint16_t create_chuchu_start_game_msg(game_room_t *gr) {
  char msg[MAX_PKT_SIZE];
  uint16_t pkt_size;
  uint32_t ip;
  int i=0,j=0,entries=0;
  int max_player_slots = gr->m_pl_slots;
  memset(msg, 0, sizeof(msg));
  
  pkt_size = 4;
  //Build pkt
  for(i=0;i<max_player_slots;i++) {
    //First add the player (controller 1)
    if(gr->player_slots[i]) {
      memcpy(&msg[pkt_size], gr->player_slots[i]->dreamcast_id, 6);            
      pkt_size = (uint16_t)(pkt_size + 8);
      ip = ntohl(gr->player_slots[i]->addr.sin_addr.s_addr); 
      uint32_to_char(ip, &msg[pkt_size]);
      pkt_size = (uint16_t)(pkt_size + 8);
      strlcpy(&msg[pkt_size], gr->player_slots[i]->username, 8+1);
      pkt_size = (uint16_t)(pkt_size + 16);
      entries++;

      /*Check if player have choosen more controllers at the start of the network screen, if, then add, 
	the entries needs to be after eachother in the start game pkt, amount of controllers/players are checked before
	the user joins the room, but lets add one more...check*/
      if(gr->player_slots[i]->controllers > 1) {
	if (entries + (gr->player_slots[i]->controllers-1) > max_player_slots) {
	  break;
	} else {
	  for (j=0;j<(gr->player_slots[i]->controllers-1);j++) {
	    memcpy(&msg[pkt_size], gr->player_slots[i]->dreamcast_id, 6);
	    pkt_size = (uint16_t)(pkt_size + 8);
	    uint32_to_char(ip, &msg[pkt_size]);
	    pkt_size = (uint16_t)(pkt_size + 8);
	    strlcpy(&msg[pkt_size], gr->player_slots[i]->username, 8+1);
	    pkt_size = (uint16_t)(pkt_size + 16);
	    entries++;
	  }
	}
      }
    }
  }
 
  create_chuchu_hdr(msg, 0x0e, (uint8_t)entries, pkt_size);
  
  //Send the start pkt to all in game room
  for(i=0;i<max_player_slots;i++) {
    if(gr->player_slots[i]) {
      send_chuchu_crypt_msg(gr->player_slots[i]->sock, &gr->player_slots[i]->server_cipher, msg, pkt_size);
      //Set start_game value to 0, will be read in delete_player during disconnect
      gr->player_slots[i]->store_ranking = 0;
      //Remove user from game room slot
      gr->taken_seats--;
      gr->player_slots[i] = NULL;
    }
  }
  
  return 0;
}


/*
 * Function:  create_chuchu_add_info
 * --------------------
 * 0x11, - Add info to right bottom box in menu screen
 * 
 * Adds text to the right bottom box in the menu screen,
 * usually when somebody presses X on a item.
 *
 *  *s:  pointer to server data struct
 *  *msg: pointer to outgoing client msg  
 *  menu_id: which menu are we in
 *  item_id: which item is pressed
 *
 *  returns: packet size
 */
uint16_t create_chuchu_add_info(server_data_t *s, char* msg, uint32_t menu_id, uint32_t item_id) {
  uint16_t pkt_size = 12;
  game_room_t *gr = NULL;
  char box_text[128];
  int i;
  int max_puzzles = s->m_puzz;
  int max_clients = s->m_cli;
  memset(box_text, 0, sizeof(box_text));

  //View stat on a user, just check item_id
  for (i=0;i<max_clients;i++) {
    if (s->p_l[i] != NULL) {
      if (s->p_l[i]->client_id == item_id) {
	sprintf(box_text,
		"Rounds\nWon: %d\nLost: %d\nTotal: %d\nControllers: %d",
		(s->p_l[i]->won_rnds + s->p_l[i]->db_won_rnds),
		(s->p_l[i]->lost_rnds + s->p_l[i]->db_lost_rnds),
		(s->p_l[i]->total_rnds + s->p_l[i]->db_total_rnds),
		 s->p_l[i]->controllers
	  );
	strcpy(&msg[pkt_size], box_text);
	pkt_size = (uint16_t)(pkt_size + strlen(box_text));
	//Padding
	pkt_size = (uint16_t)(pkt_size + 4);
	create_chuchu_hdr(msg, 0x11, 0x00, pkt_size); 
	return pkt_size;
      }
    }
  }
  //Else check the rest
  switch(menu_id) {
  case GAME_MENU:
    for(i=0;i<s->m_rooms;i++)
      if(s->g_l[i]) {
	if (s->g_l[i]->item_id == item_id) {
	  gr = s->g_l[i];
	  break;
	}
      }
    //If not a game room return 0
    if (gr == NULL)
      return 0;
    sprintf(box_text, "%d of 4\nusers in the room\nCreated by\n%s", gr->taken_seats,gr->creator);
    strcpy(&msg[pkt_size], box_text);
    break;      
  case PUZZLE_ZONE_MENU:
    sprintf(box_text, "Press X to\nsee the puzzle creator");
    strcpy(&msg[pkt_size], box_text);
    break;
  case PUZZLE_ZONE_FILE:
    for (i=0;i<max_puzzles;i++) {
      if (s->puzz_l[i] != NULL) {
	if (s->puzz_l[i]->id == item_id) {
	  sprintf(box_text, "Created by\n%s\nDownloaded:\n%d", s->puzz_l[i]->u_name, s->puzz_l[i]->dl);
	  strcpy(&msg[pkt_size], box_text);
	  break;
	}
      }
    }
    break;
  default:
    //Check if 
    sprintf(box_text, "No additional\ninfo");
    strcpy(&msg[pkt_size], box_text);
    break;
  }
  
  if (box_text[0] == '\0')
    return 0;
  
  pkt_size = (uint16_t)(pkt_size + strlen(box_text));
  //Padding
  pkt_size = (uint16_t)(pkt_size + 4);
  create_chuchu_hdr(msg, 0x11, 0x00, pkt_size); 
  
  return pkt_size;
}

/*
 * Function:  create_chuchu_info_msg
 * --------------------
 * 0x1A, - Used in top news for example, large green window with txt
 * 
 * Displays large green window with text.
 * 
 *  *msg: pointer to outgoing client msg  
 *  *s:  pointer to server data struct
 *  info_flag: Which info should be displayed, just added info or ranking in this server 
 *
 *  returns: packet size
 */
uint16_t create_chuchu_info_msg(char* msg, server_data_t *s, int info_flag) {
  FILE *file = NULL;
  const char* f_name = s->chu_info_path;
  uint16_t pkt_size = 0;
  uint32_t fileLen = 0;
    
  switch(info_flag) {
  case NEWS:
    
    file = fopen(f_name, "rb");
    if (!file) {
      chuchu_error(SERVER, "Unable to open file %s, default page", f_name);
      pkt_size = 4;
      return pkt_size;
    }
	
    fseek(file, 0, SEEK_END);
    fileLen = (uint32_t)ftell(file);
    fseek(file, 0, SEEK_SET);

    if (fileLen > (MAX_PKT_SIZE-4)) {
      chuchu_error(SERVER, "File size greater then buffer");
      return 0;
    }

    fread(&msg[4], fileLen, 1, file);
    fclose(file);

    pkt_size = (uint16_t)(4 + fileLen);
    break;
  case RANKING:
    pkt_size = (uint16_t)read_top_ranking_from_chuchu_db(&msg[4], s->chu_db_path);
    if (pkt_size == 0) {
      chuchu_error(SERVER, "Could not generate top ranking msg");
      return 0;
    }
    pkt_size = (uint16_t)(pkt_size + 4);
  }
  //Padding
  pkt_size = (uint16_t)(pkt_size + 4);  
  create_chuchu_hdr(msg, 0x1a, 0x01, pkt_size);
   
  return pkt_size;
}

/*
 * Function:  create_chuchu_login_msg
 * --------------------
 * 0x18, - Response to copyright
 * 
 * Used to tell the DC if the user is already registered or new
 * 
 * 
 *  *msg: pointer to outgoing client msg  
 *  flag: flag to be used in the chuchu hdr
 *      flag: 0x01 => Reg. user, check VMU for U/P
 *      flag: 0x02 => New user
 *
 *  returns: packet size
 */
uint16_t create_chuchu_login_msg(char *msg, uint8_t flag) {
  uint16_t pkt_size = 4;
  create_chuchu_hdr(msg, 0x18, flag, pkt_size);
  
  return pkt_size;
}

/*
 * Function:  create_chuchu_redirect_msg
 * --------------------
 * 0x19, - Redirect from login to lobby msg
 * 
 * Used to redirect the user from the login server
 * to the lobby server. This IP is also used in the
 * reconnect after a gameplay.
 *  
 *  ip: Lobby IP  
 *  port: Lobby Port
 *  *msg: ptr to outgoing client msg
 *
 *  returns: packet size
 */
uint16_t create_chuchu_redirect_msg(uint32_t ip, uint16_t port, char * msg) {
  uint16_t pkt_size = 0;
  //Header (4 bytes)
  pkt_size = (uint16_t)(pkt_size + 4);
  pkt_size = (uint16_t)(pkt_size + uint32_to_char(ip, &msg[pkt_size]));
  pkt_size = (uint16_t)(pkt_size + uint16_to_char(port, &msg[pkt_size]));
  //Add two bytes padding
  pkt_size = (uint16_t)(pkt_size + 2);
  
  create_chuchu_hdr(msg, 0x19, 0x00, pkt_size);
  
  return pkt_size;
}
