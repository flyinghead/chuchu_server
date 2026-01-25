#
# dependencies: libsqlite3-dev libdcserver
#
prefix = /usr/local
exec_prefix = $(prefix)
sbindir = $(exec_prefix)/sbin
sysconfdir = $(prefix)/etc
datarootdir = $(prefix)/share
localstatedir = /var/local
user = chuchu

#CFLAGS = -Wall -Wconversion -g -fsanitize=address
CFLAGS = -Wall -O3 -g
LDFLAGS = -lpthread -lsqlite3
TARGET = chuchu_login_server chuchu_lobby_server
HEADERS = chuchu_common.h chuchu_sql.h chuchu_msg.h
LOGIN_OBJ = chuchu_login_server.o
LOBBY_OBJ = chuchu_lobby_server.o
COMMON_OBJ = chuchu_common.o chuchu_sql.o chuchu_msg.o
DCNET = 1

ifeq ($(DCNET),1)
  LDFLAGS += -ldcserver -Wl,-rpath,/usr/local/lib
  CFLAGS += -DDISABLE_AUTH -DDCNET
  user := dcnet
  LOBBY_OBJ += discord.o
endif

all: $(TARGET)

%.o: %.c $(HEADERS) Makefile
	$(CC) $(CFLAGS) -c -o $@ $<

chuchu_login_server: $(LOGIN_OBJ) $(COMMON_OBJ)
	$(CC) $(CFLAGS) $(LOGIN_OBJ) $(COMMON_OBJ) -o $@ $(LDFLAGS)
chuchu_lobby_server: $(LOBBY_OBJ) $(COMMON_OBJ)
	$(CC) $(CFLAGS) $(LOBBY_OBJ) $(COMMON_OBJ) -o $@ $(LDFLAGS)
clean:
	rm -f $(TARGET) *.o *~ *.tmp chuchu_login@.service chuchu_lobby@.service

install:
	mkdir -p $(DESTDIR)$(sbindir)
	install chuchu_login_server chuchu_lobby_server $(DESTDIR)$(sbindir)
	mkdir -p $(DESTDIR)$(sysconfdir)
	sed -e "s:DBDIR:$(localstatedir)/lib/chuchu:g" -e "s:DATADIR:$(datarootdir)/chuchu:g" < chuchu.cfg > chuchu.cfg.tmp
	cp -n chuchu.cfg.tmp $(DESTDIR)$(sysconfdir)/chuchu.cfg
	sed -e "s:DBDIR:$(localstatedir)/lib/chuchu:g" -e "s:DATADIR:$(datarootdir)/chuchu:g" < deedee.cfg > deedee.cfg.tmp
	cp -n deedee.cfg.tmp $(DESTDIR)$(sysconfdir)/deedee.cfg
	mkdir -p $(DESTDIR)$(datarootdir)/chuchu
	cp info/chuchu_info.txt $(DESTDIR)$(datarootdir)/chuchu

%.service: %.service.in
	sed -e "s/INSTALL_USER/$(user)/g" -e "s:SBINDIR:$(sbindir):g" -e "s:SYSCONFDIR:$(sysconfdir):g" -e "s:LOCALSTATEDIR:$(localstatedir):g" < $< > $@

installservice: chuchu_login@.service chuchu_lobby@.service
	cp chuchu_login@.service chuchu_lobby@.service /usr/lib/systemd/system/
	systemctl daemon-reload
	systemctl enable chuchu_login@chuchu.service
	systemctl enable chuchu_login@deedee.service
	systemctl enable chuchu_lobby@chuchu.service
	systemctl enable chuchu_lobby@deedee.service

createdb:
	install -o $(user) -g $(user) -d $(localstatedir)/lib/chuchu
	sqlite3 $(localstatedir)/lib/chuchu/chuchu.db < createdb.sql
	sqlite3 $(localstatedir)/lib/chuchu/deedee.db < createdb.sql
	chown $(user):$(user) $(localstatedir)/lib/chuchu/chuchu.db
	chown $(user):$(user) $(localstatedir)/lib/chuchu/deedee.db
