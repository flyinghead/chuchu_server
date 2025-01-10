/*

  ChuChu MSG functions header for Dreamcast
  Author Shuouma

*/

uint16_t create_chuchu_notify_msg(char *msg, int msg_id);			  
uint16_t create_chuchu_copyright_msg(player_t *pl, char * msg, uint8_t server_type);
uint16_t create_chuchu_auth_msg(char *msg, uint8_t flag, uint16_t pkt_size);
uint16_t create_chuchu_resent_login_request_msg(char *msg, uint8_t flag, uint16_t pkt_size);
uint16_t create_chuchu_chat_msg(player_t *pl, char* buf, char* msg);
uint16_t create_chuchu_start_game_msg(game_room_t *gr);
uint16_t create_chuchu_add_info(server_data_t *s, char* msg, uint32_t menu_id, uint32_t item_id);
uint16_t create_chuchu_info_msg(char* msg,server_data_t *s, int info_flag);
uint16_t create_chuchu_login_msg(char *msg, uint8_t flag);
uint16_t create_chuchu_redirect_msg(uint32_t ip, uint16_t port, char * msg);

