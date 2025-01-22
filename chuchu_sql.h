#include <sqlite3.h> 

sqlite3* open_chuchu_db(void);
int write_player_to_chuchu_db(const char* db_path, const char* dc_id, const char* u_name, const char* passwd);
int load_puzzles_to_array(server_data_t *s);
int is_player_in_chuchu_db(const char* db_path, const char* name_or_dc_id, int name_search);
int is_puzzle_in_chuchu_db(const char* db_path, const char* p_name, const char* u_name);
int read_puzzle_in_chuchu_db(server_data_t *s, const char* db_path, char* msg, uint32_t id);
int write_puzzle_in_chuchu_db(server_data_t *s, const char* p_name, const char* u_name, char* data, int nData);
int is_username_taken(const char* db_path, const char* u_name);
int validate_player_login(const char* db_path, const char* u_name, const char* passwd, const char* dc_id);
int read_ranking_from_chuchu_db(const char* db_path, player_t* pl);
int update_player_ranking_to_chuchu_db(const char* db_path, player_t* pl);
int read_top_ranking_from_chuchu_db(char* msg, const char* db_path);
int update_puzzle_downloaded_to_chuchu_db(server_data_t *s,const char* db_path, uint32_t id);
