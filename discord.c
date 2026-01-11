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
#include <dcserver/discord.h>
#include <stdlib.h>
#include <time.h>

void discordRoomJoined(server_data_t *server, const char *player, const char *room)
{
	// Not thread safe but the worst that can happen is 2 notifications at the same time
	static time_t last_notif;
	time_t now = time(NULL);
	if (last_notif != 0 && now - last_notif < 5 * 60)
		// No more than one notification every 5 min
		return;
	last_notif = now;
	char *playerName = discordEscape(player);
	char *roomName = discordEscape(room);
	char content[128];
	sprintf(content, "Player **%s** joined room **%s**", playerName, roomName);
	free(playerName);
	free(roomName);

	char list[1024];
	list[0] = '\0';
	for(int i = 0; i < server->m_cli; i++)
	{
		if (server->p_l[i] && server->p_l[i]->authorized == 1)
		{
			char *name = discordEscape(server->p_l[i]->username);
			strcat(list, name);
			strcat(list, "\n");
			free(name);
		}
	}
	discordNotif(server->deedee_server ? "deedee" : "chuchu", content, "Connected Players", list);
}

void discordGameStart(game_room_t *gameRoom)
{
	char title[128];
	sprintf(title, "%s: Game Start", gameRoom->g_name);
	char text[1024];
	text[0] = '\0';
	for (int i = 0; i < gameRoom->m_pl_slots; i++)
	{
		if (gameRoom->player_slots[i])
		{
			char *name = discordEscape(gameRoom->player_slots[i]->username);
			strcat(text, name);
			strcat(text, "\n");
			free(name);
		}
	}

	// Get the server from a player slot
	for (int i = 0; i < gameRoom->m_pl_slots; i++)
	{
		if (gameRoom->player_slots[i]) {
			discordNotif(((server_data_t *)gameRoom->player_slots[i]->data)->deedee_server ? "deedee" : "chuchu", "", title, text);
			break;
		}
	}
}
