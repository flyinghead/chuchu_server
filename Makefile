CC = gcc
CFLAGS = -Wall -Wconversion
LFLAGS = -lpthread -lsqlite3
TARGET = chuchu_login_server chuchu_lobby_server
LOGIN_SRC = chuchu_login_server.c chuchu_common.c chuchu_sql.c chuchu_msg.c
LOBBY_SRC = chuchu_lobby_server.c chuchu_common.c chuchu_sql.c chuchu_msg.c
COMMON_SRC = chuchu_common.c chuchu_sql.c chuchu_msg.c chuchu_common.h chuchu_sql.h chuchu_msg.h

all: $(TARGET)

chuchu_login_server: chuchu_login_server.c $(COMMON_SRC)
	$(CC) $(CFLAGS) $(LOGIN_SRC) -o $@ $(LFLAGS)
chuchu_lobby_server: chuchu_lobby_server.c $(COMMON_SRC)
	$(CC) $(CFLAGS) $(LOBBY_SRC) -o $@ $(LFLAGS)
clean:
	rm $(TARGET)
	rm -f *~
