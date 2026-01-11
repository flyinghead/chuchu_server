#
# dependencies: libsqlite3-dev libdcserver
#
#CFLAGS = -Wall -Wconversion -g -fsanitize=address
CFLAGS = -Wall -O3
LDFLAGS = -lpthread -lsqlite3
TARGET = chuchu_login_server chuchu_lobby_server
HEADERS = chuchu_common.h chuchu_sql.h chuchu_msg.h
LOGIN_OBJ = chuchu_login_server.o
LOBBY_OBJ = chuchu_lobby_server.o
COMMON_OBJ = chuchu_common.o chuchu_sql.o chuchu_msg.o
USER = chuchu
INSTALL_DIR = /usr/local/chuchu
DCNET = 1

ifeq ($(DCNET),1)
  LDFLAGS := $(LDFLAGS) -ldcserver -Wl,-rpath,/usr/local/lib
  CFLAGS := $(CFLAGS) -DDISABLE_AUTH -DDCNET
  USER := dcnet
  LOBBY_OBJ := $(LOBBY_OBJ) discord.o
endif

all: $(TARGET)

%.o: %.c $(HEADERS) Makefile
	$(CC) $(CFLAGS) -c -o $@ $<

chuchu_login_server: $(LOGIN_OBJ) $(COMMON_OBJ)
	$(CC) $(CFLAGS) $(LOGIN_OBJ) $(COMMON_OBJ) -o $@ $(LDFLAGS)
chuchu_lobby_server: $(LOBBY_OBJ) $(COMMON_OBJ)
	$(CC) $(CFLAGS) $(LOBBY_OBJ) $(COMMON_OBJ) -o $@ $(LDFLAGS)
clean:
	rm -f $(TARGET) *.o *~

install:
	install -o $(USER) -g $(USER) -d $(INSTALL_DIR)
	install chuchu_login_server chuchu_lobby_server $(INSTALL_DIR)


installservice:
	cp chuchu_login@.service chuchu_lobby@.service /usr/lib/systemd/system/
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
