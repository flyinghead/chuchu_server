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
 * ChuChu Common functions for Dreamcast
 *
 * Encryption Library 
 * 
 * Functions:
 * void CRYPT_DC_MixKeys(CRYPT_SETUP* pc);
 * void CRYPT_DC_CreateKeys(CRYPT_SETUP* pc, uint32_t val);
 * void CRYPT_DC_CryptData(CRYPT_SETUP* pc,void* data,unsigned long size);
 * 
 * Has been written by Fuzziqer Software copyright 2004
 */

#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <stdarg.h>
#include "chuchu_common.h"

uint32_t strlcpy(char *dst, const char *src, size_t size) {
  char *d = dst;
  const char *s = src;
  size_t n = size;

  if (n != 0 && --n != 0) {
    do {
      if ((*d++ = *s++) == 0)
	break;
    } while (--n != 0);
  }

  if (n == 0) {
    if (size != 0)
      *d = '\0';
    while (*s++);
  }
  
  return (uint32_t)(s - src - 1); 
}

/*
 *  PRINT FUNCTIONS
 */
void chuchu_error(SERVER_TYPE type, const char* format, ... ) {
  va_list args;
  time_t t = time(NULL);
  struct tm tm = *localtime(&t);
  char td_str[64];
  const char* s_str;
  
  memset(td_str, 0, sizeof(td_str));
  snprintf(td_str, sizeof(td_str), "[%04d/%02d/%02d %02d:%02d:%02d]", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
  fprintf(stdout,"%s",td_str);

  if (type == LOGIN_SERVER)
    s_str = "[ChuChu - LoginServer] [ERROR] - ";
  else if (type == LOBBY_SERVER)
    s_str = "[ChuChu - LobbyServer] [ERROR] - ";
  else
    s_str = "[ChuChu - Server] [ERROR] - ";
  
  fprintf(stdout,"%s",s_str);
  va_start(args,format);
  vfprintf(stdout,format,args);
  va_end(args);
  fprintf(stdout,"\n");
}

void chuchu_info(SERVER_TYPE type, const char* format, ... ) {
  va_list args;
  time_t t = time(NULL);
  struct tm tm = *localtime(&t);
  char td_str[64];
  const char* s_str;

  memset(td_str, 0, sizeof(td_str));
  snprintf(td_str, sizeof(td_str), "[%04d/%02d/%02d %02d:%02d:%02d]", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
  fprintf(stdout,"%s",td_str);
  
  if (type == LOGIN_SERVER)
    s_str = "[ChuChu - LoginServer] [INFO] - ";
  else if (type == LOBBY_SERVER)
    s_str = "[ChuChu - LobbyServer] [INFO] - ";
  else
    s_str = "[ChuChu - Server] [INFO] - "; 

  fprintf(stdout,"%s",s_str);
  va_start(args,format);
  vfprintf(stdout,format,args);
  va_end(args);
  fprintf(stdout,"\n");
  fflush(stdout);
}

void print_all_game_rooms(server_data_t *s) {
  int i,j;
  int max_rooms = s->m_rooms;
  int max_player_slots = s->m_pl_slots;
  
  printf("[Chuchu - LobbyServer]\n===============Print ALL Room Data=============\n");
  for(i=0;i<max_rooms;i++) {
    if(s->g_l[i]) {
      printf("\t==========Room Data================\n");
      printf("\tGame Room Name: %s\n",s->g_l[i]->g_name);
      printf("\tMenu id: 0x%08x\n",s->g_l[i]->menu_id);
      printf("\tItem id: 0x%08x\n",s->g_l[i]->item_id);
      printf("\tPlayers in room: 0x%02x\n",s->g_l[i]->taken_seats);
      printf("\t===============Players in room===================\n");
      for (j=0;j<max_player_slots;j++) {
	if (s->g_l[i]->player_slots[j])
	  printf("\t[%d] Player name: %s\n", (j+1), s->g_l[i]->player_slots[j]->username);
      }
      printf("\t=================================================\n\n");
    }
  }
  printf("===============================================\n"); 
}

void print_all_players(server_data_t *s) {
  int i,j;
  int max_clients = s->m_cli;
  
  printf("[Chuchu - LobbyServer]\n===============Print ALL User Data=============\n");
  for(i=0;i<max_clients;i++) {
    if(s->p_l[i] && s->p_l[i]->authorized == 1) {
      printf("\t==========Player Data================\n");
      printf("\tClient ID: 0x%02x\n\tUsername: %s\n\t",s->p_l[i]->client_id,s->p_l[i]->username);
      printf("Dreamcast ID: ");
      for (j=0;j<6;j++) {
	printf("%02x ",(uint8_t)s->p_l[i]->dreamcast_id[j]);
      }
      printf("\n\tMenu ID: 0x%08x\n\tItem ID: 0x%08x\n", s->p_l[i]->menu_id, s->p_l[i]->item_id);
      printf("\t======================================\n");
    }
  }
  printf("===============================================\n");
}


/*
 * Function: get_chuchu_config
 * --------------------
 * parses the chuchu.cfg file into server data struct
 * 
 *  server_data_t *s: pointer to server data struct
 *  fn: filename to read from
 *  buf: pointer to received buffer
 *  auth_state: flag for the authentication state 
 *
 *  returns: 1 => OK
 *           0 => FAIL
 *
 */
int get_chuchu_config(server_data_t *s, char *fn) {
  chuchu_info(SERVER,"Reading config %s...", fn);
  FILE *file = fopen(fn,"r");
  int lobby_port=0, login_port=0;
  int max_puzzles=0, max_clients=0, max_rooms=0,i=0;
  int deedee_server = 0;
  char lobby_ip[16], buf[1024], db_path[256], info_path[256];
  char discord_webhook[256];
  memset(buf, 0, sizeof(buf));
  memset(lobby_ip, 0, sizeof(lobby_ip));
  memset(db_path, 0, sizeof(db_path));
  memset(info_path, 0, sizeof(info_path));
  memset(discord_webhook, 0, sizeof(discord_webhook));
  
  if (file != NULL) {
    while (fgets(buf, sizeof(buf), file) != NULL) {
      sscanf(buf, "CHUCHU_LOGIN_PORT=%d", &login_port);
      sscanf(buf, "CHUCHU_LOBBY_PORT=%d", &lobby_port);
      sscanf(buf, "CHUCHU_LOBBY_IP=%15s", lobby_ip);
      sscanf(buf, "CHUCHU_LOBBY_DB_PATH=%s", db_path);
      sscanf(buf, "CHUCHU_LOBBY_INFO_PATH=%s", info_path);
      sscanf(buf, "CHUCHU_LOBBY_MAX_PUZZLES=%d", &max_puzzles);
      sscanf(buf, "CHUCHU_LOBBY_MAX_CLIENTS=%d", &max_clients);
      sscanf(buf, "CHUCHU_LOBBY_MAX_ROOMS=%d", &max_rooms);
      sscanf(buf, "CHUCHU_LOBBY_DEEDEE=%d", &deedee_server);
      sscanf(buf, "CHUCHU_LOBBY_DISCORD_WEBHOOK=%s", discord_webhook);
    }
    fclose(file);
  } else {
    chuchu_info(SERVER,"Config file %s is missing", fn);
    return 0;
  }

  if (lobby_ip[0] == '\0') {
    chuchu_info(SERVER,"Missing CHUCHU_LOBBY_IP");
    return 0;
  }
  if (db_path[0] == '\0') {
    chuchu_info(SERVER,"Missing CHUCHU_LOBBY_DB_PATH");
    return 0;
  }
  if (info_path[0] == '\0') {
    chuchu_info(SERVER,"Missing CHUCHU_LOBBY_INFO_PATH");
    return 0;
  }
  if (lobby_port == 0) {
    chuchu_info(SERVER,"Missing CHUCHU_LOBBY_PORT");
    return 0;
  }
  if (max_puzzles == 0) {
    chuchu_info(SERVER,"Missing CHUCHU_LOBBY_MAX_PUZZLES - Set to default value");
    s->m_puzz = 96;
  } else {
    s->m_puzz = max_puzzles;
  }
  if (max_clients == 0) {
      chuchu_info(SERVER,"Missing CHUCHU_LOBBY_MAX_CLIENTS - Set to default value");
      s->m_cli = 100;
  } else {
    s->m_cli = max_clients;
  }
  if (max_rooms == 0) {
    chuchu_info(SERVER,"Missing CHUCHU_LOBBY_MAX_ROOMS");
    return 0;
  }
  
  strncpy(s->chu_lobby_ip, lobby_ip, sizeof(lobby_ip));
  strncpy(s->chu_db_path, db_path, sizeof(db_path));
  strncpy(s->chu_info_path, info_path, sizeof(info_path));
  s->chu_lobby_port = (uint16_t)lobby_port;
  if (login_port == 0)
    s->chu_login_port = 9000;
  else
    s->chu_login_port = (uint16_t)login_port;
  s->m_rooms = max_rooms;
  s->m_pl_slots = 4;
  s->deedee_server = (char)deedee_server;
  strncpy(s->discord_webhook, discord_webhook, sizeof(discord_webhook));
  
  chuchu_info(SERVER,"Loaded %s Config:", deedee_server ? "Dee Dee" : "ChuChu");
  chuchu_info(SERVER,"\tCHUCHU_LOGIN_PORT_: %d", s->chu_login_port);
  chuchu_info(SERVER,"\tCHUCHU_LOBBY_IP: %s", s->chu_lobby_ip);
  chuchu_info(SERVER,"\tCHUCHU_LOBBY_PORT: %d", s->chu_lobby_port);
  chuchu_info(SERVER,"\tCHUCHU_LOBBY_DB_PATH: %s", s->chu_db_path);
  chuchu_info(SERVER,"\tCHUCHU_LOBBY_INFO_PATH: %s", s->chu_info_path);
  chuchu_info(SERVER,"\tCHUCHU_MAX_PUZZLES: %d", s->m_puzz);
  chuchu_info(SERVER,"\tCHUCHU_MAX_CLIENTS: %d", s->m_cli);
  chuchu_info(SERVER,"\tCHUCHU_MAX_ROOMS: %d", s->m_rooms);
  chuchu_info(SERVER,"\tCHUCHU_DISCORD_WEBHOOK: %s", s->discord_webhook);
  //Allocate pointer arrays
  s->puzz_l = calloc((size_t)s->m_puzz, sizeof(puzzle_t *));
  for(i=0;i<(s->m_puzz);i++)
    s->puzz_l[i] = NULL;
  
  s->p_l = calloc((size_t)s->m_cli, sizeof(player_t *));
  for(i=0;i<(s->m_cli);i++)
    s->p_l[i] = NULL; 
  
  s->g_l = calloc((size_t)s->m_rooms, sizeof(game_room_t *));
  for(i=0;i<(s->m_rooms);i++)
    s->g_l[i] = NULL; 
  
  return 1;
}

/*
 * HELP FUNCTIONS
 */
uint32_t char_to_uint32(char* data) {
  uint32_t val = (uint32_t)((uint8_t)data[0] << 24 | (uint8_t)data[1] << 16 | (uint8_t)data[2] << 8  | (uint8_t)data[3]);
  return val;
}

uint16_t char_to_uint16(char* data) {
  uint16_t val = (uint16_t)((uint8_t)data[0] << 8 | (uint8_t)data[1]);
  return val;
}

int uint32_to_char(uint32_t data, char* msg) {
  msg[0] = (char)(data >> 24);
  msg[1] = (char)(data >> 16);
  msg[2] = (char)(data >> 8);
  msg[3] = (char)data;
  return 4;
}

int uint16_to_char(uint16_t data, char* msg) {
  msg[0] = (char)(data >> 8);
  msg[1] = (char)data;
  return 2;
}

/*
 * PRINT PACKET as TCPDUMP
 */
void print_chuchu_data(void* pkt,unsigned long pkt_size) {
  unsigned char* pkt_content = (unsigned char*)pkt;
  unsigned long i,j,off;
  char buffer[17];
  buffer[16] = 0;
  off = 0;
  printf("--------------------\n");
  printf("0000 | ");
  for (i = 0; i < pkt_size; i++) {
    if (off == 16) {
      memcpy(buffer,&pkt_content[i - 16],16);
      for (j = 0; j < 16; j++) if (buffer[j] < 0x20) buffer[j] = '.';
      printf("| %s\n%04X | ",buffer,(unsigned int)i);
      off = 0;
    }
    printf("%02X ",pkt_content[i]);
    off++;
  }
  buffer[off] = 0;
  memcpy(buffer,&pkt_content[i - off],off);
  for (j = 0; j < off; j++) if (buffer[j] < 0x20) buffer[j] = '.';
  for (j = 0; j < 16 - off; j++) printf("   ");
  printf("| %s\n",buffer);
}

/*
 * Function: create_chuchu_hdr
 * --------------------
 * create standard chuchu TCP header
 * 
 *  *msg: pointer to msg buffer
 *   msg_id: msg id
 *   msg_flag: msg flag
 *   msg_size: size of msg 
 *
 *  returns: void
 *
 */
void create_chuchu_hdr(char* msg, uint8_t msg_id, uint8_t msg_flag, uint16_t msg_size) {
  msg_size = htons(msg_size);
  msg[0] = (char)msg_id;
  msg[1] = (char)msg_flag;
  uint16_to_char(msg_size, &msg[2]);
}

void send_chuchu_msg(int sock, char* msg, int msg_size) {
  write(sock, msg, (size_t)msg_size);
}

/*
 * Function: parse_chuchu_msg
 * --------------------
 * parse chuchu msg and do some inital sanity checks
 * 
 *   buf: incoming msg
 *   buf_len: length of msg
 *
 *  returns: 0 => SKIP continue
 *           Size states in pkt
 *
 */
uint16_t parse_chuchu_msg(char* buf, int buf_len) {
  uint16_t pkt_size = 0;
  
  //Fetch size in pkt
  pkt_size = ntohs(char_to_uint16(&buf[2]));  
  
  //Parse header
  if (buf_len < 4) {
    chuchu_info(SERVER,"Length of packet is less then 4 bytes...[SKIP]");
    return 0;
  }
  
  if (buf_len >= pkt_size)
    return pkt_size;
  
  if (buf_len < pkt_size) {
    chuchu_info(SERVER,"<- Packet size %d is greater then buffer size %d", (int)pkt_size, (int)buf_len); 
    chuchu_error(SERVER,"Expected more data..[SKIP]");
    return 0;
  }
  
  return 0;
}

/*
 * Function: init_chuchu_crypt
 * --------------------
 * generates server and client seed 
 * and creates the nec. keys
 * 
 *  *pl: pointer to player struct
 *
 *  returns: void
 *
 */
void init_chuchu_crypt(player_t* pl) {
  int i=0;
  srand((unsigned int)time(NULL));
  char server_seed_dc[4],client_seed_dc[4];

  for(i=0;i<4;i++) {
    server_seed_dc[i] = (char)(rand()%16);
    client_seed_dc[i] = (char)(rand()%16);
  }

  pl->server_seed =  char_to_uint32(server_seed_dc);
  pl->client_seed =  char_to_uint32(client_seed_dc);
    
  CRYPT_DC_CreateKeys(&pl->server_cipher, pl->server_seed);
  CRYPT_DC_CreateKeys(&pl->client_cipher, pl->client_seed);  
}

/*
 * Encrypt msg before sending it out
 */
void send_chuchu_crypt_msg(int sock, CRYPT_SETUP *sp, char* msg, int msg_size) {
  char crypt_msg[2048];
  memset(crypt_msg, 0, sizeof(crypt_msg));
  memcpy(crypt_msg, msg, (size_t)msg_size);

  crypt_chuchu_msg(sp, crypt_msg, (long unsigned int)msg_size);
  write(sock, crypt_msg, (size_t)msg_size);
}

/*
 * Crypt functions
 * By Fuzziqer Software copyright 2004
 */
void crypt_chuchu_msg(CRYPT_SETUP *sp, char *msg, unsigned long msg_size) {
  CRYPT_DC_CryptData(sp, msg, msg_size);
}

void decrypt_chuchu_msg(CRYPT_SETUP *cp, char *msg, unsigned long msg_size) { 
  CRYPT_DC_CryptData(cp, msg, msg_size);
}

void CRYPT_DC_MixKeys(CRYPT_SETUP* pc) {
  uint32_t esi,edi,eax,ebp,edx;
  edi = 1;
  edx = 0x18;
  eax = edi;
  while (edx > 0) {
    esi = pc->keys[eax + 0x1F];
    ebp = pc->keys[eax];
    ebp = ebp - esi;
    pc->keys[eax] = ebp;
    eax++;
    edx--;
  }
  edi = 0x19;
  edx = 0x1F;
  eax = edi;
  while (edx > 0) {
    esi = pc->keys[eax - 0x18];
    ebp = pc->keys[eax];
    ebp = ebp - esi;
    pc->keys[eax] = ebp;
    eax++;
    edx--;
  }
}

void CRYPT_DC_CreateKeys(CRYPT_SETUP* pc, uint32_t val) {

  memset(pc, 0, sizeof(CRYPT_SETUP));
  uint32_t esi,ebx,edi,eax,edx,var1;
  esi = 1;
  ebx = val;
  edi = 0x15;
  pc->keys[56] = ebx;
  pc->keys[55] = ebx;
  while (edi <= 0x46E) {
    eax = edi;
    var1 = eax / 55;
    edx = eax - (var1 * 55);
    ebx = ebx - esi;
    edi = edi + 0x15;
    pc->keys[edx] = esi;
    esi = ebx;
    ebx = pc->keys[edx];
  }
  CRYPT_DC_MixKeys(pc);
  CRYPT_DC_MixKeys(pc);
  CRYPT_DC_MixKeys(pc);
  CRYPT_DC_MixKeys(pc);
  pc->pc_posn = 56;
}

static uint32_t CRYPT_DC_GetNextKey(CRYPT_SETUP* pc) {    
  uint32_t re;
  if (pc->pc_posn == 56) {
    CRYPT_DC_MixKeys(pc);
    pc->pc_posn = 1;
  }
  re = pc->keys[pc->pc_posn];
  pc->pc_posn++;
  return re;
}

void CRYPT_DC_CryptData(CRYPT_SETUP* pc,void* data,unsigned long size) {
  uint32_t x, tmp;
  for (x = 0; x < size; x += 4) {
    tmp = *((uint32_t *)(data + x));
    tmp = LE32(tmp) ^ CRYPT_DC_GetNextKey(pc);
    *((uint32_t *)(data + x)) = LE32(tmp);
  }
}
