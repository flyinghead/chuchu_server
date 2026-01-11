#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <pthread.h>

#if defined(__BIG_ENDIAN__) || defined(WORDS_BIGENDIAN)
#define LE16(x) (((x >> 8) & 0xFF) | ((x & 0xFF) << 8))
#define LE32(x) (((x >> 24) & 0x00FF) | \
                 ((x >>  8) & 0xFF00) | \
                 ((x & 0xFF00) <<  8) | \
                 ((x & 0x00FF) << 24))
#else
#define LE16(x) x
#define LE32(x) x
#endif

#define MAX_UNAME_LEN 17
#define MAX_PASSWD_LEN 17
#define MAX_PKT_SIZE 4096

typedef struct {
  uint32_t keys[1042];
  uint32_t pc_posn;
} CRYPT_SETUP;

typedef struct {
  char p_name[MAX_UNAME_LEN];
  char u_name[MAX_UNAME_LEN];
  uint32_t id;
  uint16_t dl;
} puzzle_t;

typedef struct {
  int sock;
  uint32_t won_rnds, lost_rnds, total_rnds;
  uint32_t db_won_rnds, db_lost_rnds, db_total_rnds;
  int store_ranking;
  int authorized;
  struct sockaddr_in addr;
  uint32_t client_id;
  uint8_t controllers;
  char username[MAX_UNAME_LEN];
  char dreamcast_id[6];
  uint32_t menu_id;
  uint32_t item_id;
  uint8_t created_game_room;
  
  uint32_t server_seed;
  uint32_t client_seed;
  CRYPT_SETUP client_cipher;
  CRYPT_SETUP server_cipher;
  void *data;
} player_t;

typedef struct {
  char g_name[MAX_UNAME_LEN];
  char g_passwd[MAX_PASSWD_LEN];
  char creator[MAX_UNAME_LEN];
  uint8_t taken_seats;
  uint32_t menu_id;
  uint32_t item_id;
  uint8_t l_icon;
  uint8_t r_icon;
  int m_pl_slots;
  int passwd_protected;
  int static_room;
  uint32_t duration;
  player_t **player_slots;
} game_room_t;

typedef struct {
  int m_puzz;
  int m_cli;
  int m_pl_slots;
  int m_rooms;
  uint16_t chu_login_port;
  uint16_t chu_lobby_port;
  char chu_lobby_ip[INET_ADDRSTRLEN];
  char chu_db_path[256];
  char chu_info_path[256];
  char deedee_server;
  pthread_mutex_t mutex;

  //Data
  puzzle_t **puzz_l;
  player_t **p_l;
  game_room_t **g_l;
} server_data_t;

typedef enum {
  NOTIFY_MSG = 0x01,
  AUTH_MSG = 0x03,
  RESENT_LOGIN_REQUEST_MSG = 0x04,
  DISCONNECT_MSG = 0x05,
  CHAT_MSG = 0x06,
  MENU_LIST_MSG = 0x07,
  ADD_INFO_MENU_MSG = 0x09,
  START_GAME_MSG = 0x0e,
  MENU_CHANGE_MSG = 0x10,
  DOWNLOAD_PUZZLE_MSG = 0x13,
  UPLOAD_PUZZLE_MSG = 0x14,
  LOGIN_USER_INFO_MSG = 0x18,
  REDIRECT_MSG = 0x19,
  PRIV_CHAT_MSG = 0x1a,
  PLAYER_STAT_MSG = 0x1b,
} MSG_ID;
  
typedef enum {
  SERVER_MENU = 0x00000000,
  ROOM_MENU = 0x00000001,
  GAME_MENU = 0x00000002,
  PUZZLE_LAND_MENU = 0x00000003,
  PUZZLE_ZONE_MENU = 0x00000004,
  PUZZLE_ZONE_FILE = 0x00000005,
} MENU_ITEM_ID;

typedef enum {
  LOGIN_SERVER = 0x01,
  LOBBY_SERVER = 0x02,
  SERVER = 0x03,
} SERVER_TYPE;

typedef enum {
  JOIN_SERVER = 0x01,
  LEAVE_SERVER = 0x02,
  RANKING = 0x03,
  NEWS = 0x04,
  NEW_GAME_ROOM = 0x05,
} TXT_TYPE;

typedef enum {
  EMPTY_ICON = 0x00,
  GUY_ICON = 0x01,
  DOOR_ICON = 0x02,
  TEAM_ICON = 0x03,
  SERVER_ICON = 0x04,
  EXIT_ICON = 0x05,
  CREATE_ROOM_ICON = 0x06,
  CREATE_TEAM_ICON = 0x07,
  GO_ICON = 0x08,
  MEMO_ICON = 0x09,
  PUZZLE_DOWNLOAD_ICON = 0xa,
  PUZZLE_UPLOAD_ICON = 0x0b,
  PUZZLE_FLAG_ICON = 0x0c,
  MICE_ICON = 0x0e,
  PSO_ICON = 0x0f,
  DD_ICON = 0x10,
  SONIC_ICON = 0x11,
} MENU_ICON;

typedef enum {
AUTH_NOT_STARTED = 0x00,
  AUTH_STARTED = 0x01,
  AUTH_DONE = 0x02,
  AUTH_BROKEN = 0x03,
} AUTH_PROCESS;

//Print functions
void chuchu_error(SERVER_TYPE type, const char* format, ... );
void chuchu_info(SERVER_TYPE type, const char* format, ... );
void print_all_game_rooms(server_data_t *s);
void print_all_players(server_data_t *s);

//Vmu
int create_vmu_list(server_data_t *s);
int save_puzzle_data(char* buf, player_t *pl);
uint16_t load_puzzle_data(server_data_t *s, char* msg, uint32_t id);

//Parse config
int get_chuchu_config(server_data_t *s, char *fn);

//Handler
void *chuchu_client_handler(void *);
void init_chuchu_crypt(player_t* pl);
  
//Crypt
void CRYPT_DC_MixKeys(CRYPT_SETUP* pc);
void CRYPT_DC_CreateKeys(CRYPT_SETUP* pc, uint32_t val);
void CRYPT_DC_CryptData(CRYPT_SETUP* pc,void* data,unsigned long size);
void crypt_chuchu_msg(CRYPT_SETUP *sp, char *msg, unsigned long msg_size);
void decrypt_chuchu_msg(CRYPT_SETUP *cp, char *msg, unsigned long msg_size);
void send_chuchu_crypt_msg(int sock, CRYPT_SETUP *sp, char* msg, int msg_size);

//Help
uint32_t strlcpy(char *dst, const char *src, size_t size);
uint32_t char_to_uint32(char* data);
uint16_t char_to_uint16(char* data);
int uint32_to_char(uint32_t data, char* msg);
int uint16_to_char(uint16_t data, char* msg);
void print_chuchu_data(void* ds,unsigned long data_size);
void create_chuchu_hdr(char* msg, uint8_t msg_id, uint8_t msg_flag, uint16_t msg_size);
void send_chuchu_msg(int sock, char* msg, int msg_size);
uint16_t parse_chuchu_msg(char* buf, int buf_len);

#ifdef DCNET
//Discord
void discordRoomJoined(server_data_t *server, const char *player, const char *room);
void discordGameStart(game_room_t *gameRoom);
#endif
