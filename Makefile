CC = gcc
#CFLAGS = -Wall -Wconversion -g
CFLAGS = -Wall -O3
LFLAGS = -lpthread -lsqlite3
TARGET = chuchu_login_server chuchu_lobby_server
LOGIN_SRC = chuchu_login_server.c chuchu_common.c chuchu_sql.c chuchu_msg.c
LOBBY_SRC = chuchu_lobby_server.c chuchu_common.c chuchu_sql.c chuchu_msg.c
COMMON_SRC = chuchu_common.c chuchu_sql.c chuchu_msg.c chuchu_common.h chuchu_sql.h chuchu_msg.h
USER = dcnet
INSTALL_DIR = /usr/local/chuchu

all: $(TARGET)

chuchu_login_server: chuchu_login_server.c $(COMMON_SRC)
	$(CC) $(CFLAGS) $(LOGIN_SRC) -o $@ $(LFLAGS)
chuchu_lobby_server: chuchu_lobby_server.c $(COMMON_SRC)
	$(CC) $(CFLAGS) $(LOBBY_SRC) -o $@ $(LFLAGS)
clean:
	rm $(TARGET)
	rm -f *~

install:
	install -o $(USER) -g $(USER) -d $(INSTALL_DIR)
	install -o $(USER) -g $(USER) chuchu_login_server chuchu_lobby_server $(INSTALL_DIR)


installservice:
	cp chuchu_login@.service chuchu_lobby@.service /lib/systemd/system/
	systemctl daemon-reload
	systemctl enable chuchu_login@chuchu.service
	systemctl enable chuchu_login@deedee.service
	systemctl enable chuchu_lobby@chuchu.service
	systemctl enable chuchu_lobby@deedee.service

createdb:
	install -o $(USER) -g $(USER) -d $(INSTALL_DIR)/db
	sqlite3 $(INSTALL_DIR)/db/chuchu.db < createdb.sql
	sqlite3 $(INSTALL_DIR)/db/deedee.db < createdb.sql
	chown $(USER):$(USER) $(INSTALL_DIR)/db/chuchu.db
	chown $(USER):$(USER) $(INSTALL_DIR)/db/deedee.db
