/*
 *
 * Copyright 2025 Flyinghead
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
 */
#include "chuchu_common.h"
#include <stdlib.h>
#include <curl/curl.h>
#include <pthread.h>
#include <time.h>

struct Notif
{
	char *content;
	char *embedTitle;
	char *embedText;
};
typedef struct Notif Notif;

static const char *discordWebhook;
static char ddplanet;

static int writeJsonString(char *json, const char *s)
{
	if (s == NULL)
		return sprintf(json, "null");

	char *j = json;
	*j++ = '"';
	for (; *s != '\0'; s++) {
		switch (*s)
		{
		case '"':
			*j++ = '\\';
			*j++ = '"';
			break;
		case '\n':
			*j++ = '\\';
			*j++ = 'n';
			break;
		default:
			*j++ = *s;
		}
	}
	*j++ = '"';
	*j++ = '\0';

	return (int)(j - json - 1);
}

static void freeNotif(Notif *notif)
{
	free(notif->content);
	free(notif->embedTitle);
	free(notif->embedText);
	free(notif);
}

static void *postWebhookThread(void *arg)
{
	Notif *notif = (Notif *)arg;
	CURL *curl = curl_easy_init();
	if (curl == NULL) {
		chuchu_error(LOBBY_SERVER, "Can't create curl handle");
		freeNotif(notif);
		return NULL;
	}
	CURLcode res;
	curl_easy_setopt(curl, CURLOPT_URL, discordWebhook);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "DCNet-DiscordWebhook");
	struct curl_slist *headers = curl_slist_append(NULL, "Content-Type: application/json");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

	char msg[1024];
	int n;
	n = sprintf(msg, "{ \"content\": ");
	n += writeJsonString(msg + n, notif->content);
	msg[n++] = ',';
	msg[n++] = ' ';

	n += sprintf(msg + n, "\"embeds\": [ "
			"{ \"author\": { \"name\": \"%s\", "
				"\"icon_url\": \"%s\" }, "
			  "\"title\": ",
			  ddplanet ? "Dee Dee Planet" : "ChuChu Rocket!",
			  ddplanet ? "https://dcnet.flyca.st/gamepic/ddplanet.jpg"
					  : "https://dcnet.flyca.st/gamepic/chuchu.jpg");
	n += writeJsonString(msg + n, notif->embedTitle);
	n += sprintf(msg + n, ", \"description\": ");
	n += writeJsonString(msg + n, notif->embedText);
	n += sprintf(msg + n, ", \"color\": 9118205 } ] }");
	//printf("%s\n", msg);

	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, msg);

	res = curl_easy_perform(curl);
	if (res != CURLE_OK) {
		chuchu_error(LOBBY_SERVER, "curl error: %d", res);
	}
	else
	{
		long code;
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
		if (code < 200 || code >= 300)
			chuchu_error(LOBBY_SERVER, "Discord error: %d", code);
	}
	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);
	freeNotif(notif);
	return NULL;
}

static void postWebhook(server_data_t *server, Notif *notif)
{
	discordWebhook = server->discord_webhook;
	ddplanet = server->deedee_server;
	if (discordWebhook[0] == '\0')
		return;
	pthread_attr_t threadAttr;
	if (pthread_attr_init(&threadAttr)) {
		chuchu_error(LOBBY_SERVER, "pthread_attr_init() error");
		freeNotif(notif);
		return;
	}
	if (pthread_attr_setdetachstate(&threadAttr, PTHREAD_CREATE_DETACHED)) {
		chuchu_error(LOBBY_SERVER, "pthread_attr_setdetachstate() error");
		freeNotif(notif);
		return;
	}
    pthread_t thread;
    if (pthread_create(&thread, &threadAttr, postWebhookThread, notif)) {
    	chuchu_error(LOBBY_SERVER, "pthread_create() error");
		freeNotif(notif);
    	return;
    }
}

void discordRoomJoined(server_data_t *server, const char *player, const char *room)
{
	// Not thread safe but the worst that can happen is 2 notifications at the same time
	static time_t last_notif;
	time_t now = time(NULL);
	if (last_notif != 0 && now - last_notif < 5 * 60)
		// No more than one notification every 5 min
		return;
	last_notif = now;
	Notif *notif = (Notif *)calloc(1, sizeof(Notif));
	char content[128];
	sprintf(content, "Player **%s** joined room **%s**", player, room);
	notif->content = strdup(content);
	notif->embedTitle = strdup("Connected Players");

	char list[1024];
	list[0] = '\0';
	for(int i = 0; i < server->m_cli; i++)
	{
		if (server->p_l[i] && server->p_l[i]->authorized == 1) {
			strcat(list, server->p_l[i]->username);
			strcat(list, "\n");
		}
	}
	notif->embedText = strdup(list);

	postWebhook(server, notif);
}

void discordGameStart(game_room_t *gameRoom)
{
	Notif *notif = (Notif *)calloc(1, sizeof(Notif));
	notif->content = strdup("");
	char title[128];
	sprintf(title, "%s: Game Start", gameRoom->g_name);
	notif->embedTitle = strdup(title);
	char text[1024];
	text[0] = '\0';
	for (int i = 0; i < gameRoom->m_pl_slots; i++)
	{
		if (gameRoom->player_slots[i]) {
			strcat(text, gameRoom->player_slots[i]->username);
			strcat(text, "\n");
		}
	}
	notif->embedText = strdup(text);

	// Get the server from a player slot
	for (int i = 0; i < gameRoom->m_pl_slots; i++)
	{
		if (gameRoom->player_slots[i]) {
			postWebhook((server_data_t *)gameRoom->player_slots[i]->data, notif);
			break;
		}
	}
}
