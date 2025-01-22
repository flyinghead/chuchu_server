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
 * SQL functions for Dreamcast
 */

#include <stdlib.h>
#include <sqlite3.h>
#include <assert.h>
#include "chuchu_common.h"

/*
 * Function: open_chuchu_db
 * --------------------
 *
 * Opens a connection to the sqlite3 DB
 * 
 *  *db_path: full path and filename to the DB
 *
 *  returns: sqlite3*: ptr to the open DB.
 *
 */
sqlite3* open_chuchu_db(const char* db_path) {
   sqlite3 *db;
   int rc=0;
   rc = sqlite3_open(db_path, &db);
   if (rc != SQLITE_OK) {
     chuchu_error(SERVER, "Can't open database: %s", sqlite3_errmsg(db));
     return NULL;
   }

   sqlite3_busy_timeout(db, 1000);
   
   return db;
}

/*
 * Function: is_player_in_chuchu_db
 * --------------------
 *
 * Function that checks if a console id or user name is
 * already registered in the DB.
 * 
 *  *db_path: full path and filename to the DB
 *  *name_or_dc_id:  ID of the DC connecting or User name
 *  *name_search:  whether to search for Console ID or User name
 *
 *  returns: nr of users found
 *
 */
int is_player_in_chuchu_db(const char* db_path, const char* name_or_dc_id, int name_search) {
  sqlite3 *db;
  int rc, count = 0;
  sqlite3_stmt *pStmt;
  
  if((db = open_chuchu_db(db_path)) == NULL) {
    return -1;
  }
  
  const char *zSql;
  if (name_search) {
    zSql = "SELECT COUNT(*) from PLAYER_DATA WHERE USERNAME = ?;";
    count = (int)strlen(name_or_dc_id);
  }
  else {
    zSql = "SELECT COUNT(*) from PLAYER_DATA WHERE DC_ID = hex(?);";
    count = 6;
  }
  rc = sqlite3_prepare_v2(db, zSql, -1, &pStmt, 0);
  if( rc != SQLITE_OK ){
    chuchu_error(SERVER, "Prepare SQL error: %d", rc);
    sqlite3_close(db);
    return -1;
  }

  rc = sqlite3_bind_text(pStmt, 1, name_or_dc_id, count, SQLITE_STATIC);
  if (rc != SQLITE_OK) {
    sqlite3_finalize(pStmt);
    chuchu_error(SERVER, "Bind text failed error: %d", rc);
    sqlite3_close(db);
    return 0;
  }
  
  rc = sqlite3_step(pStmt);
  if (rc == SQLITE_ROW ) {
    count = sqlite3_column_int(pStmt, 0);
  }
  
  if (count == 1)
    chuchu_info(SERVER,"Console ID/User '%s' is registered in the DB", name_or_dc_id);
  else
    chuchu_info(SERVER,"Console ID/User '%' is not in the DB", name_or_dc_id);

  sqlite3_finalize(pStmt);
  sqlite3_close(db);
  return count;
}

int is_username_taken(const char* db_path, const char* u_name) {
  sqlite3 *db;
  int rc, count = 0;
  sqlite3_stmt *pStmt;
  
  if((db = open_chuchu_db(db_path)) == NULL) {
    return -1;
  }
  
  const char *zSql = "SELECT COUNT(*) from PLAYER_DATA WHERE USERNAME = trim(?);"; 
  rc = sqlite3_prepare_v2(db, zSql, -1, &pStmt, 0);
  if( rc != SQLITE_OK ){
    chuchu_error(SERVER, "Prepare SQL error: %d", rc);
    sqlite3_close(db);
    return -1;
  }

  rc = sqlite3_bind_text(pStmt, 1, u_name, (int)strlen(u_name), SQLITE_STATIC);
  if (rc != SQLITE_OK) {
    sqlite3_finalize(pStmt);
    chuchu_error(SERVER, "Bind text failed error: %d", rc);
    sqlite3_close(db);
    return 0;
  }
  
  rc = sqlite3_step(pStmt);
  if (rc == SQLITE_ROW ) {
    count = sqlite3_column_int(pStmt, 0);
  }
  
  if (count == 1)
    chuchu_info(SERVER,"Username taken...");
  
  sqlite3_finalize(pStmt);
  sqlite3_close(db);
  return count;
}

/*
 * Function: validate_player_login
 * --------------------
 *
 * Function that checks if a user is
 * already registrated in the DB.
 * 
 *  *db_path: full path and filename to the DB
 *  *u_name:  entered username
 *  *passwd:  entered password
 *  *dc_id:   ID of the DC connecting
 *
 *  returns: nr of users found
 *           1 => OK to login
 *          -1 or 0 => Not validated
 *
 */
int validate_player_login(const char* db_path, const char* u_name, const char* passwd, const char* dc_id) {
#ifdef DISABLE_AUTH
  chuchu_info(SERVER,"Login granted");
  return 1;
#else
  sqlite3 *db;
  int rc, count = 0;
  sqlite3_stmt *pStmt;
  
  if((db = open_chuchu_db(db_path)) == NULL) {
    return -1;
  }
  
  const char *zSql = "SELECT COUNT(*) from PLAYER_DATA WHERE USERNAME = trim(?) AND PASSWORD = trim(?) AND DC_ID = hex(?);"; 
  rc = sqlite3_prepare_v2(db, zSql, -1, &pStmt, 0);
  if( rc != SQLITE_OK ){
    chuchu_error(SERVER, "Prepare SQL error: %d", rc);
    sqlite3_close(db);
    return -1;
  }

  rc = sqlite3_bind_text(pStmt, 1, u_name, (int)strlen(u_name), SQLITE_STATIC);
  if (rc != SQLITE_OK) {
    sqlite3_finalize(pStmt);
    chuchu_error(SERVER, "Bind text failed error: %d", rc);
    sqlite3_close(db);
    return 0;
  }

  rc = sqlite3_bind_text(pStmt, 2, passwd, (int)strlen(passwd), SQLITE_STATIC);
  if (rc != SQLITE_OK) {
    sqlite3_finalize(pStmt);
    chuchu_error(SERVER, "Bind text failed error: %d", rc);
    sqlite3_close(db);
    return 0;
  }

  rc = sqlite3_bind_text(pStmt, 3, dc_id, 6, SQLITE_STATIC);
  if (rc != SQLITE_OK) {
    sqlite3_finalize(pStmt);
    chuchu_error(SERVER, "Bind text failed error: %d", rc);
    sqlite3_close(db);
    return 0;
  }
  
  rc = sqlite3_step(pStmt);
  if (rc == SQLITE_ROW ) {
    count = sqlite3_column_int(pStmt, 0);
  }
  
  if (count == 1)
    chuchu_info(SERVER,"Login granted");
  else
    chuchu_info(SERVER,"Login failed");

  sqlite3_finalize(pStmt);
  sqlite3_close(db);
  return count;
#endif
}

/*
 * Function: write_player_to_chuchu_db
 * --------------------
 *
 * Function that writes player data to
 * the DB
 * 
 *  *db_path: full path and filename to the DB
 *  *dc_id:   ID of the DC connecting
 *  *u_name:  entered username
 *  *passwd:  entered password
 *
 *  returns: 
 *           1 => OK
 *          -1 or 0 => FAILED
 *
 */
int write_player_to_chuchu_db(const char* db_path, const char* dc_id, const char* u_name, const char* passwd) {
  sqlite3 *db = NULL;
  int rc = 0;
  sqlite3_stmt *pStmt;
  
  if((db = open_chuchu_db(db_path)) == NULL) {
    return 0;
  }

  const char* zSql = "INSERT INTO PLAYER_DATA(ID,DC_ID,USERNAME,PASSWORD,WON_RNDS,LOST_RNDS,TOTAL_RNDS) VALUES(NULL, hex(?), trim(?), trim(?), 0, 0, 0);";

  rc = sqlite3_prepare_v2(db, zSql, -1, &pStmt, 0);
  if( rc != SQLITE_OK ){
    chuchu_error(SERVER, "Prepare SQL error: %d", rc);
    sqlite3_close(db);
    return -1;
  }

  rc = sqlite3_bind_text(pStmt, 1, dc_id, 6, SQLITE_STATIC);
  if (rc != SQLITE_OK) {
    sqlite3_finalize(pStmt);
    chuchu_error(SERVER, "Bind text failed error: %d", rc);
    sqlite3_close(db);
    return 0;
  }
  
  rc = sqlite3_bind_text(pStmt, 2, u_name, (int)strlen(u_name), SQLITE_STATIC);
  if (rc != SQLITE_OK) {
    sqlite3_finalize(pStmt);
    chuchu_error(SERVER, "Bind text failed error: %d", rc);
    sqlite3_close(db);
    return 0;
  }

  rc = sqlite3_bind_text(pStmt, 3, passwd, (int)strlen(passwd), SQLITE_STATIC);
  if (rc != SQLITE_OK) {
    sqlite3_finalize(pStmt);
    chuchu_error(SERVER, "Bind text failed error: %d", rc);
    sqlite3_close(db);
    return 0;
  }

  rc = sqlite3_step(pStmt);
  if (rc != SQLITE_DONE) {
    chuchu_error(SERVER, "Insert failed error: %d", rc);
    sqlite3_finalize(pStmt);
    sqlite3_close(db);
    return 0;
  }
  
  sqlite3_finalize(pStmt);
  chuchu_info(SERVER,"Records created successfully");
  sqlite3_close(db);
  
  return 1;
}


/*
 * Function: load_puzzles_to_array
 * --------------------
 *
 * Function that loads stored puzzle
 * information from the DB to memory
 * 
 *  *s: ptr to server data struct
 *
 *  returns: 
 *           1 => OK
 *           0 => FAILED
 *
 */
int load_puzzles_to_array(server_data_t *s) {
  sqlite3 *db;
  int rc=0,index=0;
  sqlite3_stmt *pStmt;
    
  if((db = open_chuchu_db(s->chu_db_path)) == NULL) {
    return 0;
  }

  const char *zSql = "SELECT ID,PUZZLE_NAME,CREATOR,DOWNLOADED from PUZZLE_DATA;"; 
  rc = sqlite3_prepare_v2(db, zSql, -1, &pStmt, 0);
  if( rc != SQLITE_OK ){
    chuchu_error(SERVER, "Prepare SQL error: %d", rc);
    sqlite3_close(db);
    return 0;
  }

  //Puzzle struct is empty here, index is 0 and goes up
  while (sqlite3_step(pStmt) == SQLITE_ROW ) {
    puzzle_t *puz = (puzzle_t *)calloc(1, sizeof(puzzle_t));
    puz->id = (uint32_t)sqlite3_column_int(pStmt, 0);
    strlcpy(puz->p_name, (char*)sqlite3_column_text(pStmt,1), (size_t)sqlite3_column_bytes(pStmt, 1)+1);
    strlcpy(puz->u_name, (char*)sqlite3_column_text(pStmt,2), (size_t)sqlite3_column_bytes(pStmt, 2)+1);
    puz->dl = (uint16_t)sqlite3_column_int(pStmt, 3);
    //Should not happen
    assert(s->puzz_l[index] == NULL);
    s->puzz_l[index] = puz;
    index++;
  }

  chuchu_info(SERVER,"Added %d puzzles",index);
  
  sqlite3_finalize(pStmt);
  sqlite3_close(db);
  return 1;
}

/*
 * Function: is_puzzle_in_chuchu_db
 * --------------------
 *
 * Function that checks if puzzle
 * name is not already taken
 * 
 *  *db_path: full path to DB
 *  *p_name:  name of the puzzle
 *  *u_name:  name of user
 *
 *  returns: nr of puzzles found
 *           1 => OK
 *           ELSE => FAILED
 *
 */
int is_puzzle_in_chuchu_db(const char* db_path, const char* p_name, const char* u_name) {
  sqlite3 *db;
  int rc, count = 0;
  sqlite3_stmt *pStmt;
    
  if((db = open_chuchu_db(db_path)) == NULL) {
    return -1;
  }

  const char *zSql = "SELECT COUNT(*) from PUZZLE_DATA WHERE PUZZLE_NAME = ?;"; 
  rc = sqlite3_prepare_v2(db, zSql, -1, &pStmt, 0);
  if( rc != SQLITE_OK ){
    chuchu_error(SERVER, "Prepare SQL error: %d", rc);
    sqlite3_close(db);
    return -1;
  }

  rc = sqlite3_bind_text(pStmt, 1, p_name, (int)strlen(p_name), SQLITE_STATIC);
  if (rc != SQLITE_OK) {
    sqlite3_finalize(pStmt);
    chuchu_error(SERVER, "Bind text failed error: %d", rc);
    sqlite3_close(db);
    return 0;
  }

  rc = sqlite3_step(pStmt);
  if (rc == SQLITE_ROW ) {
    count = sqlite3_column_int(pStmt, 0);
  }
  
  if (count >= 1)
    chuchu_info(SERVER,"Puzzle is already in the DB");
  
  sqlite3_finalize(pStmt);
  sqlite3_close(db);
  return count;
}

/*
 * Function: write_puzzle_in_chuchu_db
 * --------------------
 *
 * Function that writes puzzle to
 * the DB
 * 
 *  *s: ptr to server data struct
 *  *p_name:  name of the puzzle
 *  *u_name:  name of user
 *  nData: size of puzzle
 *
 *  returns: 
 *           1 => OK
 *        -1,0 => FAILED
 *
 */
int write_puzzle_in_chuchu_db(server_data_t *s, const char* p_name, const char* u_name, char* data, int nData) {
  sqlite3 *db;
  int rc, lastid=0;
  sqlite3_stmt *pStmt;
  const char * db_path = s->chu_db_path;
  
  if((db = open_chuchu_db(db_path)) == NULL) {
    return 0;
  }

  chuchu_info(SERVER,"Storing puzzle: %s in DB", p_name);
  
  const char *zSql = "INSERT INTO PUZZLE_DATA(ID,PUZZLE_NAME,CREATOR,PUZZLE_FILE,DOWNLOADED) VALUES(NULL, ?, ?, ?, 0);";
 
  rc = sqlite3_prepare_v2(db, zSql, -1, &pStmt, 0);
  if(rc != SQLITE_OK) {
    chuchu_error(SERVER, "Prepare SQL statement failed error: %d", rc);
    sqlite3_close(db);
    return 0;
  }

  rc = sqlite3_bind_text(pStmt, 1, p_name, (int)strlen(p_name), SQLITE_STATIC);
  if (rc != SQLITE_OK) {
    chuchu_error(SERVER, "Bind text failed error: %d", rc);
    sqlite3_finalize(pStmt);
    sqlite3_close(db);
    return 0;
  }

  rc = sqlite3_bind_text(pStmt, 2, u_name, (int)strlen(u_name), SQLITE_STATIC);
  if (rc != SQLITE_OK) {
    chuchu_error(SERVER, "Bind text failed error: %d", rc);
    sqlite3_finalize(pStmt);
    sqlite3_close(db);
    return 0;
  }
  
  rc = sqlite3_bind_blob(pStmt, 3, data, nData, SQLITE_STATIC);
  if (rc != SQLITE_OK) {
    chuchu_error(SERVER, "Bind BLOB failed error: %d", rc);
    sqlite3_finalize(pStmt);
    sqlite3_close(db);
    return 0;
  }
  
  rc = sqlite3_step(pStmt);
  if (rc != SQLITE_DONE) {
    chuchu_error(SERVER, "Insert BLOB failed error: %d", rc);
    sqlite3_finalize(pStmt);
    sqlite3_close(db);
    return 0;
  }
  sqlite3_finalize(pStmt);
  
  const char *getID = "SELECT last_insert_rowid();";
  rc = sqlite3_prepare_v2(db, getID, -1, &pStmt, 0);
  if(rc != SQLITE_OK) {
    chuchu_error(SERVER, "Prepare SQL statement failed error: %d", rc);
    sqlite3_close(db);
    return 0;
  }
  
  rc = sqlite3_step(pStmt);
  if (rc == SQLITE_ROW) {
    lastid = sqlite3_column_int(pStmt, 0);
  }
  
  //Create puzzle struct
  puzzle_t *puz = (puzzle_t *)calloc(1, sizeof(puzzle_t));
  puz->id = (uint32_t)lastid;
  puz->dl = 0;
  strlcpy(puz->p_name, p_name, strlen(p_name)+1);
  strlcpy(puz->u_name, u_name, strlen(u_name)+1);
  //Should not happen
  assert(s->puzz_l[lastid] == NULL);
  s->puzz_l[lastid] = puz;
  
  sqlite3_finalize(pStmt);
  sqlite3_close(db);
  
  return 1;
}


/*
 * Function: read_puzzle_in_chuchu_db
 * --------------------
 *
 * Function that reads puzzle from
 * the DB
 * 
 *  *db_path: full path to the DB
 *  *msg:  ptr to outgoing client msg
 *  id: id of puzzle
 *
 *  returns: size of pkt
 *        
 */
int read_puzzle_in_chuchu_db(server_data_t *s, const char* db_path, char* msg, uint32_t id) {
  sqlite3 *db;
  int rc=0;
  char *p_name = 0;
  char *puzBlob = 0;
  int pnBlob = 0;           
  sqlite3_stmt *pStmt;
  
  if((db = open_chuchu_db(db_path)) == NULL){
    return 0;
  }
  
  const char *zSql = "SELECT PUZZLE_NAME,PUZZLE_FILE FROM PUZZLE_DATA WHERE ID = ?"; 
  
  rc = sqlite3_prepare_v2(db, zSql, -1, &pStmt, 0);
  if(rc != SQLITE_OK) {
    chuchu_error(SERVER, "Prepare SQL statement failed error: %d", rc);
    sqlite3_close(db);
    return 0;
  }

  rc = sqlite3_bind_int(pStmt, 1, (int)id);
  if (rc != SQLITE_OK) {
    sqlite3_finalize(pStmt);
    chuchu_error(SERVER, "Bind int failed error: %d", rc);
    sqlite3_close(db);
    return 0;
  }
  
  rc = sqlite3_step(pStmt);
  
  if(rc == SQLITE_ROW) {
    p_name = (char*)sqlite3_column_text(pStmt, 0);
    pnBlob = sqlite3_column_bytes(pStmt, 1);
    puzBlob = malloc( (long unsigned int)pnBlob * sizeof(char) ); 
    memcpy(puzBlob, sqlite3_column_blob(pStmt, 1), (size_t)pnBlob);
  } else {
    sqlite3_finalize(pStmt);
    chuchu_error(SERVER, "Can't find puzzle");
    sqlite3_close(db); 
    return 0;
  }
  //Add puzzle_name and blob to msg
  strlcpy(msg, p_name, strlen(p_name)+1); 
  memcpy(&msg[0x10], puzBlob, (size_t)pnBlob);
  
  sqlite3_finalize(pStmt);
  sqlite3_close(db);

  free(puzBlob);
  
  return (pnBlob+16);
}

/*
 * Function: update_puzzle_downloaded_to_chuchu_db
 * --------------------
 *
 * Function that updates the amount of times the puzzle
 * has been downloaded
 * 
 *  *db_path: full path to the DB
 *  id: id of the puzzle in the DB
 *
 *  returns: 
 *           1 => OK
 *        -1,0 => FAILED
 *        
 */
int update_puzzle_downloaded_to_chuchu_db(server_data_t *s, const char* db_path, uint32_t id) {
  sqlite3 *db = NULL;
  int rc = 0, i=0;
  uint16_t cn=0;
  sqlite3_stmt *pStmt;
  
  if((db = open_chuchu_db(db_path)) == NULL) {
    return 0;
  }

  //Update puzzle downloaded counter
  for (i=0;i<(s->m_puzz);i++) {
    if(s->puzz_l[i]) {
      if (s->puzz_l[i]->id == id) {
	s->puzz_l[i]->dl++;
	cn = s->puzz_l[i]->dl;
	break;
      }
    }
  }
  
  const char *zSql = "UPDATE PUZZLE_DATA SET DOWNLOADED = ? WHERE ID = ?"; 

  rc = sqlite3_prepare_v2(db, zSql, -1, &pStmt, 0);
  if( rc != SQLITE_OK ){
    chuchu_error(SERVER, "Prepare SQL error: %d", rc);
    sqlite3_close(db);
    return 0;
  }

  rc = sqlite3_bind_int(pStmt, 1, (int)cn);
  if (rc != SQLITE_OK) {
    sqlite3_finalize(pStmt);
    chuchu_error(SERVER, "Bind int failed error: %d", rc);
    sqlite3_close(db);
    return 0;
  }
  
  rc = sqlite3_bind_int(pStmt, 2, (int)id);
  if (rc != SQLITE_OK) {
    sqlite3_finalize(pStmt);
    chuchu_error(SERVER, "Bind int failed error: %d", rc);
    sqlite3_close(db);
    return 0;
  }

  rc = sqlite3_step(pStmt);
  if (rc != SQLITE_DONE) {
    chuchu_error(SERVER, "Insert failed error: %d", rc);
    sqlite3_finalize(pStmt);
    sqlite3_close(db);
    return 0;
  }
  
  sqlite3_finalize(pStmt);
  sqlite3_close(db);

  chuchu_info(SERVER, "Puzzle id: [%d] has been downloaded for the %d time", id, cn);
  
  return 1;
}

/*
 * Function: update_player_ranking_to_chuchu_db
 * --------------------
 *
 * Function that updates the players ranking
 * after disconnecting the game
 * 
 *  *db_path: full path to the DB
 *  *pl:  ptr to player struct
 *
 *  returns: 
 *           1 => OK
 *        -1,0 => FAILED
 *        
 */
int update_player_ranking_to_chuchu_db(const char* db_path, player_t* pl) {
  sqlite3 *db = NULL;
  int rc = 0;
  sqlite3_stmt *pStmt;

  const char* u_name = pl->username;
  const char* dc_id = pl->dreamcast_id;
#ifdef DISABLE_AUTH
  if (!is_player_in_chuchu_db(db_path, u_name, 1))
	  write_player_to_chuchu_db(db_path, dc_id, u_name, "");
#endif
  /*
   Important, chuchu sends a updated stat packet after each game, so
   we need to keep the orig. values and new apart, add here and store
   but don't change the orig. read values from the DB and only update
   after a disconnect
  */
  uint32_t won_rnds = pl->won_rnds + pl->db_won_rnds;
  uint32_t lost_rnds = pl->lost_rnds + pl->db_lost_rnds;
  uint32_t total_rnds = pl->total_rnds + pl->db_total_rnds;
  
  if((db = open_chuchu_db(db_path)) == NULL) {
    return 0;
  }

  const char* zSql = "UPDATE PLAYER_DATA SET WON_RNDS = ?, LOST_RNDS = ?, TOTAL_RNDS = ? WHERE DC_ID = hex(?) AND USERNAME = trim(?);";

  rc = sqlite3_prepare_v2(db, zSql, -1, &pStmt, 0);
  if( rc != SQLITE_OK ){
    chuchu_error(SERVER, "Prepare SQL error: %d", rc);
    sqlite3_close(db);
    return -1;
  }

  rc = sqlite3_bind_int(pStmt, 1, (int)won_rnds);
  if (rc != SQLITE_OK) {
    sqlite3_finalize(pStmt);
    chuchu_error(SERVER, "Bind int failed error: %d", rc);
    sqlite3_close(db);
    return 0;
  }
  
  rc = sqlite3_bind_int(pStmt, 2, (int)lost_rnds);
  if (rc != SQLITE_OK) {
    sqlite3_finalize(pStmt);
    chuchu_error(SERVER, "Bind int failed error: %d", rc);
    sqlite3_close(db);
    return 0;
  }
  
  rc = sqlite3_bind_int(pStmt, 3, (int)total_rnds);
  if (rc != SQLITE_OK) {
    sqlite3_finalize(pStmt);
    chuchu_error(SERVER, "Bind int failed error: %d", rc);
    sqlite3_close(db);
    return 0;
  }

  rc = sqlite3_bind_text(pStmt, 4, dc_id, 6, SQLITE_STATIC);
  if (rc != SQLITE_OK) {
    sqlite3_finalize(pStmt);
    chuchu_error(SERVER, "Bind text failed error: %d", rc);
    sqlite3_close(db);
    return 0;
  }
  
  rc = sqlite3_bind_text(pStmt, 5, u_name, (int)strlen(u_name), SQLITE_STATIC);
  if (rc != SQLITE_OK) {
    sqlite3_finalize(pStmt);
    chuchu_error(SERVER, "Bind text failed error: %d", rc);
    sqlite3_close(db);
    return 0;
  }

  rc = sqlite3_step(pStmt);
  if (rc != SQLITE_DONE) {
    chuchu_error(SERVER, "Insert failed error: %d", rc);
    sqlite3_finalize(pStmt);
    sqlite3_close(db);
    return 0;
  }
  
  sqlite3_finalize(pStmt);
  chuchu_info(SERVER,"[%s] Ranking before [%d][%d][%d]",pl->username,pl->db_won_rnds,pl->db_lost_rnds,pl->db_total_rnds);
  chuchu_info(SERVER,"[%s] Ranking after [%d][%d][%d]",pl->username,won_rnds,lost_rnds,total_rnds);
  sqlite3_close(db);
  
  return 1;
}

/*
 * Function: read_ranking_from_chuchu_db
 * --------------------
 *
 * Function that reads the players ranking
 * from the DB
 * 
 *  *db_path: full path to the DB
 *  *pl:  ptr to player struct
 *
 *  returns: 
 *           1 => OK
 *        -1,0 => FAILED
 *        
 */
int read_ranking_from_chuchu_db(const char* db_path, player_t* pl) {
  sqlite3 *db;
  int rc;
  sqlite3_stmt *pStmt;

  const char* dc_id = pl->dreamcast_id;
  const char* u_name = pl->username;
  
  if((db = open_chuchu_db(db_path)) == NULL){
    return -1;
  }
  
  const char *zSql = "SELECT WON_RNDS,LOST_RNDS,TOTAL_RNDS FROM PLAYER_DATA WHERE DC_ID = hex(?) AND USERNAME = trim(?)"; 
  
  rc = sqlite3_prepare_v2(db, zSql, -1, &pStmt, 0);
  if(rc != SQLITE_OK) {
    chuchu_error(SERVER, "Prepare SQL statement failed error: %d", rc);
    sqlite3_close(db);
    return -1;
  }

  rc = sqlite3_bind_text(pStmt, 1, dc_id, 6, SQLITE_STATIC);
  if (rc != SQLITE_OK) {
    sqlite3_finalize(pStmt);
    chuchu_error(SERVER, "Bind text failed error: %d", rc);
    sqlite3_close(db);
    return -1;
  }

  rc = sqlite3_bind_text(pStmt, 2, u_name, (int)strlen(u_name), SQLITE_STATIC);
  if (rc != SQLITE_OK) {
    sqlite3_finalize(pStmt);
    chuchu_error(SERVER, "Bind text failed error: %d", rc);
    sqlite3_close(db);
    return -1;
  }
  
  rc = sqlite3_step(pStmt);
  
  if(rc == SQLITE_ROW) {
    pl->db_won_rnds = (uint32_t)sqlite3_column_int(pStmt, 0);
    pl->db_lost_rnds = (uint32_t)sqlite3_column_int(pStmt, 1);
    pl->db_total_rnds = (uint32_t)sqlite3_column_int(pStmt, 2);
  } else {
#ifdef DISABLE_AUTH
    chuchu_info(SERVER, "Can't find user, returning 0");
    pl->db_won_rnds = 0;
    pl->db_lost_rnds = 0;
    pl->db_total_rnds = 0;
#else
    sqlite3_finalize(pStmt);
    chuchu_error(SERVER, "Can't find user, getting %d", rc);
    sqlite3_close(db); 
    return -1;
#endif
  }
  sqlite3_finalize(pStmt);
  sqlite3_close(db);

  return 1;
}

/*
 * Function: read_top_ranking_from_chuchu_db
 * --------------------
 *
 * Function that reads the top 10 ranking
 * from the DB
 * 
 *  *db_path: full path to the DB
 *  *pl:  ptr to player struct
 *
 *  returns: size of packet
 *
 */
int read_top_ranking_from_chuchu_db(char* msg, const char* db_path) {
  sqlite3 *db;
  int rc = 0;
  sqlite3_stmt *pStmt;
  char u_name[MAX_UNAME_LEN];
  int pkt_size = 0;
  uint32_t won_rnds=0,lost_rnds=0,total_rnds=0;
  memset(u_name,0,sizeof(u_name));
  
  if((db = open_chuchu_db(db_path)) == NULL){
    return 0;
  }
  
  const char *zSql = "SELECT USERNAME,WON_RNDS,LOST_RNDS,TOTAL_RNDS FROM PLAYER_DATA ORDER BY WON_RNDS DESC LIMIT 10;"; 
  
  rc = sqlite3_prepare_v2(db, zSql, -1, &pStmt, 0);
  if(rc != SQLITE_OK) {
    chuchu_error(SERVER, "Prepare SQL statement failed error: %d", rc);
    sqlite3_close(db);
    return 0;
  }

  pkt_size = sprintf(&msg[0], "================ TOP 10 RANKING ================\n%*s\n%*s%*s%*s%*s\n",33,"ROUNDS",-16,"Username",8,"Won",8,"Lost",8,"Total");
  
  while (1) {
    rc = sqlite3_step(pStmt);
    if(rc == SQLITE_ROW) {
      strlcpy(u_name, (char*)sqlite3_column_text(pStmt,0), (size_t)sqlite3_column_bytes(pStmt, 0)+1);
      won_rnds = (uint32_t)sqlite3_column_int(pStmt, 1);
      lost_rnds = (uint32_t)sqlite3_column_int(pStmt, 2);
      total_rnds = (uint32_t)sqlite3_column_int(pStmt, 3);
      pkt_size += sprintf(&msg[pkt_size], "%*s%*d%*d%*d\n",-16,u_name,8,won_rnds,8,lost_rnds,8,total_rnds);
    } else if (rc == SQLITE_DONE) {
      break;
    }
    else {
      sqlite3_finalize(pStmt);
      chuchu_error(SERVER, "Can't get top ranking");
      sqlite3_close(db); 
      return 0;
    }
  }
  
  sqlite3_finalize(pStmt);
  sqlite3_close(db);

  return pkt_size;
}
