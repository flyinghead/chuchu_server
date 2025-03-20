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
 * ChuChu Lobby Server for Dreamcast
 */

#include <stdlib.h>    
#include <sys/socket.h>
#include <arpa/inet.h> 
#include <unistd.h>
#include "chuchu_common.h"
#include "chuchu_sql.h"
#include "chuchu_msg.h"

uint16_t create_chuchu_game_menu(char* msg, game_room_t *gr);
uint16_t create_chuchu_room_menu(server_data_t* s, char* msg);
void send_txt_to_all(server_data_t *s, char* username, int txt_flag);

static void lock_server(server_data_t *server) {
  pthread_mutex_lock(&server->mutex);
}

static void unlock_server(server_data_t *server) {
  pthread_mutex_unlock(&server->mutex);
}

/*
 * Function: init_game_rooms
 * --------------------
 *
 * Allocates memory for game rooms
 * 
 *  *s: ptr to server data struct
 *
 *  returns: void
 *
 */
void init_game_rooms(server_data_t *s) {
  int max_player_slots = s->m_pl_slots;

  const char *r_menu[] = { "PSO Room", "Sonic Room", "Mice Room", "Cat Room", "Team Room", };
  int r_icons[] = { PSO_ICON, SONIC_ICON, MICE_ICON, MICE_ICON, DD_ICON };
  
  int nrooms = s->deedee_server ? 2 : 5;
  for (int i = 0; i < nrooms; i++) {
    if (s->g_l[i] == NULL) { 
      game_room_t *gr = (game_room_t *)malloc(sizeof(game_room_t));
      strlcpy(gr->g_name, r_menu[i], sizeof(gr->g_name));
      gr->l_icon = TEAM_ICON;
      gr->r_icon = (uint8_t)r_icons[i];
      gr->menu_id = (uint32_t)GAME_MENU;
      gr->item_id = (uint32_t)(0x2000 + i);
      gr->taken_seats = 0;
      gr->passwd_protected = 0;
      gr->static_room = 1;
      strlcpy(gr->creator, "Admin", MAX_UNAME_LEN);
      gr->duration = 0;
      gr->m_pl_slots = max_player_slots;
      gr->player_slots = calloc((size_t)max_player_slots, sizeof(player_t *));
      s->g_l[i] = gr;
    }
  }
}

/*
 * Function: get_room_from_item_id
 * --------------------
 *
 * Returns game room from item id
 * 
 *  *s: ptr to server data struct
 *  item_id: id of pressed game room
 *
 *  returns: ptr to game room
 *
 */
game_room_t* get_room_from_item_id(server_data_t *s, uint32_t item_id) {
  int i;
  for(i=0;i<s->m_rooms;i++)
    if(s->g_l[i])
      if (s->g_l[i]->item_id == item_id)
	return s->g_l[i];
  
  return NULL;
}

/*
 * Function: validate_game_room
 * --------------------
 *
 * When a user enters a game room, this function looks
 * if the joining user has timed out and have a ptr
 * in the game room list.
 * 
 *  *gr: ptr to game room struct
 *  *pl: ptr to player struct
 *
 *  returns: void
 *
 */
void validate_game_room(game_room_t *gr, player_t *pl) {
  int i;
  int max_player_slots = gr->m_pl_slots;
  int found = 0;
  
  for (i=0;i<max_player_slots;i++) {
    if (gr->player_slots[i] != NULL) {
      if (gr->player_slots[i]->username[0] == '\0') {
	chuchu_info(LOBBY_SERVER,"Found a ghost...remove");
	gr->taken_seats--;
	gr->player_slots[i] = NULL;
	found = 1;
      } else if (memcmp(pl->dreamcast_id, gr->player_slots[i]->dreamcast_id, 6) == 0) {
	chuchu_info(LOBBY_SERVER,"Found a duplicate...remove");
	gr->taken_seats--;
	gr->player_slots[i] = NULL;
	found = 1;
      }
    }
  }
  
  if (found) {
    //Rearrange after validation
    for (i=1;i<max_player_slots;i++) {
      if (gr->player_slots[i-1] == NULL) {
	gr->player_slots[i-1] = gr->player_slots[i];
	gr->player_slots[i] = NULL;
      }
    }
  }
  return;
}


/*
 * Function: leave_game_room
 * --------------------
 *
 * When a user leaves a game room or disconnects, this function
 * removes the user from the game rooms.
 * 
 *  *pl: ptr to player struct
 *  menu_id: id of the menu
 *  item_id: id of the item
 *
 *  returns: void
 *
 */
void leave_game_room(player_t *pl, uint32_t menu_id, uint32_t item_id) {
  char msg[MAX_PKT_SIZE];
  int i=0,j=0,k=0;
  game_room_t *gr = NULL;
  server_data_t *s = (server_data_t *)pl->data;
  int max_rooms = s->m_rooms;
  int max_player_slots = s->m_pl_slots;

  for(i=0;i<max_rooms;i++) {
    if (s->g_l[i] != NULL) {
      //Just to be safe
      for (j=0;j<max_player_slots;j++) {
	if(s->g_l[i]->player_slots[j] != NULL) { 
	  if(s->g_l[i]->player_slots[j]->client_id == pl->client_id) {
	    s->g_l[i]->taken_seats--;
	    //Leaving user is set to NULL
	    chuchu_info(LOBBY_SERVER,"User %s left room %s", s->g_l[i]->player_slots[j]->username, s->g_l[i]->g_name);
	    s->g_l[i]->player_slots[j] = NULL;
	      
	    //Rearrange array
	    for (k=1;k<max_player_slots;k++) {
	      if (s->g_l[i]->player_slots[k-1] == NULL) {
		s->g_l[i]->player_slots[k-1] = s->g_l[i]->player_slots[k];
		s->g_l[i]->player_slots[k] = NULL;
	      }
	    }
	    gr = s->g_l[i];
	    //Need to notify the game room users
	    memset(msg,0,sizeof(msg));
	    create_chuchu_game_menu(msg, gr);
	    return;
	  }
	}
      }
    }
  }
}

/*
 * Function: join_game_room
 * --------------------
 *
 * When a user joins  a game room, this function
 * adds the user to the game rooms.
 * 
 *  *pl: ptr to player struct
 *  *gr: ptr to game room struct
 *
 *  returns: void
 *
 */
void join_game_room(player_t *pl, game_room_t *gr) {
  int i;
  server_data_t *s = pl->data;
  int max_clients = s->m_cli;
  
  //Sanity check....probably not needed anymore
  validate_game_room(gr, pl);
  
  for (i=0;i<max_clients;i++) {
    if(!gr->player_slots[i]) {
      chuchu_info(LOBBY_SERVER,"User %s joined room %s", pl->username, gr->g_name); 
      gr->player_slots[i] = pl;
      gr->taken_seats++;
      discordRoomJoined(s, pl->username, gr->g_name);
      return;
    }
  }
}


/*
 * Function: check_after_old_game_rooms
 * --------------------
 * Checks after user created game rooms, is triggered
 * when somebody tries to create a new game room, will
 * remove the game room if it is older then 5h. This is
 * so the server is not filled up with game rooms.
 * 
 *  *s: ptr to server data struct
 *
 *  returns: void
 *
 */
void check_after_old_game_rooms(server_data_t *s) {
  int i=0,j=0;
  uint32_t now=0,duration=0;
  time_t seconds=0;
  game_room_t *gr;
  int max_rooms = s->m_rooms;
  seconds = time(NULL);
  now = (uint32_t)(seconds/3600);
  
  for(i=0;i<max_rooms;i++) {
    if (s->g_l[i] != NULL) {
      if (s->g_l[i]->static_room == 0) {
	duration = (s->g_l[i]->duration)/3600;
	//If game room is empty and created more then 24 h ago, remove
	if (s->g_l[i]->taken_seats == 0 && (now - duration) >= 24) {
	  chuchu_info(LOBBY_SERVER,"Game room %s, been active %d h...remove",s->g_l[i]->g_name, (now - duration));
	  gr = s->g_l[i];
	  free(gr);
	  s->g_l[i] = NULL;
	}
      }
    }
  }  
  //Rearrange array
  for (j=1;j<max_rooms;j++) {
    if (s->g_l[j-1] == NULL) {
      s->g_l[j-1] = s->g_l[j];
      s->g_l[j] = NULL;
    }
  }
}

/*
 * Function: create_game_room
 * --------------------
 * 
 * Function to create a game room
 *
 *  *s: ptr to server data struct
 *  *username: ptr to username that created the game room
 *  *buf: ptr to incoming client msg
 *
 *  returns: 1 => OK
 *           0 => FAILED
 */
int create_game_room(server_data_t *s, const char* username, char* buf) {
  int i=0, passwd_protected=0;
  int max_player_slots = s->m_pl_slots;
  int max_rooms = s->m_rooms;
  char room_name[MAX_UNAME_LEN], room_password[MAX_PASSWD_LEN], msg[MAX_PKT_SIZE];
  time_t seconds;
  seconds = time(NULL);

  memset(room_name, 0, sizeof(room_name));
  memset(room_password, 0, sizeof(room_password));
  
  strlcpy(room_name, &buf[0xc], sizeof(room_name));
  strlcpy(room_password, &buf[0x1c], sizeof(room_password));
  
  check_after_old_game_rooms(s);
  
  if (room_name[0] == '\0' || strlen(room_name) == 0) {
    chuchu_error(LOBBY_SERVER, "Player did not enter a game room name [SKIP]");
    return 0;
  }

  if (room_password[0] != '\0' && strlen(room_password) != 0) {
    passwd_protected = 1;
  }
  
  for(i=0;i<max_rooms;i++) {
    if (s->g_l[i] == NULL) {
      game_room_t *gr = (game_room_t *)malloc(sizeof(game_room_t));
      strlcpy(gr->g_name, room_name, sizeof(gr->g_name));
      
      if (passwd_protected) {
	strlcpy(gr->g_passwd, room_password, sizeof(gr->g_passwd));
      } else {
	gr->g_passwd[0] = '\0';
      }
      strlcpy(gr->creator, username, sizeof(gr->creator));

      gr->l_icon = TEAM_ICON;
      gr->r_icon = MICE_ICON;
      gr->menu_id = (uint32_t)GAME_MENU;
      gr->item_id = (uint32_t)(0x2000 + i);
      gr->taken_seats = 0;
      gr->passwd_protected = passwd_protected;
      gr->static_room = 0;
      gr->duration = (uint32_t)seconds;
      gr->m_pl_slots = max_player_slots;
      gr->player_slots = calloc((size_t)max_player_slots, sizeof(player_t *));
      s->g_l[i] = gr;

      //Rebuild room menu
      memset(msg,0,sizeof(msg));
      create_chuchu_room_menu(s, msg);
      chuchu_info(LOBBY_SERVER,"User %s created game room %s",username,room_name);
      return 1;
    }
  }
  return 0;
}

/*
 * Function: if_session_exists
 * --------------------
 * 
 * Function to try to fix issues when a user
 * has timed out and then reconnects really fast.
 * Find the old sessions, store ranking to DB if any.
 *
 *  *s: ptr to server data struct
 *  *pl: ptr to player struct
 *
 *  returns: void
 *
 */
void if_session_exists(server_data_t *s, player_t *pl) {
  int i=0;
  int max_clients = s->m_cli;

  for(i=0;i<max_clients;i++) {
    if (s->p_l[i] != NULL && s->p_l[i] != pl) {
      if (s->p_l[i]->authorized == 1) {
	if(memcmp(pl->dreamcast_id, s->p_l[i]->dreamcast_id, 6) == 0) {
	  chuchu_info(LOBBY_SERVER,"Found a old session for %s", pl->username);
	  //Set so the old sessions doesn't store in the DB
	  s->p_l[i]->store_ranking = 0;
	  //Copy ranking values to DB before reading the old
	  if ((s->p_l[i]->won_rnds != 0) || (s->p_l[i]->lost_rnds != 0)) {
	    pl->won_rnds = s->p_l[i]->won_rnds;
	    pl->lost_rnds = s->p_l[i]->lost_rnds;
	    pl->total_rnds = s->p_l[i]->total_rnds;
	    if (!update_player_ranking_to_chuchu_db(s->chu_db_path, pl)) {
	      chuchu_error(LOBBY_SERVER,"Could not update player %s stats", pl->username);
	    }
	    pl->won_rnds = 0;
	    pl->lost_rnds = 0;
	    pl->total_rnds = 0;
	  }
	  return;
	}
      }
    }
  }
}

/*
 * Function: add_player
 * --------------------
 * 
 * Function to add player to the server struct 
 *
 *  *s: ptr to server data struct
 *  *pl: ptr to player struct
 *
 *  returns: 1 => OK
 *           0 => FAILED
 *
 */
int add_player(server_data_t *s, player_t *pl) {
  int i;
  int max_clients = s->m_cli;

  //Init values
  memset(pl->username, 0, MAX_UNAME_LEN);
  memset(pl->dreamcast_id, 0, 6);
  pl->controllers = 1;
  pl->menu_id = SERVER_MENU;
  pl->item_id = 0x00000000;
  pl->won_rnds = 0;
  pl->lost_rnds = 0;
  pl->total_rnds = 0;
  pl->db_won_rnds = 0;
  pl->db_lost_rnds = 0;
  pl->db_total_rnds = 0;
  pl->store_ranking = 1;
  pl->authorized = 0;
  pl->created_game_room = 0;

  for(i=0;i<max_clients;i++) {
    if(!(s->p_l[i])) {
      s->p_l[i] = pl;
      chuchu_info(LOBBY_SERVER,"Added client: 0x%02x", pl->client_id);
      return 1;
    }
  }
  chuchu_info(LOBBY_SERVER,"Could not add client: 0x%02x", pl->client_id);
  return 0;
}

/*
 * Function: delete_player
 * --------------------
 * 
 * Function to delete player from the server struct
 * store ranking/highscore in DB
 * remove player if in any game room
 *
 *  *pl: ptr to player struct
 *
 *  returns: void
 *           
 */
void delete_player(player_t *pl){
  server_data_t *s = pl->data;
  lock_server(s);
  int max_clients = s->m_cli;
  char u_name[MAX_UNAME_LEN];
  int i=0;

  if ((pl->authorized == 1) && (pl->store_ranking == 1)) {
    if ((pl->won_rnds != 0) || (pl->lost_rnds != 0)) {
      if (!update_player_ranking_to_chuchu_db(s->chu_db_path, pl)) {
	chuchu_error(LOBBY_SERVER,"Could not update player %s stats", pl->username);
      }
    } 
    leave_game_room(pl, pl->menu_id, pl->item_id);
    memset(u_name, 0, sizeof(u_name));
    strlcpy(u_name, pl->username, sizeof(u_name));
    send_txt_to_all(s, u_name, LEAVE_SERVER);
  }

  for(i=0;i<max_clients;i++) {
    if (s->p_l[i] != NULL) {
      if(s->p_l[i]->client_id == pl->client_id) {
	chuchu_info(LOBBY_SERVER,"Removed client: 0x%02x", pl->client_id);
	s->p_l[i] = NULL;
      }
    }
  }
  unlock_server(s);
}

/*
 * Function: create_chuchu_menu_item
 * --------------------
 * 
 * Function to create a menu item, used in pkt 0x07
 *
 *  *msg: ptr to outgoing client msg
 *  c_pkt_size: current packet size
 *  item_id: id of the created menu item
 *  menu_id: id of the created menu
 *  menu_icon_left: icon displayed on the left of the menu text
 *  menu_icon_right: icon displayed on the right of the left icon.
 *  *item_str: Name of the menu item, will be displayed next to the icons
 *
 *  returns: packet size
 *           
 */
uint16_t create_chuchu_menu_item(char *msg, int c_pkt_size, uint32_t item_id, MENU_ITEM_ID menu_id, MENU_ICON menu_icon_left, MENU_ICON menu_icon_right, const char* item_str) {

  c_pkt_size += uint32_to_char(menu_id, &msg[c_pkt_size]);
  c_pkt_size += uint32_to_char(item_id, &msg[c_pkt_size]);
    
  msg[c_pkt_size] = menu_icon_left;
  msg[c_pkt_size + 1] = menu_icon_right;

  strcpy(&msg[c_pkt_size + 2], item_str);
  
  //index c_pkt_size + 19 sets different types of rooms, for example if it requires a password.
  if (menu_icon_left == CREATE_TEAM_ICON) {
    msg[c_pkt_size + 19] = (char)(0xff);
  }

  c_pkt_size = (c_pkt_size + 0x14);
  
  return (uint16_t)c_pkt_size;
}


/*
 * Function: create_chuchu_server_menu
 * --------------------
 * 
 * Function to create the menu display (0x07)
 * Chuchu hdr flag has the nr of menu entries in packet. 
 *
 *  *msg: ptr to outgoing client msg
 *
 *  returns: packet size
 *           
 */
uint16_t create_chuchu_server_menu(server_data_t* s, char* msg) {
  uint8_t entries = 0;
  uint16_t pkt_size = 4;
  if (s->deedee_server) {
    pkt_size = create_chuchu_menu_item(msg, pkt_size, 0x00, SERVER_MENU, SERVER_ICON, EMPTY_ICON, "Dee Dee Server");
    entries = 1;
  }
  else {
    pkt_size = create_chuchu_menu_item(msg, pkt_size, 0x00, SERVER_MENU, SERVER_ICON, EMPTY_ICON, "ChuChu Server");
    pkt_size = create_chuchu_menu_item(msg, pkt_size, 0xdd, SERVER_MENU, MEMO_ICON, EMPTY_ICON, "Top News");
    pkt_size = create_chuchu_menu_item(msg, pkt_size, 0xcc, SERVER_MENU, MEMO_ICON, EMPTY_ICON, "Top Ranking");
    entries = 4;
  }
  pkt_size = create_chuchu_menu_item(msg, pkt_size, 0x00, ROOM_MENU, DOOR_ICON, EMPTY_ICON, "Game Rooms");
  if (!s->deedee_server)
    pkt_size = create_chuchu_menu_item(msg, pkt_size, 0x00, PUZZLE_LAND_MENU, DOOR_ICON, EMPTY_ICON, "Puzzle Land");

  //Create header
  create_chuchu_hdr(msg, 0x07, entries, pkt_size);
  return pkt_size;
}


/*
 * Function: create_chuchu_room_menu
 * --------------------
 * 
 * Function to create the room menu display (0x07)
 * Chuchu hdr flag has the nr of menu entries in packet. 
 *
 *  *s:   ptr to server data struct
 *  *msg: ptr to outgoing client msg
 *
 *  returns: packet size
 *           
 */
uint16_t create_chuchu_room_menu(server_data_t* s, char* msg) {
  uint16_t pkt_size=4;
  int i=0,entries=0;
  int max_rooms = s->m_rooms;
  int max_clients = s->m_cli;
  
  pkt_size = create_chuchu_menu_item(msg, pkt_size, 0x00, ROOM_MENU, DOOR_ICON, EMPTY_ICON, "Game Rooms");
  pkt_size = create_chuchu_menu_item(msg, pkt_size, 0xee, SERVER_MENU, EXIT_ICON, EMPTY_ICON, "Exit");
  pkt_size = create_chuchu_menu_item(msg, pkt_size, 0xcc, ROOM_MENU, CREATE_TEAM_ICON, EMPTY_ICON, "Create Game Room");
  entries += 2;

  //Add all rooms
  for(i=0;i<max_rooms;i++) {
    if (s->g_l[i]) {
      pkt_size = create_chuchu_menu_item(msg, pkt_size, s->g_l[i]->item_id, s->g_l[i]->menu_id, TEAM_ICON, s->g_l[i]->r_icon, s->g_l[i]->g_name);
      if (s->g_l[i]->passwd_protected) {
	msg[pkt_size - 1] = (char)(0xff);
      }
      entries++;
    }
  }
  
  //Add all users in the rooms menu
  for(i=0;i<max_clients;i++) {
    if(s->p_l[i] && (s->p_l[i]->authorized == 1)) {
      if (s->p_l[i]->menu_id == ROOM_MENU)
	{
	  pkt_size = create_chuchu_menu_item(msg, pkt_size, s->p_l[i]->client_id, ROOM_MENU, GUY_ICON, MICE_ICON, s->p_l[i]->username);
	  entries++;
	}
    }
  }
  //Create header
  create_chuchu_hdr(msg, 0x07, (uint8_t)entries, pkt_size);

  //Update all other users in the rooms menu
  for(i=0;i<max_clients; i++) {
    if(s->p_l[i] && (s->p_l[i]->authorized == 1)) {
	if (s->p_l[i]->menu_id == ROOM_MENU)
	  send_chuchu_crypt_msg(s->p_l[i]->sock, &s->p_l[i]->server_cipher, msg, pkt_size);
      }
  }
  
  return 0;
}

/*
 * Function: create_chuchu_puzzle_land_menu
 * --------------------
 * 
 * Function to create the puzzle land menu display (0x07)
 * Chuchu hdr flag has the nr of menu entries in packet. 
 *
 *  *msg: ptr to outgoing client msg
 *
 *  returns: packet size
 *           
 */
uint16_t create_chuchu_puzzle_land_menu(char *msg) {
  uint16_t pkt_size = 4;
  int entries = 0;
  pkt_size = create_chuchu_menu_item(msg, pkt_size, 0x00, PUZZLE_LAND_MENU, DOOR_ICON, EMPTY_ICON, "Puzzle Land");
  pkt_size = create_chuchu_menu_item(msg, pkt_size, 0xee, SERVER_MENU, EXIT_ICON, EMPTY_ICON, "Exit");
  pkt_size = create_chuchu_menu_item(msg, pkt_size, 0x00, PUZZLE_ZONE_MENU, PUZZLE_FLAG_ICON, EMPTY_ICON, "Puzzle zone");
  pkt_size = create_chuchu_menu_item(msg, pkt_size, 0xaa, PUZZLE_LAND_MENU, PUZZLE_UPLOAD_ICON, EMPTY_ICON, "Upload puzzle");
  entries += 3;

  //Create header
  create_chuchu_hdr(msg, 0x07, (uint8_t)entries, pkt_size);
  return pkt_size;
}

/*
 * Function: create_chuchu_puzzle_zone_menu
 * --------------------
 * 
 * Function to create the puzzle zone menu display (0x07)
 * Chuchu hdr flag has the nr of menu entries in packet. 
 *
 *  *msg: ptr to outgoing client msg
 *
 *  returns: packet size
 *           
 */
uint16_t create_chuchu_puzzle_zone_menu(server_data_t *s, char* msg) {
  uint16_t pkt_size = 4;
  int entries, i, max_puzzles = s->m_puzz;
  
  pkt_size = create_chuchu_menu_item(msg, pkt_size, 0x00, PUZZLE_ZONE_MENU, PUZZLE_FLAG_ICON, EMPTY_ICON, "Puzzle zone");
  pkt_size = create_chuchu_menu_item(msg, pkt_size, 0xee, PUZZLE_LAND_MENU, EXIT_ICON, EMPTY_ICON, "Exit");
  entries=1;

  //Get all puzzles
  for(i=0;i<max_puzzles;i++) {
    if(s->puzz_l[i]) {
      pkt_size = create_chuchu_menu_item(msg, pkt_size, s->puzz_l[i]->id, PUZZLE_ZONE_FILE, PUZZLE_DOWNLOAD_ICON, EMPTY_ICON, s->puzz_l[i]->p_name);
      entries++;
    }
  }
  create_chuchu_hdr(msg, 0x07, (uint8_t)entries, pkt_size);
  return pkt_size;
}

/*
 * Function: create_chuchu_game_menu
 * --------------------
 * 
 * Function to create the game room menu display (0x07)
 * Chuchu hdr flag has the nr of menu entries in packet. 
 *
 *  *msg: ptr to outgoing client msg
 *  *gr;  ptr to game room struct
 *
 *  returns: packet size
 *           
 */
uint16_t create_chuchu_game_menu(char* msg, game_room_t *gr) {
  uint16_t pkt_size = 4;
  int entries=0,i=0;
  int max_player_slots = gr->m_pl_slots;
  
  pkt_size = create_chuchu_menu_item(msg, pkt_size, gr->item_id, GAME_MENU, TEAM_ICON, EMPTY_ICON, gr->g_name);
  pkt_size = create_chuchu_menu_item(msg, pkt_size, 0xee, ROOM_MENU, EXIT_ICON, EMPTY_ICON, "Exit");
  pkt_size = create_chuchu_menu_item(msg, pkt_size, 0xff, GAME_MENU, GO_ICON, EMPTY_ICON, "Start Game");
  entries = 2;

  //Get all players in the room
  for(i=0;i<max_player_slots;i++) {
    if(gr->player_slots[i]) {
      pkt_size = create_chuchu_menu_item(msg, pkt_size, gr->player_slots[i]->client_id, GAME_MENU, GUY_ICON, MICE_ICON, gr->player_slots[i]->username);
      entries++;
    }
  }
  //Create header
  create_chuchu_hdr(msg, 0x07, (uint8_t)entries, pkt_size);

  //Update all other users in game_room, so they can see the newly joined player
  for(i=0;i<max_player_slots;i++)
    if(gr->player_slots[i])
      send_chuchu_crypt_msg(gr->player_slots[i]->sock, &gr->player_slots[i]->server_cipher, msg, pkt_size);
  
  return 0;
}


/*
 * Function: create_chuchu_menu_msg
 * --------------------
 * 
 * Help functions that checks where the user is going and updating the menus
 * menu_id and item_id, it's just to navigate through the menus, 
 * when a user presses a item, a 0x10 pkt will be sent with the menu and 
 * item id of the selection, then it is up to the server to create 
 * a 0x07 with menu-item.
 *
 *  *pl: ptr to player struct
 *  menu_id: id of the menu
 *  item_id: id of the item
 *  *msg: ptr to outgoing client msg
 *
 *  returns: packet size
 *           
 */
uint16_t create_chuchu_menu_msg(player_t *pl, uint32_t menu_id, uint32_t item_id, char* msg) {
  uint16_t pkt_size = 0;
  uint32_t prev_menu_id, prev_item_id;
  int rc = 0;
  game_room_t *gr;
  server_data_t *s = pl->data;
  MENU_ITEM_ID id = menu_id;

  //Store prev state and update to the new
  prev_menu_id = pl->menu_id;
  prev_item_id = pl->item_id;
  pl->menu_id = menu_id;
  pl->item_id = item_id;
   
  switch(id) {
  case SERVER_MENU:
    if (item_id == 0xcc)
      return create_chuchu_info_msg(msg, s, RANKING);
    if (item_id == 0xdd)
      return create_chuchu_info_msg(msg, s, NEWS);
    return create_chuchu_server_menu(s, msg);
  case ROOM_MENU:
    if(item_id == 0xee)
      leave_game_room(pl, prev_menu_id, prev_item_id);
    return create_chuchu_room_menu(s, msg);
  case GAME_MENU:
    if (item_id == 0xff) {
      gr = get_room_from_item_id(s, prev_item_id);
      if (gr == NULL)
	return 0;
      //Do we have atleast two players?
      if (gr->taken_seats < 2) {
	pkt_size = create_chuchu_notify_msg(msg, 0x01);
	send_chuchu_crypt_msg(pl->sock, &pl->server_cipher, msg, pkt_size);
	pl->menu_id = prev_menu_id;
	pl->item_id = prev_item_id; 
	return 0;
      }
      //Start the game
      discordGameStart(gr);
      return create_chuchu_start_game_msg(gr);
    } 
    //Else a player is joing a game room
    gr = get_room_from_item_id(s, item_id); 
    if (gr == NULL)
      return 0; 
    //If the game room is full send a notify and restore position of player
    //This checks how many taken seats there are in the game room, but also if there are seats 
    //for "several controllers", for example, 3 taken seats and a player with two people/controllers
    //wants to join = Denied.
    if (gr->taken_seats >= 4 || ((pl->controllers + gr->taken_seats) > 4)) {
      pkt_size = create_chuchu_notify_msg(msg, 0x02);
      send_chuchu_crypt_msg(pl->sock, &pl->server_cipher, msg, pkt_size);
      pl->menu_id = prev_menu_id;
      pl->item_id = prev_item_id; 
      return 0;
    }
    //Else update the game room with the new player
    join_game_room(pl, gr);
    return create_chuchu_game_menu(msg, gr);
    //Puzze land has been pressed
  case PUZZLE_LAND_MENU:
    if (item_id == 0xaa) {
      pkt_size = 12;
      //Sends 0x14, meaning pick which puzzle to upload
      create_chuchu_hdr(msg, 0x14, 0x00, pkt_size);
      return pkt_size;
    }
    //Else update the menu to puzzle land
    pkt_size = create_chuchu_puzzle_land_menu(msg);
    break;
  case PUZZLE_ZONE_MENU:
    pkt_size = create_chuchu_puzzle_zone_menu(s, msg);
    pkt_size = (uint16_t)(pkt_size + create_chuchu_add_info(s, &msg[pkt_size], PUZZLE_ZONE_MENU, 0x00));
    break;
  case PUZZLE_ZONE_FILE:
    //Item id is the id of the rowid in the DB
    pkt_size = (uint16_t)read_puzzle_in_chuchu_db(s, s->chu_db_path, &msg[4], item_id);
    //Something went wrong, sent notify msg
    if (pkt_size == 0) {
      pkt_size = create_chuchu_notify_msg(msg, 0x05);
      return pkt_size;
    }
    //Send puzzle data
    rc = update_puzzle_downloaded_to_chuchu_db(s, s->chu_db_path, item_id);
    if (rc != 1)
      chuchu_info(LOBBY_SERVER,"Could not update puzzle downloaded");
    
    pkt_size = (uint16_t)(pkt_size + 4);
    create_chuchu_hdr(msg, 0x13, 0x00, pkt_size);
    break;
  default:
    pkt_size = create_chuchu_server_menu(s, msg);
    break;
  }
  
  return pkt_size;
}

/*
 * Function: send_txt_to_all
 * --------------------
 * 
 * Function that echos if a user joins
 * or leaves the server and if a new 
 * game room has been created.
 *
 *  *s: ptr to server data struct
 *  *username: which user left/joined?
 *  txt_flag: left,joined,created game room
 *
 *  returns: void
 *           
 */
void send_txt_to_all(server_data_t *s, char* username, int txt_flag) {
  char msg[MAX_PKT_SIZE], chat_msg[64];
  uint16_t pkt_size = 0;
  uint32_t menu_id = 0, item_id = 0;
  int i, max_client = s->m_cli;

  memset(chat_msg, 0, sizeof(chat_msg));
  memset(msg, 0, sizeof(msg)); 
    
  pkt_size = (uint16_t)(pkt_size + 4);
  pkt_size = (uint16_t)(pkt_size + uint32_to_char(menu_id, &msg[pkt_size]));
  pkt_size = (uint16_t)(pkt_size + uint32_to_char(item_id, &msg[pkt_size]));
  
  //Join txt/Left txt
  switch(txt_flag) {
  case JOIN_SERVER:
    snprintf(chat_msg, sizeof(chat_msg), "[%s]: *** joined the server ***\n", username);
    break;
  case LEAVE_SERVER:
    snprintf(chat_msg, sizeof(chat_msg), "[%s]: *** left the server ***\n", username);
    break;
  case NEW_GAME_ROOM:
    snprintf(chat_msg, sizeof(chat_msg), "[%s]: *** created a new game room ***\n", username);
    break;
  default:
    return;
  }
  strcpy(&msg[pkt_size], chat_msg);

  pkt_size = (uint16_t)(pkt_size + strlen(chat_msg));
  //Padding
  pkt_size = (uint16_t)(pkt_size + 2);
  //Create header
  create_chuchu_hdr(msg, 0x06, 0x00, pkt_size);
  //Send to all
  for(i=0;i<max_client;i++) {
    if(s->p_l[i] && (s->p_l[i]->authorized == 1)) {
      send_chuchu_crypt_msg(s->p_l[i]->sock, &s->p_l[i]->server_cipher, msg, pkt_size);
    }
  }
}

/*
 * Function: handle_chuchu_msg
 * --------------------
 * 
 * Function that processes the incoming
 * msg/pkt.
 *
 *  *pl: ptr to player data struct
 *  *msg: ptr to incoming client msg
 *  *buf: ptr to outgoing client msg
 *
 *  returns: packet size or -1
 *           
 */
int handle_chuchu_msg(player_t *pl, char* msg, char* buf) {
  char username[MAX_UNAME_LEN], puzzlename[MAX_UNAME_LEN], entered_passwd[MAX_PASSWD_LEN];
  char join_chat_msg[64];
  uint8_t msg_id=0, msg_flag=0;
  uint16_t msg_size=0, msg_len=0;
  uint32_t menu_id=0, item_id=0;
  server_data_t *s = (server_data_t*)pl->data;
  int rc = 0;

  memset(username, 0, sizeof(username));
  memset(entered_passwd, 0, sizeof(entered_passwd));
  memset(puzzlename, 0, sizeof(puzzlename));
  memset(join_chat_msg, 0, sizeof(join_chat_msg));
  
  msg_id = (uint8_t)buf[0];
  msg_flag = (uint8_t)buf[1];
  msg_len = ntohs(char_to_uint16(&buf[2]));

  switch(msg_id) {
  case RESENT_LOGIN_REQUEST_MSG:
    //Connect msg
    if (msg_len == 0x34 || msg_len == 0x98) {
      strlcpy(username, &buf[0x14], sizeof(username));
      if(strlen(username) == 0 || username[0] == '\0') {
	//Send server is full
	msg_size = create_chuchu_resent_login_request_msg(msg, 0x01, 4);
	return msg_size;
      }
      //Print this for the first packet
      if (strcmp(pl->username, username) != 0) {
	chuchu_info(LOBBY_SERVER," <- Username: %s is trying to join...", username);
	strlcpy(pl->username, username, sizeof(pl->username));
	memcpy(pl->dreamcast_id, &buf[0x06], 6);

	//Get stats/ranking aswell
	rc = read_ranking_from_chuchu_db(s->chu_db_path, pl); 
	if (rc == 1) {
	  chuchu_info(LOBBY_SERVER,"Stats fetched for username: %s", username);
	} else {
	  //If this happens disconnnect user
	  chuchu_error(LOBBY_SERVER,"Could not get stats for username: %s", username);
	  return -1;
	}
	//Find if a old session exist and get the stat and save it.
	if_session_exists(s,pl);
	
	//Set authorized
	pl->authorized = 1;
      }
      //Amount of controllers
      if (msg_flag != 0x00) {
	pl->controllers = msg_flag;
	chuchu_info(LOBBY_SERVER," <- 0x%02x players on this dreamcast wants to play", msg_flag);
	send_txt_to_all(s, username, JOIN_SERVER);
      }
      //Ack. the DC ID again, sets it on the DC, a must to play the game
      memcpy(&msg[0x04], &buf[0x06], 6);
      msg_size = create_chuchu_resent_login_request_msg(msg, 0x00, 12); 
      msg_size = (uint16_t)(msg_size + create_chuchu_menu_msg(pl, SERVER_MENU, 0x00, &msg[msg_size])); 
    } else {
      chuchu_error(LOBBY_SERVER,"RESENT LOGIN REQUEST MSG is corrupt");
      return -1;
    }
    break;

  case MENU_CHANGE_MSG:      
    if (msg_len >= 0x0c) {
      menu_id = char_to_uint32(&buf[4]);
      item_id = char_to_uint32(&buf[8]);
      //If somebody wants to create a game room
      if(msg_len == 0x2c && msg_flag == 0x01 && item_id == 0xcc) {
	//Check if player already created a game room
	if(pl->created_game_room) {
	  msg_size = create_chuchu_notify_msg(msg, 0x09);
	  break;
	}
	if(create_game_room(s,pl->username, buf)) {
	  //Game room created
	  pl->created_game_room = 1;
	  // TODD This seems to confuse the creator of the room in some cases?
	  send_txt_to_all(s, pl->username, NEW_GAME_ROOM);
	  return 0;
	} else {
	  //Something went wrong
	  msg_size = create_chuchu_notify_msg(msg, 0x03);
	}
	break;
      } else if (msg_len == 0x2c && msg_flag == 0x01 && item_id >= 0x2000) {
	chuchu_info(LOBBY_SERVER, "Trying to join password protected game room");
	game_room_t *gr = get_room_from_item_id(s, item_id);
	if (gr->passwd_protected) {
	  strlcpy(entered_passwd, &buf[0x1c], sizeof(entered_passwd));

	  if (strcmp(gr->g_passwd, entered_passwd) != 0) {
	    //Invalid password
	    chuchu_error(LOBBY_SERVER, "User %s typed the wrong password",pl->username);
	    msg_size = create_chuchu_notify_msg(msg, 0x08);
	    break;
	  }
	}
      }
      msg_size = create_chuchu_menu_msg(pl, menu_id, item_id, msg);
      break;
    } else {
      chuchu_error(LOBBY_SERVER,"MENU CHANGE MSG is corrupt");
      return -1;
    }
    break;

  case CHAT_MSG:
    if (msg_len >= 0x0c)
      msg_size = create_chuchu_chat_msg(pl, buf, msg);
    break;

  case ADD_INFO_MENU_MSG:
    if (msg_len >= 0x0c) {
      menu_id = char_to_uint32(&buf[4]);
      item_id = char_to_uint32(&buf[8]);
      msg_size = create_chuchu_add_info(s, msg, menu_id, item_id);
    }
    break;

  case UPLOAD_PUZZLE_MSG:
    if (msg_len != 0x0390) {
      msg_size = create_chuchu_notify_msg(msg, 0x03);
      chuchu_error(LOBBY_SERVER,"Overflow no thanks");
      break;
    }
    strlcpy(puzzlename, &buf[4], sizeof(puzzlename));
    
    int n_flag = 0;
    msg_size = create_chuchu_puzzle_land_menu(msg);
    rc = is_puzzle_in_chuchu_db(s->chu_db_path, puzzlename, pl->username);
    //New Puzzle
    if (rc == 0) {
      rc = write_puzzle_in_chuchu_db(s, puzzlename, pl->username, &buf[0x14], (msg_len - 0x14));
      if (rc == 1) {
	//Upload OK
	n_flag = 0x04;
      } else {
	//Something went wrong
	n_flag = 0x03;
      }
    } else if (rc == 1) {
      //Puzzle name is already in the DB
      n_flag = 0x06;
    } else {
      chuchu_error(LOBBY_SERVER,"Error in DB");
      //Something went wrong 
      n_flag = 0x03;
    }
    msg_size = (uint16_t)(msg_size + create_chuchu_notify_msg(&msg[msg_size], n_flag));
    break;

  case PLAYER_STAT_MSG:
    if (msg_len == 0x14) {
      uint32_t w_rnd = ntohl(char_to_uint32(&buf[0x08]));
      uint32_t l_rnd = ntohl(char_to_uint32(&buf[0x0c]));
      uint32_t t_rnd = ntohl(char_to_uint32(&buf[0x10]));
      //Update values, will be update to DB after a disconnect
      pl->won_rnds = w_rnd;
      pl->lost_rnds = l_rnd;
      pl->total_rnds = t_rnd;
    } else {
      chuchu_error(LOBBY_SERVER,"PLAYER STAT MSG is corrupt");
      return -1;
    }
    break;

  default:
    print_chuchu_data(buf, msg_len);
    chuchu_info(LOBBY_SERVER,"Client msg not supported id 0x%02x", msg_id);
    msg_size = 0;
    break;
  }
  
  return msg_size;
}

int main(int argc , char *argv[]) {
  int socket_desc , client_sock , c, optval;
  struct sockaddr_in server , client;
  server_data_t s_data;

  //Load cfg and init server_data
  if (!get_chuchu_config(&s_data, argc >= 2 ? argv[1] : "chuchu.cfg"))
    return 0;

  //Load puzzles from DB to array
  if(!load_puzzles_to_array(&s_data))
    return 0;
  
  //Init game rooms
  init_game_rooms(&s_data);
  
  socket_desc = socket(AF_INET , SOCK_STREAM , 0);
  if (socket_desc == -1) {
    chuchu_info(LOBBY_SERVER,"Could not create socket");
  }

  chuchu_info(LOBBY_SERVER,"Socket created");
  
  optval = 1;
  setsockopt(socket_desc, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int));

  server.sin_family = AF_INET;
  server.sin_addr.s_addr = INADDR_ANY;
  server.sin_port = htons( s_data.chu_lobby_port );
  
  if( bind(socket_desc,(struct sockaddr *)&server , sizeof(server)) < 0) {
    perror("Bind failed. Error");
    return 1;
  }
  chuchu_info(LOBBY_SERVER,"Bind done");
  
  listen(socket_desc , 3);

  chuchu_info(LOBBY_SERVER,"Waiting for incoming connections...");
  
  c = sizeof(struct sockaddr_in);
  pthread_t thread_id;
  if (pthread_mutex_init(&s_data.mutex, NULL)) {
	  perror("pthread_mutex_init");
	  return 1;
  }
  
  while( (client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c)) ) {
    chuchu_info(LOBBY_SERVER,"Connection accepted from %s on socket %d", inet_ntoa(client.sin_addr), client_sock);
    //Store player data
    player_t *pl = (player_t *)malloc(sizeof(player_t));
    pl->addr = client;
    pl->sock = client_sock;
    pl->client_id = (uint32_t)(client_sock + 0x0100);
    lock_server(&s_data);
    int success = add_player(&s_data, pl);
    unlock_server(&s_data);
    if (!success) {
	free(pl);
	return 0;
    }
    pl->data = &s_data;
    
    if( pthread_create( &thread_id , NULL ,  chuchu_client_handler , (void*)pl) < 0) {
      perror("Could not create thread");
      return 1;
    }
    
    chuchu_info(LOBBY_SERVER,"Handler assigned");
    pthread_detach(thread_id);
  }
  pthread_mutex_destroy(&s_data.mutex);
  if (client_sock < 0) {
    perror("Accept failed");
    return 1;
  }
  
  return 0;
}

/*
 * Function: chuchu_client_handler
 * --------------------
 * 
 * Function that handles connected
 * clients/players
 *
 *  *data: ptr to player data struct
 *
 *  returns: void
 *           
 */
void *chuchu_client_handler(void *data) {
  player_t *pl = (player_t *)data; 
  int sock = pl->sock;
  ssize_t read_size=0;
  ssize_t write_size=0;
  int index=0, n_index=0;
  char c_msg[MAX_PKT_SIZE], s_msg[MAX_PKT_SIZE];

  memset(c_msg, 0, sizeof(c_msg));
  memset(s_msg, 0, sizeof(s_msg));

  struct timeval tv;
  tv.tv_sec = 1800;       /* Timeout in seconds */
  tv.tv_usec = 0;
  setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO,(char *)&tv,sizeof(struct timeval));
  setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,(char *)&tv,sizeof(struct timeval));

  init_chuchu_crypt(pl);
  
  //Send inital message to the client
  write_size = create_chuchu_copyright_msg(pl,s_msg,LOBBY_SERVER);
  if (write_size != 0) {
    send_chuchu_msg(sock, s_msg , (int)write_size);
    memset(s_msg, 0, sizeof(s_msg));
  } else {
    delete_player(pl); 
    free(pl);
    return 0;
  }

  //Receive a message from client
  while( (read_size = recv(sock , c_msg , sizeof(c_msg) , 0)) > 0 ) {
    //Decrypt msg
    decrypt_chuchu_msg(&pl->client_cipher, c_msg, (long unsigned int)read_size);
    
    //Parse msg
    index = 0;
    n_index = 0;
    while (read_size > 0) {
      if ((n_index = parse_chuchu_msg(&c_msg[index], (int)read_size)) > 0) {
    lock_server((server_data_t *)pl->data);
	//Handle msg, do some initial checks
	write_size = (ssize_t)handle_chuchu_msg(pl, s_msg, &c_msg[index]);
    unlock_server((server_data_t *)pl->data);
	if (write_size > 0)
	  send_chuchu_crypt_msg(sock, &pl->server_cipher, s_msg, (int)write_size);
	if (write_size < 0) {
	  chuchu_info(LOBBY_SERVER,"Client with socket %d is not following protocol - Disconnecting", sock);
	  delete_player(pl);
	  free(pl);
	  close(sock);
	  return 0;
	}
	//Decrease size
	read_size -= n_index;
	//Update pointer in recv buff
	index += n_index;
      } else
	break;
      memset(s_msg, 0, sizeof(s_msg)); 
    }
    memset(c_msg, 0, sizeof(c_msg));
    fflush(stdout);  
  }
  
  if(read_size == 0) {
    chuchu_info(LOBBY_SERVER,"Client with socket %d [%s] disconnected", sock, inet_ntoa(pl->addr.sin_addr));
    close(sock);
    fflush(stdout);
  } else if(read_size == -1) {
    chuchu_info(LOBBY_SERVER,"recv failed");
    close(sock);
    fflush(stdout);
  }
  
  delete_player(pl);
  free(pl);
  
  return 0;
} 
