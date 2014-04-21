/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2000-2006 Tim Angus

This file is part of Tremulous.

Tremulous is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Tremulous is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Tremulous; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "../qcommon/cJSON.h"

/*
=======================================================================

  SESSION DATA

Session data is the only data that stays persistant across level loads
and tournament restarts.
=======================================================================
*/

/*
================
G_WriteClientSessionData

Called on game shutdown
================
*/
void G_WriteClientSessionData( gclient_t *client )
{
  const char  *s;
  const char  *var;

  s = va( "%i %i %i %i %i %i %i %i %s",
    client->sess.sessionTeam,
    client->sess.restartTeam,
    client->sess.spectatorTime,
    client->sess.spectatorState,
    client->sess.spectatorClient,
    client->sess.wins,
    client->sess.losses,
    client->sess.teamLeader,
    BG_ClientListString( &client->sess.ignoreList )
    );

  var = va( "session%i", client - level.clients );

  trap_Cvar_Set( var, s );
}

/*
================
G_ReadSessionData

Called on a reconnect
================
*/
void G_ReadSessionData( gclient_t *client )
{
  char  s[ MAX_STRING_CHARS ];
  const char  *var;

  // bk001205 - format
  int teamLeader;
  int spectatorState;
  int sessionTeam;
  int restartTeam;

  var = va( "session%i", client - level.clients );
  trap_Cvar_VariableStringBuffer( var, s, sizeof(s) );

  // FIXME: should be using BG_ClientListParse() for ignoreList, but
  //        bg_lib.c's sscanf() currently lacks %s
  sscanf( s, "%i %i %i %i %i %i %i %i %x%x",
    &sessionTeam,
    &restartTeam,
    &client->sess.spectatorTime,
    &spectatorState,
    &client->sess.spectatorClient,
    &client->sess.wins,
    &client->sess.losses,
    &teamLeader,
    &client->sess.ignoreList.hi,
    &client->sess.ignoreList.lo
    );
  // bk001205 - format issues
  client->sess.sessionTeam = (team_t)sessionTeam;
  client->sess.restartTeam = (pTeam_t)restartTeam;
  client->sess.spectatorState = (spectatorState_t)spectatorState;
  client->sess.teamLeader = (qboolean)teamLeader;
}


/*
================
G_InitSessionData

Called on a first-time connect
================
*/
void G_InitSessionData( gclient_t *client, char *userinfo )
{
  clientSession_t  *sess;
  const char      *value;

  sess = &client->sess;

  // initial team determination
  value = Info_ValueForKey( userinfo, "team" );
  if( value[ 0 ] == 's' )
  {
    // a willing spectator, not a waiting-in-line
    sess->sessionTeam = TEAM_SPECTATOR;
  }
  else
  {
    if( g_maxGameClients.integer > 0 &&
      level.numNonSpectatorClients >= g_maxGameClients.integer )
      sess->sessionTeam = TEAM_SPECTATOR;
    else
      sess->sessionTeam = TEAM_FREE;
  }

  sess->restartTeam = PTE_NONE;
  sess->spectatorState = SPECTATOR_FREE;
  sess->spectatorTime = level.time;
  sess->spectatorClient = -1;
  memset( &sess->ignoreList, 0, sizeof( sess->ignoreList ) );

  G_WriteClientSessionData( client );
}


/*
==================
G_WriteSessionData

==================
*/
void G_WriteSessionData( void )
{
  int    i,j;
  cJSON *root;
  char *badge_out;

  //TA: ?
  trap_Cvar_Set( "session", va( "%i", 0 ) );
  trap_SendServerCommand( -1, "print \"^2Syncing with database\n\"" );
  for( i = 0 ; i < level.maxclients ; i++ )
  {
    if( level.clients[ i ].pers.connected == CON_CONNECTED )
    {
      G_WriteClientSessionData( &level.clients[ i ] );
      //Update mysql stuff here to.
      if(level.clients[ i ].pers.mysqlid > 1)
			{
				level.clients[ i ].pers.timeplayed += (level.time - level.clients[ i ].pers.enterTime) / 60000; //Minutes played
				level.clients[ i ].pers.structsbuilt += level.clients[ i ].pers.statscounters.structsbuilt;
				level.clients[ i ].pers.structskilled += level.clients[ i ].pers.statscounters.structskilled;
				if(level.clients[ i ].pers.teamSelection == PTE_HUMANS)
				{
					level.clients[ i ].pers.credits+=level.clients[ i ].ps.persistant[ PERS_CREDIT ];
				}
				if(level.clients[ i ].pers.teamSelection == PTE_ALIENS)
				{
					level.clients[ i ].pers.evos+=level.clients[ i ].ps.persistant[ PERS_CREDIT ];
				}
				
				/* Badged related stuff */
				if(level.clients[ i ].pers.teamSelection == level.lastWin)
				{
					level.clients[ i ].pers.gameswin += 1;
				}
				/************************IF MAZE for BADGES************************************/
				//1  	Legendary  	Win over 100 Games
				if(level.clients[ i ].pers.badges[ 1 ] != 1 && level.clients[ i ].pers.gameswin >= 100)
				{
					level.clients[ i ].pers.badgeupdate[1] = 1;
					level.clients[ i ].pers.badges[1] = 1;
				}
				//3  	Fanatic  	Played over 200 Hours
				if(level.clients[ i ].pers.badges[ 3 ] != 1 && (level.clients[ i ].pers.timeplayed/60) >= 200)
				{
					level.clients[ i ].pers.badgeupdate[3] = 1;
					level.clients[ i ].pers.badges[3] = 1;
				}
				//4  	Skilled  	Dealed over 100 headshots
				if(level.clients[ i ].pers.badges[ 4 ] != 1 && level.clients[ i ].pers.statscounters.headshots >= 100)
				{
					level.clients[ i ].pers.badgeupdate[4] = 1;
					level.clients[ i ].pers.badges[4] = 1;
				}
				//10  	Legendary Builder  	Have built over 1000 structures
				if(level.clients[ i ].pers.badges[ 10 ] != 1 
					&& level.clients[ i ].pers.structsbuilt >= 1000)
				{
					level.clients[ i ].pers.badgeupdate[10] = 1;
					level.clients[ i ].pers.badges[10] = 1;
				}
				//12  	Legendary Destructor  	Have killed over 1000 structures
				if(level.clients[ i ].pers.badges[ 12 ] != 1 
					&& level.clients[ i ].pers.structskilled >= 1000)
				{
					level.clients[ i ].pers.badgeupdate[12] = 1;
					level.clients[ i ].pers.badges[12] = 1;
				}
				//13  	Strategist Build  	200 Buildings in one game
				if(level.clients[ i ].pers.badges[ 13 ] != 1 && level.clients[ i ].pers.statscounters.structsbuilt >= 200)
				{
					level.clients[ i ].pers.badgeupdate[13] = 1;
					level.clients[ i ].pers.badges[13] = 1;
				}
				//16  	Alien Guillotine  	Dealed 50 headshots in a game
				if(level.clients[ i ].pers.badges[ 16 ] != 1 && level.clients[ i ].pers.statscounters.headshots >= 50)
				{
					level.clients[ i ].pers.badgeupdate[16] = 1;
					level.clients[ i ].pers.badges[16] = 1;
				}
				//25  	Structure Killer  	Killed over 50 Structures in one game
				if(level.clients[ i ].pers.badges[ 25 ] != 1 
					&& level.clients[ i ].pers.statscounters.structskilled >= 50)
				{
					level.clients[ i ].pers.badgeupdate[25] = 1;
					level.clients[ i ].pers.badges[25] = 1;
				}
				//26  	Entusiast  	Played 50 Hours
				if(level.clients[ i ].pers.badges[ 26 ] != 1 && (level.clients[ i ].pers.timeplayed/60) >= 50)
				{
					level.clients[ i ].pers.badgeupdate[26] = 1;
					level.clients[ i ].pers.badges[26] = 1;
				}
				//38  	Guardian Angel  	Assist over 20 times in 1 game
				if(level.clients[ i ].pers.badges[ 38 ] != 1 && level.clients[ i ].pers.statscounters.assists > 20)
				{
					level.clients[ i ].pers.badgeupdate[38] = 1;
					level.clients[ i ].pers.badges[38] = 1;
				}
				
				//39  	Saver  	Have over 100,000c/666e on the bank
				if(level.clients[ i ].pers.badges[ 39 ] != 1 
				&& (level.clients[ i ].pers.evos > 666 || level.clients[ i ].pers.credits > 100000) )
				{
					level.clients[ i ].pers.badgeupdate[39] = 1;
					level.clients[ i ].pers.badges[39] = 1;
				}
				
				//40  	Builder  	Have built 50 structures
				if(level.clients[ i ].pers.badges[ 40 ] != 1 && level.clients[ i ].pers.statscounters.structsbuilt >= 50)
				{
					level.clients[ i ].pers.badgeupdate[40] = 1;
					level.clients[ i ].pers.badges[40] = 1;
				}
				//41  	Learner  	Played at least 5 Hours  	
				if(level.clients[ i ].pers.badges[ 41 ] != 1 && (level.clients[ i ].pers.timeplayed/60) >= 5)
				{
					level.clients[ i ].pers.badgeupdate[41] = 1;
					level.clients[ i ].pers.badges[41] = 1;
				}
				

				//badges to JSON
				root = cJSON_CreateIntArray(level.clients[ i ].pers.badgeupdate,50)
                badge_out = cJSON_Print(root);
                cJSON_Delete(root);
				/*************************************************************/
				//Would be better if i runquery just one time instead of one per client.
				if( trap_mysql_runquery( va("UPDATE players SET credits=\"%d\",evos=\"%d\",timeplayed=\"%d\",adminlevel=\"%d\",lasttime=NOW(),gameswin=\"%d\",structsbuilt=\"%d\",structskilled=\"%d\",badges=\"%s\" WHERE id=\"%d\" LIMIT 1", level.clients[ i ].pers.credits, level.clients[ i ].pers.evos, level.clients[ i ].pers.timeplayed, level.clients[ i ].pers.adminlevel, level.clients[ i ].pers.gameswin, level.clients[ i ].pers.structsbuilt, level.clients[ i ].pers.structskilled, badge_out, level.clients[ i ].pers.mysqlid ) ) == qtrue )
				{
					trap_mysql_finishquery();
					//Lets update the badges. //FIX ME: WTF LOL DOUBLE LOOPED UNECESARY
					/*for(j=1;j<49;j++)
					{
						if(level.clients[ i ].pers.badgeupdate[j] == 1)
						{
							if(trap_mysql_runquery( va("INSERT HIGH_PRIORITY INTO badges_player (idplayer,idbadge) VALUES (\"%d\",\"%d\")", level.clients[ i ].pers.mysqlid, j ) ) == qtrue)
							{
								trap_mysql_finishquery();
							}
							else
							{
								trap_mysql_finishquery();
							}
						}
					}*/
				}
				else
				{
					trap_mysql_finishquery();
				}
				free(badge_out);
			}
		}
  }
  trap_SendServerCommand( -1, "print \"^5Data updated\n\"" );
}
void G_WinBadge( gentity_t *ent, int id )
{
	//trap_SendServerCommand( ent-g_entities, "print \"^3You have win a new Badge\ntype !badges for more information\n\"" );
	trap_SendServerCommand( ent-g_entities, "print \"^7You won the badge: \"" );
	switch(id)
	{
		case 1:
			trap_SendServerCommand( ent-g_entities, "print \"^3[Legendary] Win over 100 Games\n\"" );
		break;
		case 2:
			trap_SendServerCommand( ent-g_entities, "print \"^3[The Mad Granger] Kill 1 Human wearing a Battle Suit with the Granger swipe\n\"" );
		break;
		case 3:
			trap_SendServerCommand( ent-g_entities, "print \"^3[Fanatic] Played over 200 Hours3\n\"" );
		break;
		case 4:
			trap_SendServerCommand( ent-g_entities, "print \"^3[Skilled] Dealed over 100 headshots4\n\"" );
		break;
		case 5:
			trap_SendServerCommand( ent-g_entities, "print \"^3[Blast it!] Killed over 15 aliens by a blaster in a game\n\"" );
		break;
		case 6:
			trap_SendServerCommand( ent-g_entities, "print \"^3[Survivalist] Dont die for over 15 minutes\n\"" );
		break;
		case 7:
			trap_SendServerCommand( ent-g_entities, "print \"^3[Ultra Granger] Destroy The Reactor While being a Granger\n\"" );
		break;
		case 8:
			trap_SendServerCommand( ent-g_entities, "print \"^3[Starcraft] Kill one Tyrant While using a Rifle\n\"" );
		break;
		case 9:
			trap_SendServerCommand( ent-g_entities, "print \"^3[I Bite Pretty Hard] kill a human with battlesuit while using a dretch\n\"" );
		break;
		case 10:
			trap_SendServerCommand( ent-g_entities, "print \"^3[Legendary Builder] Has built over 1000 structures\n\"" );
		break;
		case 11:
			trap_SendServerCommand( ent-g_entities, "print \"^3[Spitter] Kill a Human while using the barb of the granger\n\"" );
		break;
		case 12:
			trap_SendServerCommand( ent-g_entities, "print \"^3[Legendary Destructor] Has killed over 1000 structures\n\"" );
		break;
		case 13:
			trap_SendServerCommand( ent-g_entities, "print \"^3[Strategist Build] 200 Buildings in one game\n\"" );
		break;
		case 14:
			trap_SendServerCommand( ent-g_entities, "print \"^3[Gattling Gun] Kill one Tyrant while using the chaingun\n\"" );
		break;
		case 15:
			trap_SendServerCommand( ent-g_entities, "print \"^3[Pain master] Killed 5 aliens in a row with the psaw\n\"" );
		break;
		case 16:
			trap_SendServerCommand( ent-g_entities, "print \"^3[Alien Guillotine] Dealed 50 headshots in a game\n\"" );
		break;
		case 17:
			trap_SendServerCommand( ent-g_entities, "print \"^3[Anti-Sniper] kill 2 adv goon with your mass driver\n\"" );
		break;
		case 18:
			trap_SendServerCommand( ent-g_entities, "print \"^3[Tyrant\'s Might]Charge and kill upto 3 humans in 1 Tyrant life\n\"" );
		break;
		case 19:
			trap_SendServerCommand( ent-g_entities, "print \"^3[Specialist] Kill the reactor by the goon barbs\n\"" );
		break;
		case 20:
			trap_SendServerCommand( ent-g_entities, "print \"^3[Electric Eel] Killed 3 humans with Zap\n\"" );
		break;
		case 21:
			trap_SendServerCommand( ent-g_entities, "print \"^3[Attack Granger] Kill 1 Human while using a granger\n\"" );
		break;
		case 22:
			trap_SendServerCommand( ent-g_entities, "print \"^3[I like my eggs...scrambled] Kill 3 eggs with a saw within 2 minutes\n\"" );
		break;
		case 23:
			trap_SendServerCommand( ent-g_entities, "print \"^3[Flying Dragoon] Pounce and kill over 5 humans while being a dragoon in 1 life\n\"" );
		break;
		case 24:
			trap_SendServerCommand( ent-g_entities, "print \"^3[Hugs and Swipings] kill 2 Humans while using a basilisk on 1 life\n\"" );
		break;
		case 25:
			trap_SendServerCommand( ent-g_entities, "print \"^3[Structure Killer] Killed over 50 Structures in one game\n\"" );
		break;
		case 26:
			trap_SendServerCommand( ent-g_entities, "print \"^3[Entusiast] Played 50 Hours\n\"" );
		break;
		case 27:
			trap_SendServerCommand( ent-g_entities, "print \"^3[David and Goliath] Kill a tyrant the lasgun\n\"" );
		break;
		case 28:
			trap_SendServerCommand( ent-g_entities, "print \"^3RESERVED\n\"" );
		break;
		case 29:
			trap_SendServerCommand( ent-g_entities, "print \"^3[Freezing Field] Icewave over 5 enemies in 1 icewave \n\"" );
		break;
		case 30:
			trap_SendServerCommand( ent-g_entities, "print \"^3RESERVED\n\"" );
		break;
		case 31:
			trap_SendServerCommand( ent-g_entities, "print \"^3[Red right hand-y weapon] Killed a goon in one lucifer cannon shot\n\"" );
		break;
		case 32:
			trap_SendServerCommand( ent-g_entities, "print \"^3[Stealth Killer] killed 10 enemy\'s while on invisible mode\n\"" );
		break;
		case 33:
			trap_SendServerCommand( ent-g_entities, "print \"^3[Divine Gift] Killed 5 Enemy while using God Mode\n\"" );
		break;
		case 34:
			trap_SendServerCommand( ent-g_entities, "print \"^3[Pro Nader] killed 5 aliens with 1 grenade\n\"" );
		break;
		case 35:
			trap_SendServerCommand( ent-g_entities, "print \"^3[Suicide Bomber] killed 3 humans in 1 dretch nade\n\"" );
		break;
		case 36:
			trap_SendServerCommand( ent-g_entities, "print \"^3[Poison Freak] killed 3 Humans by a Poisoned Icewave\n\"" );
		break;
		case 37:
			trap_SendServerCommand( ent-g_entities, "print \"^3[Fire Breathing] Human killed 10 Dretches in 1 Flamethrower without reloading\n\"" );
		break;
		case 38:
			trap_SendServerCommand( ent-g_entities, "print \"^3[Guardian Angel] Assist over 20 times in 1 gameGuardian Angel  	Assist over 20 times in 1 game\n\"" );
		break;
		case 39:
			trap_SendServerCommand( ent-g_entities, "print \"^3[Saver] Has over 100,000c/666e on the bank\n\"" );
		break;
		case 40:
			trap_SendServerCommand( ent-g_entities, "print \"^3[Builder] Has built 50 structures\n\"" );
		break;
		case 41:
			trap_SendServerCommand( ent-g_entities, "print \"^3[Learner] Played at least 5 Hours\n\"" );
		break;
		case 42:
			trap_SendServerCommand( ent-g_entities, "print \"^3[Banker] Used bank for first time\n\"" );
		break;
		case 43:
			trap_SendServerCommand( ent-g_entities, "print \"^3[Newbie] Has won the the newbie bonus for first time\n\"" );
		break;
	}
}
