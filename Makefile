CC = gcc
#CFLAGS = -DDISABLE_AUTH -Wall -Wconversion -g -fsanitize=address
CFLAGS = -DDISABLE_AUTH -Wall -O3
LFLAGS = -lpthread -lsqlite3 -lcurl $(CFLAGS)
TARGET = chuchu_login_server chuchu_lobby_server
LOGIN_SRC = chuchu_login_server.c chuchu_common.c chuchu_sql.c chuchu_msg.c
LOBBY_SRC = chuchu_lobby_server.c chuchu_common.c chuchu_sql.c chuchu_msg.c discord.c
COMMON_SRC = chuchu_common.c chuchu_sql.c chuchu_msg.c chuchu_common.h chuchu_sql.h chuchu_msg.h
USER = dcnet
INSTALL_DIR = /usr/local/chuchu

all: $(TARGET)

chuchu_login_server: $(LOGIN_SRC) $(COMMON_SRC)
	$(CC) $(CFLAGS) $(LOGIN_SRC) -o $@ $(LFLAGS)
chuchu_lobby_server: $(LOBBY_SRC) $(COMMON_SRC)
	$(CC) $(CFLAGS) $(LOBBY_SRC) -o $@ $(LFLAGS)
clean:
	rm -f $(TARGET) *.o
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
