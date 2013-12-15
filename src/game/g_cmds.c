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

#include "g_local.h"

/*
==================
G_SanitiseName

Remove case and control characters from a player name
==================
*/
void G_SanitiseName( char *in, char *out )
{
  qboolean skip = qtrue;
  int spaces = 0;
  int out_len = 0;

  while( *in && out_len < MAX_NAME_LENGTH - 1 )
  {
    // strip leading white space
    if( *in == ' ' )
    {
      if( skip )
      {
        in++;
        continue;
      }
      spaces++;
    }
    else
    {
      spaces = 0;
      skip = qfalse;
    }
    
    if( *in == 27 || *in == '^' )
    {
      in += 2;    // skip color code
      continue;
    }

    if( *in < 32 )
    {
      in++;
      continue;
    }

    *out++ = tolower( *in++ );
    out_len++;
  }
  out -= spaces; 
  *out = 0;
}

/*
==================
G_ClientNumberFromString

Returns a player number for either a number or name string
Returns -1 if invalid
==================
*/
int G_ClientNumberFromString( gentity_t *to, char *s )
{
  gclient_t *cl;
  int       idnum;
  char      s2[ MAX_STRING_CHARS ];
  char      n2[ MAX_STRING_CHARS ];

  // numeric values are just slot numbers
  if( s[ 0 ] >= '0' && s[ 0 ] <= '9' )
  {
    idnum = atoi( s );

    if( idnum < 0 || idnum >= level.maxclients )
      return -1;

    cl = &level.clients[ idnum ];

    if( cl->pers.connected != CON_CONNECTED )
      return -1;

    return idnum;
  }

  // check for a name match
  G_SanitiseName( s, s2 );

  for( idnum = 0, cl = level.clients; idnum < level.maxclients; idnum++, cl++ )
  {
    if( cl->pers.connected != CON_CONNECTED )
      continue;

    G_SanitiseName( cl->pers.netname, n2 );

    if( !strcmp( n2, s2 ) )
      return idnum;
  }

  return -1;
}


/*
==================
G_MatchOnePlayer

This is a companion function to G_ClientNumbersFromString()

returns qtrue if the int array plist only has one client id, false otherwise
In the case of false, err will be populated with an error message.
==================
*/
qboolean G_MatchOnePlayer( int *plist, char *err, int len )
{
  gclient_t *cl;
  int *p;
  char line[ MAX_NAME_LENGTH + 10 ] = {""};

  err[ 0 ] = '\0';
  if( plist[ 0 ] == -1 )
  {
    Q_strcat( err, len, "no connected player by that name or slot #" );
    return qfalse;
  }
  if( plist[ 1 ] != -1 )
  {
    Q_strcat( err, len, "more than one player name matches. "
            "be more specific or use the slot #:\n" );
    for( p = plist; *p != -1; p++ )
    {
      cl = &level.clients[ *p ];
      if( cl->pers.connected == CON_CONNECTED )
      {
        Com_sprintf( line, sizeof( line ), "%2i - %s^7\n",
          *p, cl->pers.netname );
        if( strlen( err ) + strlen( line ) > len )
          break;
        Q_strcat( err, len, line );
      }
    }
    return qfalse;
  }
  return qtrue;
}

/*
==================
G_ClientNumbersFromString

Sets plist to an array of integers that represent client numbers that have
names that are a partial match for s. List is terminated by a -1.

Returns number of matching clientids.
==================
*/
int G_ClientNumbersFromString( char *s, int *plist )
{
  gclient_t *p;
  int i, found = 0;
  char n2[ MAX_NAME_LENGTH ] = {""}; 
  char s2[ MAX_NAME_LENGTH ] = {""}; 
  qboolean is_slot = qtrue;

  *plist = -1;

  // if a number is provided, it might be a slot #
  for( i = 0; i < (int)strlen( s ); i++ )
  {
    if( s[i] < '0' || s[i] > '9' )
    {
      is_slot = qfalse;
      break;
    }
  }

  if( is_slot ) {
    i = atoi( s );
    if( i >= 0 && i < level.maxclients ) {
      p = &level.clients[ i ];
      if( p->pers.connected == CON_CONNECTED ||
        p->pers.connected == CON_CONNECTING ) 
      {
        *plist++ = i;
        *plist = -1;
        return 1;
      }
    }
    // we must assume that if only a number is provided, it is a clientNum
    return 0;
  }
  
  // now look for name matches
  G_SanitiseName( s, s2 );
  if( strlen( s2 ) < 1 )
    return 0;
  for( i = 0; i < level.maxclients; i++ )
  {
    p = &level.clients[ i ];
    if(p->pers.connected != CON_CONNECTED
      && p->pers.connected != CON_CONNECTING)
    {
      continue;
    }
    G_SanitiseName( p->pers.netname, n2 );
    if( strstr( n2, s2 ) )
    {
      *plist++ = i;
      found++;
    }
  }
  *plist = -1;
  return found;
}

/*
==================
ScoreboardMessage

==================
*/
void ScoreboardMessage( gentity_t *ent )
{
  char      entry[ 1024 ];
  char      string[ 1400 ];
  int       stringlength;
  int       i, j;
  gclient_t *cl;
  int       numSorted;
  weapon_t  weapon = WP_NONE;
  upgrade_t upgrade = UP_NONE;

  // send the latest information on all clients
  string[ 0 ] = 0;
  stringlength = 0;

  numSorted = level.numConnectedClients;

  for( i = 0; i < numSorted; i++ )
  {
    int   ping;

    cl = &level.clients[ level.sortedClients[ i ] ];

    if( cl->pers.connected == CON_CONNECTING )
      ping = -1;
    else
      ping = cl->ps.ping < 999 ? cl->ps.ping : 999;

    if( cl->ps.stats[ STAT_HEALTH ] > 0 )
    {
      weapon = cl->ps.weapon;

      if( BG_InventoryContainsUpgrade( UP_BATTLESUIT, cl->ps.stats ) )
        upgrade = UP_BATTLESUIT;
      else if( BG_InventoryContainsUpgrade( UP_JETPACK, cl->ps.stats ) )
        upgrade = UP_JETPACK;
      else if( BG_InventoryContainsUpgrade( UP_BATTPACK, cl->ps.stats ) )
        upgrade = UP_BATTPACK;
      else if( BG_InventoryContainsUpgrade( UP_HELMET, cl->ps.stats ) )
        upgrade = UP_HELMET;
      else if( BG_InventoryContainsUpgrade( UP_LIGHTARMOUR, cl->ps.stats ) )
        upgrade = UP_LIGHTARMOUR;
      else
        upgrade = UP_NONE;
    }
    else
    {
      weapon = WP_NONE;
      upgrade = UP_NONE;
    }

    Com_sprintf( entry, sizeof( entry ),
      " %d %d %d %d %d %d", level.sortedClients[ i ], cl->ps.persistant[ PERS_SCORE ],
      ping, ( level.time - cl->pers.enterTime ) / 60000, weapon, upgrade );

    j = strlen( entry );

    if( stringlength + j > 1024 )
      break;

    strcpy( string + stringlength, entry );
    stringlength += j;
  }

  trap_SendServerCommand( ent-g_entities, va( "scores %i %i %i%s", i,
    level.alienKills, level.humanKills, string ) );
}


/*
==================
ConcatArgs
==================
*/
char *ConcatArgs( int start )
{
  int         i, c, tlen;
  static char line[ MAX_STRING_CHARS ];
  int         len;
  char        arg[ MAX_STRING_CHARS ];

  len = 0;
  c = trap_Argc( );

  for( i = start; i < c; i++ )
  {
    trap_Argv( i, arg, sizeof( arg ) );
    tlen = strlen( arg );

    if( len + tlen >= MAX_STRING_CHARS - 1 )
      break;

    memcpy( line + len, arg, tlen );
    len += tlen;

    if( i != c - 1 )
    {
      line[ len ] = ' ';
      len++;
    }
  }

  line[ len ] = 0;

  return line;
}

/*
==================
G_Flood_Limited

Determine whether a user is flood limited, and adjust their flood demerits
==================
*/

qboolean G_Flood_Limited( gentity_t *ent )
{
  int millisSinceLastCommand;
  int maximumDemerits;

  // This shouldn't be called if g_floodMinTime isn't set, but handle it anyway.
  if( !g_floodMinTime.integer )
    return qfalse;
  
  // Do not limit admins with no censor/flood flag
  if( G_admin_permission( ent, ADMF_NOCENSORFLOOD ) )
   return qfalse;
  
  millisSinceLastCommand = level.time - ent->client->pers.lastFloodTime;
  if( millisSinceLastCommand < g_floodMinTime.integer )
    ent->client->pers.floodDemerits += ( g_floodMinTime.integer - millisSinceLastCommand );
  else
  {
    ent->client->pers.floodDemerits -= ( millisSinceLastCommand - g_floodMinTime.integer );
    if( ent->client->pers.floodDemerits < 0 )
      ent->client->pers.floodDemerits = 0;
  }

  ent->client->pers.lastFloodTime = level.time;

  // If g_floodMaxDemerits == 0, then we go against g_floodMinTime^2.
  
  if( !g_floodMaxDemerits.integer )
     maximumDemerits = g_floodMinTime.integer * g_floodMinTime.integer / 1000;
  else
     maximumDemerits = g_floodMaxDemerits.integer;

  if( ent->client->pers.floodDemerits > maximumDemerits )
     return qtrue;

  return qfalse;
}
  
/*
==================
Cmd_Give_f

Give items to a client
==================
*/
void Cmd_Give_f( gentity_t *ent )
{
  char      *name;
  qboolean  give_all = qfalse;

  name = ConcatArgs( 1 );
  if( Q_stricmp( name, "all" ) == 0 )
    give_all = qtrue;

  if( give_all || Q_stricmp( name, "health" ) == 0 )
  {
    if(!g_devmapNoGod.integer)
    {
     ent->health = ent->client->ps.stats[ STAT_MAX_HEALTH ];
     BG_AddUpgradeToInventory( UP_MEDKIT, ent->client->ps.stats );
    }
  }

  if( give_all || Q_stricmpn( name, "funds", 5 ) == 0 )
  {
    int credits = give_all ? HUMAN_MAX_CREDITS : atoi( name + 6 );
    G_AddCreditToClient( ent->client, credits, qtrue );
  }

  if( give_all || Q_stricmp( name, "stamina" ) == 0 )
    ent->client->ps.stats[ STAT_STAMINA ] = MAX_STAMINA;

  if( Q_stricmp( name, "poison" ) == 0 )
  {
    ent->client->ps.stats[ STAT_STATE ] |= SS_BOOSTED;
    ent->client->lastBoostedTime = level.time;
  }

  if( give_all || Q_stricmp( name, "ammo" ) == 0 )
  {
    int maxAmmo, maxClips;
    gclient_t *client = ent->client;

    if( client->ps.weapon != WP_ALEVEL3_UPG &&
        BG_FindInfinteAmmoForWeapon( client->ps.weapon ) )
      return;

    BG_FindAmmoForWeapon( client->ps.weapon, &maxAmmo, &maxClips );

    if( BG_FindUsesEnergyForWeapon( client->ps.weapon ) &&
        BG_InventoryContainsUpgrade( UP_BATTPACK, client->ps.stats ) )
      maxAmmo = (int)( (float)maxAmmo * BATTPACK_MODIFIER );

    BG_PackAmmoArray( client->ps.weapon, client->ps.ammo, client->ps.powerups, maxAmmo, maxClips );
  }
}


/*
==================
Cmd_God_f

Sets client to godmode

argv(0) god
==================
*/
void Cmd_God_f( gentity_t *ent )
{
  char  *msg;

 if( !g_devmapNoGod.integer )
 {
  ent->flags ^= FL_GODMODE;

  if( !( ent->flags & FL_GODMODE ) )
    msg = "godmode OFF\n";
  else
    msg = "godmode ON\n";
 }
 else
 {
  msg = "Godmode has been disabled.\n";
 }

  trap_SendServerCommand( ent - g_entities, va( "print \"%s\"", msg ) );
}


/*
==================
Cmd_Notarget_f

Sets client to notarget

argv(0) notarget
==================
*/
void Cmd_Notarget_f( gentity_t *ent )
{
  char  *msg;

 if( !g_devmapNoGod.integer )
 {
  ent->flags ^= FL_NOTARGET;

  if( !( ent->flags & FL_NOTARGET ) )
    msg = "notarget OFF\n";
  else
    msg = "notarget ON\n";
 }
 else
 {
  msg = "Godmode has been disabled.\n";
 }

  trap_SendServerCommand( ent - g_entities, va( "print \"%s\"", msg ) );
}


/*
==================
Cmd_Noclip_f

argv(0) noclip
==================
*/
void Cmd_Noclip_f( gentity_t *ent )
{
  char  *msg;

 if( !g_devmapNoGod.integer )
 {
  if( ent->client->noclip )
    msg = "noclip OFF\n";
  else
    msg = "noclip ON\n";

  ent->client->noclip = !ent->client->noclip;
 } 
 else
 {
  msg = "Godmode has been disabled.\n";
 }

  trap_SendServerCommand( ent - g_entities, va( "print \"%s\"", msg ) );
}


/*
==================
Cmd_LevelShot_f

This is just to help generate the level pictures
for the menus.  It goes to the intermission immediately
and sends over a command to the client to resize the view,
hide the scoreboard, and take a special screenshot
==================
*/
void Cmd_LevelShot_f( gentity_t *ent )
{
  BeginIntermission( );
  trap_SendServerCommand( ent - g_entities, "clientLevelShot" );
}

/*
=================
Cmd_Kill_f
=================
*/
void Cmd_Kill_f( gentity_t *ent )
{
  if( ent->client->ps.stats[ STAT_STATE ] & SS_INFESTING )
    return;

  if( ent->client->ps.stats[ STAT_STATE ] & SS_HOVELING )
  {
    trap_SendServerCommand( ent-g_entities, "print \"Leave the hovel first (use your destroy key)\n\"" );
    return;
  }

  if( g_cheats.integer )
  {
    ent->flags &= ~FL_GODMODE;
    ent->client->ps.stats[ STAT_HEALTH ] = ent->health = 0;
    player_die( ent, ent, ent, 100000, MOD_SUICIDE );
  }
  else
  {
    if( ent->suicideTime == 0 )
    {
      trap_SendServerCommand( ent-g_entities, "print \"You will suicide in 20 seconds\n\"" );
      ent->suicideTime = level.time + 20000;
    }
    else if( ent->suicideTime > level.time )
    {
      trap_SendServerCommand( ent-g_entities, "print \"Suicide canceled\n\"" );
      ent->suicideTime = 0;
    }
  }
}

/*
==================
G_LeaveTeam
==================
*/
void G_LeaveTeam( gentity_t *self )
{
  pTeam_t   team = self->client->pers.teamSelection;
  gentity_t *ent;
  int       i;

  if( team == PTE_ALIENS )
    G_RemoveFromSpawnQueue( &level.alienSpawnQueue, self->client->ps.clientNum );
  else if( team == PTE_HUMANS )
    G_RemoveFromSpawnQueue( &level.humanSpawnQueue, self->client->ps.clientNum );
  else
    return;

  for( i = 0; i < level.num_entities; i++ )
  {
    ent = &g_entities[ i ];
    if( !ent->inuse )
      continue;

    // clean up projectiles
    if( ent->s.eType == ET_MISSILE && ent->r.ownerNum == self->s.number )
      G_FreeEntity( ent );
    if( ent->client && ent->client->pers.connected == CON_CONNECTED )
    {
      // stop following clients
      if( ent->client->sess.sessionTeam == TEAM_SPECTATOR &&
          ent->client->sess.spectatorState == SPECTATOR_FOLLOW &&
          ent->client->sess.spectatorClient == self->client->ps.clientNum )
      {
        if( !G_FollowNewClient( ent, 1 ) )
          G_StopFollowing( ent );
      }
      // cure poison
      if( ent->client->ps.stats[ STAT_STATE ] & SS_POISONCLOUDED &&
          ent->client->lastPoisonCloudedClient == self )
        ent->client->ps.stats[ STAT_STATE ] &= ~SS_POISONCLOUDED;
      if( ent->client->ps.stats[ STAT_STATE ] & SS_POISONED &&
          ent->client->lastPoisonClient == self )
        ent->client->ps.stats[ STAT_STATE ] &= ~SS_POISONED;
    }
  }
}

/*
=================
G_ChangeTeam
=================
*/
void G_ChangeTeam( gentity_t *ent, pTeam_t newTeam )
{
  pTeam_t oldTeam = ent->client->pers.teamSelection;
  qboolean isFixingImbalance=qfalse;
 
  if( oldTeam == newTeam )
    return;

  G_LeaveTeam( ent );
  ent->client->pers.teamSelection = newTeam;
  
  
  if ( ( level.numAlienClients - level.numHumanClients > 2 && oldTeam==PTE_ALIENS && newTeam == PTE_HUMANS && level.numHumanSpawns>0 ) ||
       ( level.numHumanClients - level.numAlienClients > 2 && oldTeam==PTE_HUMANS && newTeam == PTE_ALIENS  && level.numAlienSpawns>0 ) ) 
  {
    isFixingImbalance=qtrue;
  }

  // under certain circumstances, clients can keep their kills and credits
  // when switching teams
  if(ent->client->pers.saved || G_admin_permission( ent, ADMF_TEAMCHANGEFREE ) ||
    ( g_teamImbalanceWarnings.integer && isFixingImbalance ) ||
    ( ( oldTeam == PTE_HUMANS || oldTeam == PTE_ALIENS )
    && ( level.time - ent->client->pers.teamChangeTime ) > 60000 ) )
  {
    if( oldTeam == PTE_NONE )
    {
      // ps.persistant[] from a spectator cannot be trusted
      ent->client->ps.persistant[ PERS_SCORE ] = ent->client->pers.savedScore;
      ent->client->ps.persistant[ PERS_CREDIT ] = ent->client->pers.savedCredit;
    }
    else if( oldTeam == PTE_ALIENS )
    {
      // always save in human credtis
      ent->client->ps.persistant[ PERS_CREDIT ] *=
        (float)FREEKILL_HUMAN / FREEKILL_ALIEN;
    }

    if( newTeam == PTE_NONE )
    {
      // save values before the client enters the spectator team and their
      // ps.persistant[] values become trashed
      ent->client->pers.savedScore = ent->client->ps.persistant[ PERS_SCORE ];
      ent->client->pers.savedCredit = ent->client->ps.persistant[ PERS_CREDIT ];
    }
    else if( newTeam == PTE_ALIENS )
    {
      // convert to alien currency
      ent->client->ps.persistant[ PERS_CREDIT ] *=
        (float)FREEKILL_ALIEN / FREEKILL_HUMAN;
    }
  }
  else
  {
    ent->client->ps.persistant[ PERS_CREDIT ] = 0;
    ent->client->ps.persistant[ PERS_SCORE ] = 0;
    ent->client->pers.savedScore = 0;
    ent->client->pers.savedCredit = 0;
  }
  
  ent->client->ps.persistant[ PERS_KILLED ] = 0;
  ent->client->pers.statscounters.kills = 0;
  ent->client->pers.statscounters.structskilled = 0;
  ent->client->pers.statscounters.assists = 0;
  ent->client->pers.statscounters.repairspoisons = 0;
  ent->client->pers.statscounters.headshots = 0;
  ent->client->pers.statscounters.hits = 0;
  ent->client->pers.statscounters.hitslocational = 0;
  ent->client->pers.statscounters.deaths = 0;
  ent->client->pers.statscounters.feeds = 0;
  ent->client->pers.statscounters.suicides = 0;
  ent->client->pers.statscounters.teamkills = 0;
  ent->client->pers.statscounters.dmgdone = 0;
  ent->client->pers.statscounters.structdmgdone = 0;
  ent->client->pers.statscounters.ffdmgdone = 0;
  ent->client->pers.statscounters.structsbuilt = 0;
  ent->client->pers.statscounters.timealive = 0;
  ent->client->pers.statscounters.timeinbase = 0;
  ent->client->pers.statscounters.dretchbasytime = 0;
  ent->client->pers.statscounters.jetpackusewallwalkusetime = 0;

  if( G_admin_permission( ent, ADMF_DBUILDER ) )
  {
    if( !ent->client->pers.designatedBuilder )
    {
      ent->client->pers.designatedBuilder = qtrue;
      trap_SendServerCommand( ent-g_entities, 
        "print \"Your designation has been restored\n\"" );
    }
  }
  else if( ent->client->pers.designatedBuilder )
  {
    ent->client->pers.designatedBuilder = qfalse;
    trap_SendServerCommand( ent-g_entities, 
     "print \"You have lost designation due to teamchange\n\"" );
  }

  ent->client->pers.classSelection = PCL_NONE;
  ClientSpawn( ent, NULL, NULL, NULL );

  ent->client->pers.joinedATeam = qtrue;
  ent->client->pers.teamChangeTime = level.time;

  //update ClientInfo
  ClientUserinfoChanged( ent->client->ps.clientNum );
  
  
//make sure it doesnt let them save all the time
  if( !ent->client->pers.teamSelection == PTE_NONE && ent->client->pers.saved )
  {
  ent->client->pers.saved = qfalse;
  }
  
  G_CheckDBProtection( );
}

/*
=================
Cmd_Team_f
=================
*/
void Cmd_Team_f( gentity_t *ent )
{
  pTeam_t team;
  pTeam_t oldteam = ent->client->pers.teamSelection;
  char    s[ MAX_TOKEN_CHARS ];
  qboolean force = G_admin_permission(ent, ADMF_FORCETEAMCHANGE);
  int     aliens = level.numAlienClients;
  int     humans = level.numHumanClients;

  // stop team join spam
  if( level.time - ent->client->pers.teamChangeTime < 1000 )
    return;

  if( oldteam == PTE_ALIENS )
    aliens--;
  else if( oldteam == PTE_HUMANS )
    humans--;

  // practice mode
  if( !force && g_practiceCount.integer )
  {
    char name[ MAX_NAME_LENGTH ];

    G_DecolorString( ent->client->pers.netname, name );
    if( strstr( name, g_practiceText.string ) )
    {
      force = qtrue;
    }
  }

  // do warm up
  if( g_doWarmup.integer &&
      level.time - level.startTime < g_warmup.integer * 1000 )
  {
    trap_SendServerCommand( ent - g_entities, va( "print \"team: you can't join"
      " a team during warm up (%d seconds remaining)\n\"",
      g_warmup.integer - ( level.time - level.startTime ) / 1000 ) );
    return;
  }

  trap_Argv( 1, s, sizeof( s ) );

  if( !strlen( s ) )
  {
    trap_SendServerCommand( ent-g_entities, va("print \"team: %i\n\"",
      oldteam ) );
    return;
  }

  if( Q_stricmp( s, "spectate" ) ){
	if(G_admin_level(ent)<g_minLevelToJoinTeam.integer){
		trap_SendServerCommand( ent-g_entities,"print \"Sorry, but your admin level is only permitted to spectate.\n\"" ); 
		return;
	}
  }
  
  if( !Q_stricmp( s, "spectate" ) )
    team = PTE_NONE;
  else if( !force && ent->client->pers.teamSelection == PTE_NONE &&
           g_maxGameClients.integer && level.numPlayingClients >=
           g_maxGameClients.integer )
  {
    trap_SendServerCommand( ent - g_entities, va( "print \"The maximum number "
      "of playing clients has been reached (g_maxGameClients = %i)\n\"",
      g_maxGameClients.integer ) );
    return;
  }
  else if( !Q_stricmp( s, "aliens" ) )
  {
  if ( ent->client->pers.specd )
    {
      trap_SendServerCommand( ent-g_entities, va( "print \"you cannot join teams\n\"" ) );
      return; 
    }
    else if( level.alienTeamLocked && !force )
    {
      trap_SendServerCommand( ent-g_entities,
        va( "print \"Alien team has been ^1LOCKED\n\"" ) );
      return; 
    }
    else if( level.humanTeamLocked )
    {
      // if only one team has been locked, let people join the other
      // regardless of balance
      force = qtrue;
    }

    if( !force && g_teamForceBalance.integer && aliens > humans )
    {
      G_TriggerMenu( ent - g_entities, MN_A_TEAMFULL );
      return;
    }
    

    team = PTE_ALIENS;
  }
  else if( !Q_stricmp( s, "humans" ) )
  {
    if ( ent->client->pers.specd )
    {
      trap_SendServerCommand( ent-g_entities, va( "print \"you cannot join teams\n\"" ) );
      return; 
    }
    else if( level.humanTeamLocked && !force )
    {
      trap_SendServerCommand( ent-g_entities,
        va( "print \"Human team has been ^1LOCKED\n\"" ) );
      return; 
    }
    else if( level.alienTeamLocked )
    {
      // if only one team has been locked, let people join the other
      // regardless of balance
      force = qtrue;
    }

    if( !force && g_teamForceBalance.integer && humans > aliens )
    {
      G_TriggerMenu( ent - g_entities, MN_H_TEAMFULL );
      return;
    }

    team = PTE_HUMANS;
  }
  else if( !Q_stricmp( s, "auto" ) )
  {
   if ( ent->client->pers.specd )
    {
      trap_SendServerCommand( ent-g_entities, va( "print \"you cannot join teams\n\"" ) );
      return; 
    }
    
    else if( level.humanTeamLocked && level.alienTeamLocked )
      team = PTE_NONE;
    else if( humans > aliens )
      team = PTE_ALIENS;
    else if( humans < aliens )
      team = PTE_HUMANS;
    else
      team = PTE_ALIENS + ( rand( ) % 2 );

    if( team == PTE_ALIENS && level.alienTeamLocked )
      team = PTE_HUMANS;
    else if( team == PTE_HUMANS && level.humanTeamLocked )
      team = PTE_ALIENS;
  }
  else
  {
    trap_SendServerCommand( ent-g_entities, va( "print \"Unknown team: %s\n\"", s ) );
    return;
  }

  // stop team join spam
  if( oldteam == team )
    return;

  //guard against build timer exploit
  if( oldteam != PTE_NONE &&
     ( ent->client->ps.stats[ STAT_PCLASS ] == PCL_ALIEN_BUILDER0 ||
       ent->client->ps.stats[ STAT_PCLASS ] == PCL_ALIEN_BUILDER0_UPG ||
       BG_InventoryContainsWeapon( WP_HBUILD, ent->client->ps.stats ) ||
       BG_InventoryContainsWeapon( WP_HBUILD2, ent->client->ps.stats ) ) &&
      ent->client->ps.stats[ STAT_MISC ] > 0 )
  {
    trap_SendServerCommand( ent-g_entities,
        va( "print \"You cannot change teams until build timer expires\n\"" ) );
    return;
  }

   if (team != PTE_NONE)
   {
 	  char	namebuff[32];
 
 	  Q_strncpyz (namebuff, ent->client->pers.netname, sizeof(namebuff));
 	  Q_CleanStr (namebuff);
 
 	  if (!namebuff[0] || !Q_stricmp (namebuff, "UnnamedPlayer"))
 	  {
 		  trap_SendServerCommand( ent-g_entities, va( "print \"Please set your player name before joining a team. Press ESC and use the Options / Game menu  or use /name in the console\n\"") );
 		  return;
 	  }
   }
 

  G_ChangeTeam( ent, team );

   if( team == PTE_ALIENS ) {
     if ( oldteam == PTE_HUMANS )
       trap_SendServerCommand( -1, va( "print \"%s" S_COLOR_WHITE " abandoned humans and joined the aliens\n\"",
                                       ent->client->pers.netname ) );
     else
       trap_SendServerCommand( -1, va( "print \"%s" S_COLOR_WHITE " joined the aliens\n\"",
                                       ent->client->pers.netname ) );
   }
   else if( team == PTE_HUMANS ) {
     if ( oldteam == PTE_ALIENS )
       trap_SendServerCommand( -1, va( "print \"%s" S_COLOR_WHITE " abandoned aliens and joined the humans\n\"",
                                       ent->client->pers.netname ) );
     else
       trap_SendServerCommand( -1, va( "print \"%s" S_COLOR_WHITE " joined the humans\n\"",
                                       ent->client->pers.netname ) );
   }
   else if( team == PTE_NONE ) {
     if ( oldteam == PTE_HUMANS )
       trap_SendServerCommand( -1, va( "print \"%s" S_COLOR_WHITE " left the humans\n\"",
                                       ent->client->pers.netname ) );
     else
       trap_SendServerCommand( -1, va( "print \"%s" S_COLOR_WHITE " left the aliens\n\"",
                                       ent->client->pers.netname ) );
   }
}


/*
==================
G_Say
==================
*/
static void G_SayTo( gentity_t *ent, gentity_t *other, int mode, int color, const char *name, const char *message, const char *prefix )
{
  qboolean ignore = qfalse;
  qboolean specAllChat = qfalse;

  if( !other )
    return;

  if( !other->inuse )
    return;

  if( !other->client )
    return;

  if( other->client->pers.connected != CON_CONNECTED )
    return;

  if( ( mode == SAY_TEAM || mode == SAY_ACTION_T ) && !OnSameTeam( ent, other ) )
  {
    if( other->client->pers.teamSelection != PTE_NONE )
      return;

    specAllChat = G_admin_permission( other, ADMF_SPEC_ALLCHAT );
    if( !specAllChat )
      return;

    // specs with ADMF_SPEC_ALLCHAT flag can see team chat
  }
  
  if( mode == SAY_ADMINS &&
     (!G_admin_permission( other, ADMF_ADMINCHAT) || other->client->pers.ignoreAdminWarnings ) )
     return;

  if( BG_ClientListTest( &other->client->sess.ignoreList, ent-g_entities ) )
    ignore = qtrue;
  
  trap_SendServerCommand( other-g_entities, va( "%s \"%s%s%s%c%c%s\"",
    ( mode == SAY_TEAM || mode == SAY_ACTION_T ) ? "tchat" : "chat",
    ( ignore ) ? "[skipnotify]" : "",
    ( specAllChat ) ? prefix : "",
    name, Q_COLOR_ESCAPE, color, message ) );
}

#define EC    "\x19"

void G_Say( gentity_t *ent, gentity_t *target, int mode, const char *chatText )
{
  int         j;
  gentity_t   *other;
  int         color;
  const char  *prefix;
  char        name[ 64 ];
  // don't let text be too long for malicious reasons
  char        text[ MAX_SAY_TEXT ];
  char        location[ 64 ];

  // Bail if the text is blank.
  if( ! chatText[0] )
     return;

  // Flood limit.  If they're talking too fast, determine that and return.
  if( g_floodMinTime.integer )
    if ( G_Flood_Limited( ent ) )
    {
      trap_SendServerCommand( ent-g_entities, "print \"Your chat is flood-limited; wait before chatting again\n\"" );
      return;
    }
       
  if (g_chatTeamPrefix.integer)
  {
    switch( ent->client->pers.teamSelection)
    {
      default:
      case PTE_NONE:
        prefix = "[^3S^7] ";
        break;

      case PTE_ALIENS:
        prefix = "[^1A^7] ";
        break;

      case PTE_HUMANS:
        prefix = "[^4H^7] ";
    }
  }
  else
    prefix = "";

  switch( mode )
  {
    default:
    case SAY_ALL:
      G_LogPrintf( "say: %s^7: %s^7\n", ent->client->pers.netname, chatText );
      Com_sprintf( name, sizeof( name ), "%s%s%c%c"EC": ", prefix,
                   ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE );
      color = COLOR_GREEN;
      break;

    case SAY_TEAM:
      G_LogPrintf( "sayteam: %s^7: %s^7\n", ent->client->pers.netname, chatText );
      if( Team_GetLocationMsg( ent, location, sizeof( location ) ) )
        Com_sprintf( name, sizeof( name ), EC"(%s%c%c"EC") (%s)"EC": ",
          ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE, location );
      else
        Com_sprintf( name, sizeof( name ), EC"(%s%c%c"EC")"EC": ",
          ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE );
      
      if( ent->client->pers.teamSelection == PTE_NONE )
        color = COLOR_YELLOW;
      else
        color = COLOR_CYAN;
      break;

    case SAY_TELL:
      if( target && OnSameTeam( target, ent ) &&
          Team_GetLocationMsg( ent, location, sizeof( location ) ) )
        Com_sprintf( name, sizeof( name ), EC"[%s%c%c"EC"] (%s)"EC": ",
          ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE, location );
      else
        Com_sprintf( name, sizeof( name ), EC"[%s%c%c"EC"]"EC": ",
          ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE );
      color = COLOR_MAGENTA;
      break;
      
    case SAY_ACTION:
      G_LogPrintf( "action: %s: %s\n", ent->client->pers.netname, chatText );
      Com_sprintf( name, sizeof( name ), "^2%s^7%s%s%c%c"EC" ", g_actionPrefix.string, prefix,
                   ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE );
      color = COLOR_WHITE;
      break;

    case SAY_ACTION_T:
      G_LogPrintf( "actionteam: %s: %s\n", ent->client->pers.netname, chatText );
      if( Team_GetLocationMsg( ent, location, sizeof( location ) ) )
        Com_sprintf( name, sizeof( name ), EC"^5%s^7%s%c%c"EC"(%s)"EC" ", g_actionPrefix.string, 
          ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE, location );
      else
        Com_sprintf( name, sizeof( name ), EC"^5%s^7%s%c%c"EC""EC" ", g_actionPrefix.string, 
          ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE );
      color = COLOR_WHITE;
      break;
      
      case SAY_ADMINS:
        if( G_admin_permission( ent, ADMF_ADMINCHAT ) ) //Differentiate between inter-admin chatter and user-admin alerts
        {
         G_LogPrintf( "say_admins: [ADMIN]%s: %s\n", ( ent ) ? ent->client->pers.netname : "console", chatText );
         Com_sprintf( name, sizeof( name ), "%s[ADMIN]%s%c%c"EC": ", prefix,
                    ( ent ) ? ent->client->pers.netname : "console", Q_COLOR_ESCAPE, COLOR_WHITE );
	color = COLOR_MAGENTA;
	}
	else
	{
         G_LogPrintf( "say_admins: [PLAYER]%s: %s\n", ent->client->pers.netname, chatText );
         Com_sprintf( name, sizeof( name ), "%s[PLAYER]%s%c%c"EC": ", prefix,
                    ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE );
	color = COLOR_MAGENTA;
	}

	break;
  }
  
  if( mode!=SAY_TEAM && ent->client->pers.teamSelection == PTE_NONE && G_admin_level(ent)<g_minLevelToSpecMM1.integer )
  {
    trap_SendServerCommand( ent-g_entities,va( "print \"Sorry, but your admin level may only use teamchat while spectating.\n\"") ); 
    return;
  }

  Q_strncpyz( text, chatText, sizeof( text ) );

  if( target )
  {
    G_SayTo( ent, target, mode, color, name, text, prefix );
    return;
  }
  
   
   if( g_adminParseSay.integer )
   {
     if( G_admin_cmd_check ( ent, qtrue ) && g_adminSayFilter.integer ) 
     {
       return;
     }
   }

  // send it to all the apropriate clients
  for( j = 0; j < level.maxclients; j++ )
  {
    other = &g_entities[ j ];
    G_SayTo( ent, other, mode, color, name, text, prefix );
  }
  

}

static void Cmd_SayArea_f( gentity_t *ent )
{
  int    entityList[ MAX_GENTITIES ];
  int    num, i;
  int    color = COLOR_BLUE;
  const char  *prefix;
  vec3_t range = { HELMET_RANGE, HELMET_RANGE, HELMET_RANGE };
  vec3_t mins, maxs;
  char   *msg = ConcatArgs( 1 );
  char   name[ 64 ];
  char        location[ 64 ];
    
    if( g_floodMinTime.integer )
    if ( G_Flood_Limited( ent ) )
    {
      trap_SendServerCommand( ent-g_entities, "print \"Your chat is flood-limited; wait before chatting again\n\"" );
      return;
    }
       
  
  if (g_chatTeamPrefix.integer)
  {
    switch( ent->client->pers.teamSelection)
    {
      default:
      case PTE_NONE:
        prefix = "[S] ";
        break;

      case PTE_ALIENS:
        prefix = "[A] ";
        break;

      case PTE_HUMANS:
        prefix = "[H] ";
    }
  }
  else
    prefix = "";

  G_LogPrintf( "sayarea: %s %s: %s\n", prefix, ent->client->pers.netname, msg );
  
      if( g_sayAreaLocations.integer && Team_GetLocationMsg( ent, location, sizeof( location ) ) )
        Com_sprintf( name, sizeof( name ), EC"%s<%s%c%c"EC"> (%s)"EC" ",
          prefix, ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE, location );
      else
        Com_sprintf( name, sizeof( name ), EC"%s<%s%c%c"EC"> ",
          prefix, ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE );

  VectorAdd( ent->s.origin, range, maxs );
  VectorSubtract( ent->s.origin, range, mins );

  num = trap_EntitiesInBox( mins, maxs, entityList, MAX_GENTITIES );
  for( i = 0; i < num; i++ )
    G_SayTo( ent, &g_entities[ entityList[ i ] ], SAY_TEAM, color, name, msg, prefix );
}

/*
==================
Cmd_Say_f
==================
*/
static void Cmd_Say_f( gentity_t *ent )
{
  char    *p;
  char    *args;
  int     offset = 0;
  int     mode = SAY_ALL;

  args = G_SayConcatArgs( 0 );
  if( Q_stricmpn( args, "say_team ", 9 ) == 0 )
    mode = SAY_TEAM;
  if( Q_stricmpn( args, "say_admins ", 11 ) == 0 || Q_stricmpn( args, "a ", 2 ) == 0)
    mode = SAY_ADMINS;

  // support parsing /m out of say text since some people have a hard
  // time figuring out what the console is.
  if( !Q_stricmpn( args, "say /m ", 7 ) ||
      !Q_stricmpn( args, "say_team /m ", 12 ) || 
      !Q_stricmpn( args, "say /mt ", 8 ) || 
      !Q_stricmpn( args, "say_team /mt ", 13 ) )
  {
    G_PrivateMessage( ent );
    return;
  }
  
   
   if( !Q_stricmpn( args, "say /a ", 7) ||
       !Q_stricmpn( args, "say_team /a ", 12) ||
       !Q_stricmpn( args, "say /say_admins ", 16) ||
       !Q_stricmpn( args, "say_team /say_admins ", 21) )
   {
       mode = SAY_ADMINS;
       offset=3;
   }
   
   if( mode == SAY_ADMINS)  
   if(!G_admin_permission( ent, ADMF_ADMINCHAT ) )
   {
     if( !g_publicSayadmins.integer )
     {
      ADMP( "Sorry, but public use of say_admins has been disabled.\n" );
      return;
     }
     else
     {
       ADMP( "Your message has been sent to any available admins and to the server logs.\n" );
     }
   }
   

  if(!Q_stricmpn( args, "say /me ", 8 ) )
  {
   if( g_allowActions.integer ) 
   { 
    mode = SAY_ACTION;
    offset = 4;
   } else return;
  }
  else if(!Q_stricmpn( args, "say_team /me ", 13 ) )
  {
   if( g_allowActions.integer ) 
   { 
    mode = SAY_ACTION_T;
    offset = 4;
   } else return;
  }
  else if( !Q_stricmpn( args, "me ", 3 ) )
  {
   if( g_allowActions.integer ) 
   { 
    mode = SAY_ACTION;
   } else return;
  }
  else if( !Q_stricmpn( args, "me_team ", 8 ) )
  {
   if( g_allowActions.integer ) 
   { 
    mode = SAY_ACTION_T;
   } else return;
  }


  if( g_allowShare.integer )
  {
    args = G_SayConcatArgs(0);
    if( !Q_stricmpn( args, "say /share", 10 ) ||
      !Q_stricmpn( args, "say_team /share", 15 ) )
    {
      Cmd_Share_f( ent );
      return;
    }
   if( !Q_stricmpn( args, "say /donate", 11 ) ||
      !Q_stricmpn( args, "say_team /share", 16 ) )
    {
      Cmd_Donate_f( ent );
      return;
    }
  }

  args = G_SayConcatArgs(0);
  if( !Q_stricmpn( args, "say /join", 9 ) )
  {
    Cmd_Join_f( ent );
    return;
  }
  if( !Q_stricmpn( args, "say /part", 9 ) )
  {
    Cmd_Part_f( ent );
    return;
  }
  if( !Q_stricmpn( args, "say /0", 6 ) ||
      !Q_stricmpn( args, "say /1", 6 ) ||
      !Q_stricmpn( args, "say /2", 6 ) ||
      !Q_stricmpn( args, "say /3", 6 ) ||
      !Q_stricmpn( args, "say /4", 6 ) ||
      !Q_stricmpn( args, "say /5", 6 ) ||
      !Q_stricmpn( args, "say /6", 6 ) ||
      !Q_stricmpn( args, "say /7", 6 ) ||
      !Q_stricmpn( args, "say /8", 6 ) ||
      !Q_stricmpn( args, "say /9", 6 ) )
  {
    Cmd_Channel_f( ent );
    return;
  }

  if( trap_Argc( ) < 2 )
    return;

  p = ConcatArgs( 1 );

  p += offset;
  
  G_Say( ent, NULL, mode, p );
}

/*
==================
Cmd_Tell_f
==================
*/
static void Cmd_Tell_f( gentity_t *ent )
{
  int     targetNum;
  gentity_t *target;
  char    *p;
  char    arg[MAX_TOKEN_CHARS];

  if( trap_Argc( ) < 2 )
    return;

  trap_Argv( 1, arg, sizeof( arg ) );
  targetNum = atoi( arg );

  if( targetNum < 0 || targetNum >= level.maxclients )
    return;

  target = &g_entities[ targetNum ];
  if( !target || !target->inuse || !target->client )
    return;

  p = ConcatArgs( 2 );

  G_LogPrintf( "tell: %s^7 to %s^7: %s^7\n", ent->client->pers.netname, target->client->pers.netname, p );
  G_Say( ent, target, SAY_TELL, p );
  // don't tell to the player self if it was already directed to this player
  // also don't send the chat back to a bot
  if( ent != target )
    G_Say( ent, ent, SAY_TELL, p );
}

void Cmd_Join_f( gentity_t *ent )
{
  char pass[CHAT_MAXPASS];
  char arg[MAX_TOKEN_CHARS];
  int  skipargs = 0;
  int  chan;
  int  i;
  gentity_t *target;

  if( g_floodMinTime.integer &&
      G_Flood_Limited( ent ) )
  {
    trap_SendServerCommand( ent-g_entities,
      "print \"Your chat is flood-limited; wait before chatting again\n\"" );
    return;
  }

  G_SayArgv( skipargs, arg, sizeof( arg ) );
  if( !Q_stricmp( arg, "say" ) )
    skipargs++;

  if( G_SayArgc( ) < 2 + skipargs )
  {
    char message[ 64 ];
    int n;
    
    n = 0;
    for( i = 0; i < CHAT_MAXCHAN && n < 64 - 3; i++ )
    {
      if( ent->client->pers.chat[i][0] )
      {
        message[ n ] = ' '; n++;
        message[ n ] = '0' + i; n++;
      }
    }
    if( n == 0)
      Com_sprintf( message, sizeof( message ), " no channels, use: /join [0-%d] (password)", CHAT_MAXCHAN - 1 );
    else
      message[ n ] = '\0';

    trap_SendServerCommand( ent-g_entities, va( "print \"joined in:%s\n\"", message ) );
    return;
  }

  G_SayArgv( 1 + skipargs, arg, sizeof( arg ) );
  chan = atoi( arg );
  if( chan < 0 || chan >= CHAT_MAXCHAN )
  {
    trap_SendServerCommand( ent-g_entities,
     va( "print \"^3/join^7: invalid channel, usage: /join [0-%d] (password)\n\"", CHAT_MAXCHAN - 1 ) );
    return;
  }

  G_SayArgv( 2 + skipargs, pass, sizeof( pass ) );
  if( pass[0] == '\0' )
    Q_strncpyz( pass, "default", sizeof( pass ) );

  if( !Q_stricmp( ent->client->pers.chat[chan], pass ) )
  {
    trap_SendServerCommand( ent-g_entities,
      va( "print \"^3/join^7: already join to channel %d\n\"", chan ) );
    return;
  }

  G_LogPrintf( "join: channel %d %s^7\n", chan, ent->client->pers.netname );
  Q_strncpyz( ent->client->pers.chat[chan], pass, sizeof( ent->client->pers.chat[chan] ) );
  G_admin_chat_update( ent, chan );

  for( i = 0; i < level.maxclients; i++ )
  {
    target = &g_entities[ i ];
    if( target && target->client &&
        target->client->pers.connected == CON_CONNECTED &&
        !Q_stricmp( target->client->pers.chat[ chan ], pass ) )
    {
      trap_SendServerCommand( i, va( "print \"join: %s^7 has joined channel #%d\n\"",
        ent->client->pers.netname, chan ) );
    }
  }
}

void Cmd_Part_f( gentity_t *ent )
{
  char arg[MAX_TOKEN_CHARS];
  int  skipargs = 0;
  int  chan;
  int  i;
  gentity_t *target;

  G_SayArgv( skipargs, arg, sizeof( arg ) );
  if( !Q_stricmp( arg, "say" ) )
    skipargs++;

  if( G_SayArgc( ) < 2 + skipargs )
  {
    trap_SendServerCommand( ent-g_entities, va( "print \"^3/part^7 usage: /part [0-%d]\n\"", CHAT_MAXCHAN - 1 ) );
    return;
  }

  if( g_floodMinTime.integer &&
      G_Flood_Limited( ent ) )
  {
    trap_SendServerCommand( ent-g_entities, "print \"Your chat is flood-limited; wait before chatting again\n\"" );
    return;
  }

  G_SayArgv( 1 + skipargs, arg, sizeof( arg ) );
  chan = atoi( arg );
  if( chan < 0 || chan >= CHAT_MAXCHAN )
  {
    trap_SendServerCommand( ent-g_entities,
      va( "print \"^3/part^7: invalid channel, available channels are 0 to %d\n\"", CHAT_MAXCHAN - 1 ) );
    return;
  }

  if( ent->client->pers.chat[chan][0] == '\0' )
  {
    trap_SendServerCommand( ent-g_entities,
      "print \"^3/part^7: not in that channel\n\"" );
    return;
  }

  for( i = 0; i < level.maxclients; i++ )
  {
    target = &g_entities[ i ];
    if( target && target->client &&
        target->client->pers.connected == CON_CONNECTED &&
        !Q_stricmp( target->client->pers.chat[ chan ], ent->client->pers.chat[ chan ] ) )
    {
      trap_SendServerCommand( i, va( "print \"part: %s^7 has left channel #%d\n\"",
        ent->client->pers.netname, chan ) );
    }
  }

  G_LogPrintf( "part: channel %d %s^7\n", chan, ent->client->pers.netname );
  Q_strncpyz( ent->client->pers.chat[chan], "", sizeof( ent->client->pers.chat[chan] ) );
  G_admin_chat_update( ent, chan );
}

void Cmd_Channel_f( gentity_t *ent )
{
  char arg[MAX_TOKEN_CHARS];
  char str[MAX_STRING_CHARS];
  char *cmd, *p;
  char location[ 64 ];
  char escaped[ 64 ];
  int  chan;
  int  i;
  int  num = 0;
  int  skipargs = 0;
  qboolean who = qfalse;
  gentity_t *target;

  if( g_floodMinTime.integer &&
      G_Flood_Limited( ent ) )
  {
    trap_SendServerCommand( ent-g_entities, "print \"Your chat is flood-limited; wait before chatting again\n\"" );
    return;
  }

  G_SayArgv( skipargs, arg, sizeof( arg ) );
  if( !Q_stricmp( arg, "say" ) )
  {
    skipargs++;
    G_SayArgv( skipargs, arg, sizeof( arg ) );
  }
  cmd = arg;
  if( *cmd == '/' )
    cmd++;
  chan = atoi( cmd );
  if( chan < 0 || chan >= CHAT_MAXCHAN ||
      ent->client->pers.chat[chan][0] == '\0' )
  {
    trap_SendServerCommand( ent-g_entities,
      va( "print \"^3/%s^7: you are not joined to that channel, use /join\n\"", cmd ) );
    return;
  }

  p = G_SayConcatArgs( 1 + skipargs );
  if( !p || p[0] == '\0' )
    who = qtrue;

  if( Team_GetLocationMsg( ent, location, sizeof( location ) ) )
  {
    Com_sprintf( escaped, sizeof( escaped ), " (^4%s^7)", location );
  }
  else
  {
    escaped[ 0 ] = '\0';
  }

  str[ 0 ] = '\0';
  for( i = 0; i < level.maxclients; i++ )
  {
    target = &g_entities[ i ];
    if( target && target->client &&
        target->client->pers.connected == CON_CONNECTED &&
        !Q_stricmp( target->client->pers.chat[ chan ], ent->client->pers.chat[ chan ] ) )
    {
      if( num > 0 )
        Q_strcat( str, sizeof( str ), "^7, " );
      Q_strcat( str, sizeof( str ), target->client->pers.netname );
      num++;
    }
  }

  if( who )
  {
    trap_SendServerCommand( ent-g_entities,
      va( "print \"channel ^3%d^7 has %s%s and contains %d player%s%s%s\n\"",
      chan,
      ( !Q_stricmp( ent->client->pers.chat[ chan ], "default" ) ) ? "no password" : "password ",
      ( !Q_stricmp( ent->client->pers.chat[ chan ], "default" ) ) ? "" : ent->client->pers.chat[ chan ],
      num,
      ( num == 1 ) ? "" : "s",
      ( num ) ? ": " : "",
      str ) );
    return;
  }

  G_LogPrintf( "channel%d: %s^7 %s\n", chan, ent->client->pers.netname, p );
  for( i = 0; i < level.maxclients; i++ )
  {
    target = &g_entities[ i ];
    if( target && target->client &&
        target->client->pers.connected == CON_CONNECTED &&
        !Q_stricmp( target->client->pers.chat[ chan ], ent->client->pers.chat[ chan ] ) )
    {
      trap_SendServerCommand( i, va( "chat \"(^1#%d^7 %d) [%s^7]%s: ^1%s^7\"",
        chan, num, ent->client->pers.netname,
        ( OnSameTeam( target, ent ) ) ? escaped : "",
        p ) );
    }
  }
}
 
/*
==================
Cmd_Where_f
==================
*/
void Cmd_Where_f( gentity_t *ent )
{
  trap_SendServerCommand( ent-g_entities, va( "print \"%s\n\"", vtos( ent->s.origin ) ) );
}

/*
==================
Cmd_CallVote_f
==================
*/
void Cmd_CallVote_f( gentity_t *ent )
{
  int   i;
  char  arg1[ MAX_STRING_TOKENS ];
  char  arg2[ MAX_STRING_TOKENS ];
  char  arg3[ MAX_STRING_TOKENS ];
  int   clientNum = -1;
  char  name[ MAX_NETNAME ];
  char *arg2plus;
  char nullstring[] = "";
  char  message[ MAX_STRING_CHARS ];
  	
  arg2plus = G_SayConcatArgs( 2 );

  if( !g_allowVote.integer )
  {
    trap_SendServerCommand( ent-g_entities, "print \"Voting not allowed here\n\"" );
    return;
  }
  
  if( g_voteMinTime.integer
    && ent->client->pers.firstConnect 
    && level.time - ent->client->pers.enterTime < g_voteMinTime.integer * 1000
    && !G_admin_permission( ent, ADMF_NO_VOTE_LIMIT ) 
    && (level.numPlayingClients > 0 && level.numConnectedClients>1) )
  {
    trap_SendServerCommand( ent-g_entities, va(
      "print \"You must wait %d seconds after connecting before calling a vote\n\"",
      g_voteMinTime.integer ) );
    return;
  }

  if( level.voteTime )
  {
    trap_SendServerCommand( ent-g_entities, "print \"A vote is already in progress\n\"" );
    return;
  }

  if( g_voteLimit.integer > 0
    && ent->client->pers.voteCount >= g_voteLimit.integer
    && !G_admin_permission( ent, ADMF_NO_VOTE_LIMIT ) )
  {
    trap_SendServerCommand( ent-g_entities, va(
      "print \"You have already called the maximum number of votes (%d)\n\"",
      g_voteLimit.integer ) );
    return;
  }
  
  if( ent->client->pers.muted )
  {
    trap_SendServerCommand( ent - g_entities,
      "print \"You are muted and cannot call votes\n\"" );
    return;
  }

  // make sure it is a valid command to vote on
  trap_Argv( 1, arg1, sizeof( arg1 ) );
  trap_Argv( 2, arg2, sizeof( arg2 ) );
  trap_Argv( 3, arg3, sizeof( arg3 ) );

  if( strchr( arg1, ';' ) || strchr( arg2, ';' ) )
  {
    trap_SendServerCommand( ent-g_entities, "print \"Invalid vote string\n\"" );
    return;
  }

  // if there is still a vote to be executed
  if( level.voteExecuteTime )
  {
    if( !Q_stricmp( level.voteString, "map_restart" ) )
    {
      G_admin_maplog_result( "r" );
    }
    else if( !Q_stricmpn( level.voteString, "map", 3 ) )
    {
      G_admin_maplog_result( "m" );
    }
    else if( !Q_stricmpn( level.voteString, "!restart", 8 ) )
    {
      G_admin_maplog_result( "l" );
    }

    level.voteExecuteTime = 0;
    trap_SendConsoleCommand( EXEC_APPEND, va( "%s\n", level.voteString ) );
  }
  
  level.votePercentToPass=50;

  // detect clientNum for partial name match votes
  if( !Q_stricmp( arg1, "kick" ) ||
    !Q_stricmp( arg1, "mute" ) ||
    !Q_stricmp( arg1, "unmute" ) )
  {
    int clientNums[ MAX_CLIENTS ] = { -1 };
    int numMatches=0;
    char err[ MAX_STRING_CHARS ];

    if( !arg2[ 0 ] )
    {
      trap_SendServerCommand( ent-g_entities,
        "print \"callvote: no target\n\"" );
      return;
    }
    
        if( !arg3[ 0 ] )
    {
      trap_SendServerCommand( ent-g_entities,
        "print \"callvote: no reason\n\"" );
      return;
    }


    numMatches = G_ClientNumbersFromString( arg2, clientNums );
    if( numMatches == 1 )
    {
      // there was only one partial name match
      clientNum = clientNums[ 0 ]; 
    }
    else
    {
      // look for an exact name match (sets clientNum to -1 if it fails) 
      clientNum = G_ClientNumberFromString( ent, arg2 );
    }
    
    if( clientNum==-1  && numMatches > 1 ) 
    {
      G_MatchOnePlayer( clientNums, err, sizeof( err ) );
      ADMP( va( "^3callvote: ^7%s\n", err ) );
      return;
    }
    
    if( clientNum != -1 &&
      level.clients[ clientNum ].pers.connected == CON_DISCONNECTED )
    {
      clientNum = -1;
    }

    if( clientNum != -1 )
    {
      Q_strncpyz( name, level.clients[ clientNum ].pers.netname,
        sizeof( name ) );
      Q_CleanStr( name );
      if ( G_admin_permission ( &g_entities[ clientNum ], ADMF_IMMUNITY ) )
        Com_sprintf( message, sizeof( message ), "%s^7 attempted /callvote %s %s on immune admin %s^7",
          ent->client->pers.netname, arg1, arg2, g_entities[ clientNum ].client->pers.netname );
     
    }
    else
    {
      trap_SendServerCommand( ent-g_entities,
        "print \"callvote: invalid player\n\"" );
      return;
    }
  }
 
  if( !Q_stricmp( arg1, "kick" ) )
  {
    if( G_admin_permission( &g_entities[ clientNum ], ADMF_IMMUNITY ) )
    {
      trap_SendServerCommand( ent-g_entities,
        "print \"callvote: admin is immune from vote kick\n\"" );
      G_AdminsPrintf("%s\n",message);
      G_admin_adminlog_log( ent, "vote", NULL, 0, qfalse );
      return;
    }
    
    // use ip in case this player disconnects before the vote ends
    Com_sprintf( level.voteString, sizeof( level.voteString ),
      "!ban %s %d %s", level.clients[ clientNum ].pers.ip,
      g_adminTempBan.integer + 1, G_SayConcatArgs( 3 ) );
      //bit of a hack here, but it does work :)
    Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ),
      "Kick player \'%s^7\' for \'%s^7\'", name, G_SayConcatArgs( 3 )  );
  }
  else if( !Q_stricmp( arg1, "mute" ) )
  {
    if( level.clients[ clientNum ].pers.muted )
    {
      trap_SendServerCommand( ent-g_entities,
        "print \"callvote: player is already muted\n\"" );
      return;
    }

    if( G_admin_permission( &g_entities[ clientNum ], ADMF_IMMUNITY ) )
    {
      trap_SendServerCommand( ent-g_entities,
        "print \"callvote: admin is immune from vote mute\n\"" );
      G_AdminsPrintf("%s\n",message);
      G_admin_adminlog_log( ent, "vote", NULL, 0, qfalse );
      return;
    }
    
    Com_sprintf( level.voteString, sizeof( level.voteString ),
      "!mute %i", clientNum );
    Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ),
      "Mute player \'%s^7\' for \'%s^7\'", name, G_SayConcatArgs( 3 ) );
  }
  else if( !Q_stricmp( arg1, "unmute" ) )
  {
    if( !level.clients[ clientNum ].pers.muted )
    {
      trap_SendServerCommand( ent-g_entities,
        "print \"callvote: player is not currently muted\n\"" );
      return;
    }
    Com_sprintf( level.voteString, sizeof( level.voteString ),
      "!unmute %i", clientNum );
    Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ),
      "Un-Mute player \'%s\' for \'%s\'", name, G_SayConcatArgs( 3 ) );
  }
  else if( !Q_stricmp( arg1, "map_restart" ) )
  {
    if( g_mapvoteMaxTime.integer 
      && level.time >= g_mapvoteMaxTime.integer * 1000 
      && !G_admin_permission( ent, ADMF_NO_VOTE_LIMIT ) 
      && (level.numPlayingClients > 0 && level.numConnectedClients>1) )
    {
       trap_SendServerCommand( ent-g_entities, va(
         "print \"You cannot call for a restart after %d seconds\n\"",
         g_mapvoteMaxTime.integer ) );
       G_admin_adminlog_log( ent, "vote", NULL, 0, qfalse );
       return;
    }
    Com_sprintf( level.voteString, sizeof( level.voteString ), "%s", arg1 );
    Com_sprintf( level.voteDisplayString,
        sizeof( level.voteDisplayString ), "Restart current map" );
    level.votePercentToPass = g_mapVotesPercent.integer;
  }
  else if( !Q_stricmp( arg1, "map" ) )
  {
    if( g_mapvoteMaxTime.integer 
      && level.time >= g_mapvoteMaxTime.integer * 1000 
      && !G_admin_permission( ent, ADMF_NO_VOTE_LIMIT ) 
      && (level.numPlayingClients > 0 && level.numConnectedClients>1) )
    {
       trap_SendServerCommand( ent-g_entities, va(
         "print \"You cannot call for a mapchange after %d seconds\n\"",
         g_mapvoteMaxTime.integer ) );
       G_admin_adminlog_log( ent, "vote", NULL, 0, qfalse );
       return;
    }
  
    if( !trap_FS_FOpenFile( va( "maps/%s.bsp", arg2 ), NULL, FS_READ ) )
    {
      trap_SendServerCommand( ent - g_entities, va( "print \"callvote: "
        "'maps/%s.bsp' could not be found on the server\n\"", arg2 ) );
      return;
    }
    
    if (!g_devmapVotes.integer)
    {
    Com_sprintf( level.voteString, sizeof( level.voteString ), "map %s", arg2 );
    }
    else
    {
    Com_sprintf( level.voteString, sizeof( level.voteString ), "devmap %s", arg2 );
    }
    
    Com_sprintf( level.voteDisplayString,
        sizeof( level.voteDisplayString ), "Change to map '%s'", arg2 );
    level.votePercentToPass = g_mapVotesPercent.integer;
  }
  else if( !Q_stricmp( arg1, "layout" ) )
  {
    char map[ 64 ];

    if( g_mapvoteMaxTime.integer 
      && level.time >= g_mapvoteMaxTime.integer * 1000 
      && !G_admin_permission( ent, ADMF_NO_VOTE_LIMIT ) 
      && (level.numPlayingClients > 0 && level.numConnectedClients>1) )
    {
       trap_SendServerCommand( ent-g_entities, va(
         "print \"You cannot call for a layout change after %d seconds\n\"",
         g_mapvoteMaxTime.integer ) );
       G_admin_adminlog_log( ent, "vote", NULL, 0, qfalse );
       return;
    }
  
    trap_Cvar_VariableStringBuffer( "mapname", map, sizeof( map ) );
    if( Q_stricmp( arg2, "*BUILTIN*" ) &&
        !trap_FS_FOpenFile( va( "layouts/%s/%s.dat", map, arg2 ), NULL, FS_READ ) )
    {
      trap_SendServerCommand( ent - g_entities, va( "print \"callvote: "
        "layout '%s' could not be found on the server\n\"", arg2 ) );
      return;
    }
    
    Com_sprintf( level.voteString, sizeof( level.voteString ), "!restart %s", arg2 );
    Com_sprintf( level.voteDisplayString,
        sizeof( level.voteDisplayString ), "Change to layout '%s'", arg2 );
    level.votePercentToPass = g_layoutVotePercent.integer;
  }
  else if( !Q_stricmp( arg1, "nextmap" ) )
  {
    if( G_MapExists( g_nextMap.string ) )
    {
      trap_SendServerCommand( ent - g_entities, va( "print \"callvote: "
        "the next map is already set to '%s^7'\n\"", g_nextMap.string ) );
      return;
    }

    if( !trap_FS_FOpenFile( va( "maps/%s.bsp", arg2 ), NULL, FS_READ ) )
    {
      trap_SendServerCommand( ent - g_entities, va( "print \"callvote: "
        "'maps/%s^7.bsp' could not be found on the server\n\"", arg2 ) );
      return;
    }
    Com_sprintf( level.voteString, sizeof( level.voteString ),
      "set g_nextMap %s", arg2 );
    Com_sprintf( level.voteDisplayString,
        sizeof( level.voteDisplayString ), "Next map '%s^7'", arg2 );
    level.votePercentToPass = g_mapVotesPercent.integer;
  }
  else if( !Q_stricmp( arg1, "draw" ) )
  {
    Com_sprintf( level.voteString, sizeof( level.voteString ), "evacuation" );
    Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ),
        "End match in a draw" );
    level.votePercentToPass = g_mapVotesPercent.integer;
  }
   else if( !Q_stricmp( arg1, "poll" ) )
    {
	if(!g_pollVotes.integer)
     {
       trap_SendServerCommand( ent-g_entities, "print \"Poll votes have been disabled\n\"" );
       return;
     }
    Com_sprintf( level.voteString, sizeof( level.voteString ), nullstring);
    Com_sprintf( level.voteDisplayString,
        sizeof( level.voteDisplayString ), "[Poll] \'%s\'", arg2plus );
   }
   else if( !Q_stricmp( arg1, "sudden_death" ) ||
     !Q_stricmp( arg1, "suddendeath" ) )
   {
     if(!g_suddenDeathVotePercent.integer)
     {
       trap_SendServerCommand( ent-g_entities, "print \"Sudden Death votes have been disabled\n\"" );
       return;
     } 	  
     else if( g_suddenDeath.integer ) 
     {
      trap_SendServerCommand( ent - g_entities, va( "print \"callvote: ""Sudden Death has already begun\n\"") );
      return;
     }
    else 
     {
     level.votePercentToPass = g_suddenDeathVotePercent.integer;	   
     Com_sprintf( level.voteString, sizeof( level.voteString ), "g_suddenDeath 1" );
     Com_sprintf( level.voteDisplayString,
         sizeof( level.voteDisplayString ), "Begin sudden death" );
     }
   }
  else if( !Q_stricmp( arg1, "extend" ) )
  {
    if( !g_extendVotesPercent.integer )
    {
      trap_SendServerCommand( ent-g_entities, "print \"Extend votes have been disabled\n\"" );
      return;
    }
    if( g_extendVotesCount.integer
      && level.extend_vote_count >= g_extendVotesCount.integer )
    {
      trap_SendServerCommand( ent-g_entities,
        va( "print \"callvote: Maximum number of %d extend votes has been reached\n\"",
        g_extendVotesCount.integer ) );
      return;
    }
    if( level.time - level.startTime <
      ( g_timelimit.integer - g_extendVotesTime.integer / 2 ) * 60000 )
    {
      trap_SendServerCommand( ent-g_entities,
        va( "print \"callvote: Extend votes only allowed with less than %d minutes remaining\n\"",
        g_extendVotesTime.integer / 2 ) );
      return;
    }
    level.extend_vote_count++;
    level.votePercentToPass = g_extendVotesPercent.integer;	   
    Com_sprintf( level.voteString, sizeof( level.voteString ),
      "timelimit %i", g_timelimit.integer + g_extendVotesTime.integer );
    Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ),
      "Extend the timelimit by %d minutes", g_extendVotesTime.integer );
  }
  else
  {
    trap_SendServerCommand( ent-g_entities, "print \"Invalid vote string\n\"" );
    trap_SendServerCommand( ent-g_entities, "print \"Valid vote commands are: "
      "map, map_restart, nextmap, layout, draw, extend, kick, mute, unmute, poll, and sudden_death\n" );
    return;
  }
  
  if( level.votePercentToPass!=50 )
  {
    Q_strcat( level.voteDisplayString, sizeof( level.voteDisplayString ), va( " (Needs %d percent)", level.votePercentToPass ) );
  }

  G_admin_adminlog_log( ent, "vote", NULL, 0, qtrue );

  trap_SendServerCommand( -1, va( "print \"%s" S_COLOR_WHITE
         " called a vote: %s\n\"", ent->client->pers.netname, level.voteDisplayString ) );
  trap_SendServerCommand( -1, va( "cp \"A vote has been called\n^2F1: Yes^7, ^1F2: No^7\"" ) );
  
  Q_strcat( level.voteDisplayString, sizeof( level.voteDisplayString ), va( " Called by: %s^7", ent->client->pers.netname ) );

  ent->client->pers.voteCount++;

  // start the voting
  level.voteTime = level.time;
  level.voteYes = 0;
  level.voteNo = 0;

  for( i = 0 ; i < level.maxclients ; i++ )
    level.clients[i].ps.eFlags &= ~EF_VOTED;

  // the caller always votes yes, except for polls
  if( Q_stricmp( arg1, "poll" ) )
  {
    level.voteYes++;
    ent->client->ps.eFlags |= EF_VOTED;
  }

  trap_SetConfigstring( CS_VOTE_TIME, va( "%i", level.voteTime ) );
  trap_SetConfigstring( CS_VOTE_STRING, level.voteDisplayString );
  trap_SetConfigstring( CS_VOTE_YES, va( "%i", level.voteYes ) );
  trap_SetConfigstring( CS_VOTE_NO, va( "%i", level.voteNo ) );
}


/*
==================
Cmd_Vote_f
==================
*/
void Cmd_Vote_f( gentity_t *ent )
{
  char msg[ 64 ];

  if ( level.intermissiontime || level.paused )
  {
    if( level.mapRotationVoteTime )
    {
      trap_Argv( 1, msg, sizeof( msg ) );
      if( msg[ 0 ] == 'y' || msg[ 1 ] == 'Y' || msg[ 1 ] == '1' )
        G_IntermissionMapVoteCommand( ent, qfalse, qtrue );
    }

    return;
  }

  if( !level.voteTime )
  { 
    if( ent->client->pers.teamSelection != PTE_NONE )
    {
      // If there is a teamvote going on but no global vote, forward this vote on as a teamvote
      // (ugly hack for 1.1 cgames + noobs who can't figure out how to use any command that isn't bound by default)
      int     cs_offset = 0;
      if( ent->client->pers.teamSelection == PTE_ALIENS )
        cs_offset = 1;
    
      if( level.teamVoteTime[ cs_offset ] )
      {
         if( !(ent->client->ps.eFlags & EF_TEAMVOTED ) )
        {
          Cmd_TeamVote_f(ent); 
	  return;
        }
      }
    }
    trap_SendServerCommand( ent-g_entities, "print \"No vote in progress\n\"" );
    return;
  }

  if( ent->client->ps.eFlags & EF_VOTED )
  {
    trap_SendServerCommand( ent-g_entities, "print \"Vote already cast\n\"" );
    return;
  }

  trap_SendServerCommand( ent-g_entities, "print \"Vote cast\n\"" );

  ent->client->ps.eFlags |= EF_VOTED;

  trap_Argv( 1, msg, sizeof( msg ) );

  if( msg[ 0 ] == 'y' || msg[ 1 ] == 'Y' || msg[ 1 ] == '1' )
  {
    level.voteYes++;
    trap_SetConfigstring( CS_VOTE_YES, va( "%i", level.voteYes ) );
  }
  else
  {
    level.voteNo++;
    trap_SetConfigstring( CS_VOTE_NO, va( "%i", level.voteNo ) );
  }

  // a majority will be determined in G_CheckVote, which will also account
  // for players entering or leaving
}

/*
==================
Cmd_CallTeamVote_f
==================
*/
void Cmd_CallTeamVote_f( gentity_t *ent )
{
  int   i, team, cs_offset = 0;
  char  arg1[ MAX_STRING_TOKENS ];
  char  arg2[ MAX_STRING_TOKENS ];
  int   clientNum = -1;
  char  name[ MAX_NETNAME ];
  char  message[ MAX_STRING_CHARS ];
  char nullstring[] = "";
	
  char *arg2plus;
	
  arg2plus = G_SayConcatArgs( 2 );
  
  team = ent->client->pers.teamSelection;

  if( team == PTE_ALIENS )
    cs_offset = 1;

  if( !g_allowVote.integer )
  {
    trap_SendServerCommand( ent-g_entities, "print \"Voting not allowed here\n\"" );
    return;
  }

  if( level.teamVoteTime[ cs_offset ] )
  {
    trap_SendServerCommand( ent-g_entities, "print \"A team vote is already in progress\n\"" );
    return;
  }

  if( g_voteLimit.integer > 0
    && ent->client->pers.voteCount >= g_voteLimit.integer 
    && !G_admin_permission( ent, ADMF_NO_VOTE_LIMIT ) )
  {
    trap_SendServerCommand( ent-g_entities, va(
      "print \"You have already called the maximum number of votes (%d)\n\"",
      g_voteLimit.integer ) );
    return;
  }
  
  if( ent->client->pers.muted )
  {
    trap_SendServerCommand( ent - g_entities,
      "print \"You are muted and cannot call teamvotes\n\"" );
    return;
  }
  
  if( g_voteMinTime.integer
    && ent->client->pers.firstConnect 
    && level.time - ent->client->pers.enterTime < g_voteMinTime.integer * 1000
    && !G_admin_permission( ent, ADMF_NO_VOTE_LIMIT ) 
    && (level.numPlayingClients > 0 && level.numConnectedClients>1) )
  {
    trap_SendServerCommand( ent-g_entities, va(
      "print \"You must wait %d seconds after connecting before calling a vote\n\"",
      g_voteMinTime.integer ) );
    return;
  }

  // make sure it is a valid command to vote on
  trap_Argv( 1, arg1, sizeof( arg1 ) );
  trap_Argv( 2, arg2, sizeof( arg2 ) );

  if( strchr( arg1, ';' ) || strchr( arg2, ';' ) )
  {
    trap_SendServerCommand( ent-g_entities, "print \"Invalid team vote string\n\"" );
    return;
  }

  level.teamVotePercentToPass[ cs_offset ] = 50;
  
  // detect clientNum for partial name match votes
  if( !Q_stricmp( arg1, "kick" ) ||
    !Q_stricmp( arg1, "denybuild" ) ||
    !Q_stricmp( arg1, "allowbuild" ) || 
    !Q_stricmp( arg1, "designate" ) || 
    !Q_stricmp( arg1, "undesignate" ) )
  {
    int clientNums[ MAX_CLIENTS ] = { -1 };
    int numMatches=0;
    char err[ MAX_STRING_CHARS ];

    if( !arg2[ 0 ] )
    {
      trap_SendServerCommand( ent-g_entities,
        "print \"callteamvote: no target\n\"" );
      return;
    }

    numMatches = G_ClientNumbersFromString( arg2, clientNums );
    if( numMatches == 1 )
    {
      // there was only one partial name match
      clientNum = clientNums[ 0 ]; 
    }
    else
    {
      // look for an exact name match (sets clientNum to -1 if it fails) 
      clientNum = G_ClientNumberFromString( ent, arg2plus );
    }
    
    if( clientNum==-1  && numMatches > 1 ) 
    {
      G_MatchOnePlayer( clientNums, err, sizeof( err ) );
      ADMP( va( "^3callteamvote: ^7%s\n", err ) );
      return;
    }

    // make sure this player is on the same team
    if( clientNum != -1 && level.clients[ clientNum ].pers.teamSelection !=
      team )
    {
      clientNum = -1;
    }
      
    if( clientNum != -1 &&
      level.clients[ clientNum ].pers.connected == CON_DISCONNECTED )
    {
      clientNum = -1;
    }

    if( clientNum != -1 )
    {
      Q_strncpyz( name, level.clients[ clientNum ].pers.netname,
        sizeof( name ) );
      Q_CleanStr( name );
      if( G_admin_permission( &g_entities[ clientNum ], ADMF_IMMUNITY ) )
      {
        Com_sprintf( message, sizeof( message ), "%s^7 attempted /callteamvote %s %s on immune admin %s^7",
          ent->client->pers.netname, arg1, arg2, g_entities[ clientNum ].client->pers.netname );
      }
    }
    else
    {
      trap_SendServerCommand( ent-g_entities,
        "print \"callteamvote: invalid player\n\"" );
      return;
    }
  }

  if( !Q_stricmp( arg1, "kick" ) )
  {
    if( G_admin_permission( &g_entities[ clientNum ], ADMF_IMMUNITY ) )
    {
      trap_SendServerCommand( ent-g_entities,
        "print \"callteamvote: admin is immune from vote kick\n\"" );
	G_AdminsPrintf("%s\n",message);
      G_admin_adminlog_log( ent, "teamvote", NULL, 0, qfalse );
      return;
    }


    // use ip in case this player disconnects before the vote ends
    Com_sprintf( level.teamVoteString[ cs_offset ],
      sizeof( level.teamVoteString[ cs_offset ] ),
      "!ban %s %d team vote kick", level.clients[ clientNum ].pers.ip,
      g_adminTempBan.integer + 1 );
    Com_sprintf( level.teamVoteDisplayString[ cs_offset ],
        sizeof( level.teamVoteDisplayString[ cs_offset ] ),
        "Kick player '%s'", name );
  }
  else if( !Q_stricmp( arg1, "denybuild" ) )
  {
    if( level.clients[ clientNum ].pers.denyBuild )
    {
      trap_SendServerCommand( ent-g_entities,
        "print \"callteamvote: player already lost building rights\n\"" );
      return;
    }

    if( G_admin_permission( &g_entities[ clientNum ], ADMF_IMMUNITY ) )
    {
      trap_SendServerCommand( ent-g_entities,
        "print \"callteamvote: admin is immune from denybuild\n\"" );
	G_AdminsPrintf("%s\n",message);
        G_admin_adminlog_log( ent, "teamvote", NULL, 0, qfalse );
      return;
    }

    Com_sprintf( level.teamVoteString[ cs_offset ],
      sizeof( level.teamVoteString[ cs_offset ] ), "!denybuild %i", clientNum );
    Com_sprintf( level.teamVoteDisplayString[ cs_offset ],
        sizeof( level.teamVoteDisplayString[ cs_offset ] ),
        "Take away building rights from '%s'", name );
  }
  else if( !Q_stricmp( arg1, "allowbuild" ) )
  {
    if( !level.clients[ clientNum ].pers.denyBuild )
    {
      trap_SendServerCommand( ent-g_entities,
        "print \"callteamvote: player already has building rights\n\"" );
      return;
    }

    Com_sprintf( level.teamVoteString[ cs_offset ],
      sizeof( level.teamVoteString[ cs_offset ] ), "!allowbuild %i", clientNum );
    Com_sprintf( level.teamVoteDisplayString[ cs_offset ],
        sizeof( level.teamVoteDisplayString[ cs_offset ] ),
        "Allow '%s' to build", name );
  }
  else if( !Q_stricmp( arg1, "designate" ) )
  {
    if( !g_designateVotes.integer )
    {
      trap_SendServerCommand( ent-g_entities,
        "print \"callvote: Designate votes have been disabled.\n\"" );
      return;
    }
	  
    if( level.clients[ clientNum ].pers.designatedBuilder )
    {
      trap_SendServerCommand( ent-g_entities,
        "print \"callvote: player is already a designated builder\n\"" );
      return;
    }
    Com_sprintf( level.teamVoteString[ cs_offset ],
      sizeof( level.teamVoteString[ cs_offset ] ), "!designate %i", clientNum );
    Com_sprintf( level.teamVoteDisplayString[ cs_offset ],
        sizeof( level.teamVoteDisplayString[ cs_offset ] ),
        "Make '%s' a designated builder", name );
  }
  else if( !Q_stricmp( arg1, "undesignate" ) )
  {
	  
    if( !g_designateVotes.integer )
    {
      trap_SendServerCommand( ent-g_entities,
        "print \"callvote: Designate votes have been disabled.\n\"" );
      return;
    }
	  
    if( !level.clients[ clientNum ].pers.designatedBuilder )
    {
      trap_SendServerCommand( ent-g_entities,
        "print \"callvote: player is not currently a designated builder\n\"" );
      return;
    }
    Com_sprintf( level.teamVoteString[ cs_offset ],
      sizeof( level.teamVoteString[ cs_offset ] ), "!undesignate %i", clientNum );
    Com_sprintf( level.teamVoteDisplayString[ cs_offset ],
        sizeof( level.teamVoteDisplayString[ cs_offset ] ),
        "Remove designated builder status from '%s'", name );
  }
  else if( !Q_stricmp( arg1, "admitdefeat" ) )
  {
    if( !g_admitDefeatVotePercent.integer )
    {
      trap_SendServerCommand( ent-g_entities, "print \"admitdefeat votes are disabled\n\"" );
      return;
    }
    if( g_admitDefeatTime.integer > 0 &&
        level.time - level.startTime < g_admitDefeatTime.integer * 1000 )
    {
       trap_SendServerCommand( ent-g_entities,
         va( "print \"Cannot call an admitdefeat vote before %d:%02d\n\"",
         g_admitDefeatTime.integer / 60, g_admitDefeatTime.integer % 60 ) );
       return;
    }
    level.teamVotePercentToPass[ cs_offset ] = g_admitDefeatVotePercent.integer;
    Com_sprintf( level.teamVoteString[ cs_offset ],
      sizeof( level.teamVoteString[ cs_offset ] ), "admitdefeat %i", team );
    Com_sprintf( level.teamVoteDisplayString[ cs_offset ],
        sizeof( level.teamVoteDisplayString[ cs_offset ] ),
        "Admit Defeat" );
  }
   else if( !Q_stricmp( arg1, "poll" ) )
   {
     Com_sprintf( level.teamVoteString[ cs_offset ], sizeof( level.teamVoteString[ cs_offset ] ), nullstring );
     Com_sprintf( level.teamVoteDisplayString[ cs_offset ],
         sizeof( level.voteDisplayString ), "[Poll] \'%s\'", arg2plus );
   }
  else
  {
    trap_SendServerCommand( ent-g_entities, "print \"Invalid vote string\n\"" );
    trap_SendServerCommand( ent-g_entities,
       "print \"Valid team vote commands are: "
       "kick, denybuild, allowbuild, poll, designate, undesignate, and admitdefeat\n\"" );
    return;
  }
  ent->client->pers.voteCount++;

  G_admin_adminlog_log( ent, "teamvote", arg1, 0, qtrue );

  if( level.teamVotePercentToPass[ cs_offset ] != 50 )
  {
    Q_strcat( level.teamVoteDisplayString[ cs_offset ], sizeof( level.teamVoteDisplayString[ cs_offset ] ),
      va( " (Needs %d percent)", level.teamVotePercentToPass[ cs_offset ] ) );
  }

  for( i = 0 ; i < level.maxclients ; i++ )
  {
    if( level.clients[ i ].pers.connected == CON_DISCONNECTED )
      continue;

    if( level.clients[ i ].ps.stats[ STAT_PTEAM ] == team )
	{
      trap_SendServerCommand( i, va("print \"%s " S_COLOR_WHITE
            "called a team vote: %s \n\"", ent->client->pers.netname, level.teamVoteDisplayString[ cs_offset ] ) );
	  trap_SendServerCommand( i, "cp \"A teamvote has been called\n^2F1: Yes^7, ^1F2: No^7\"" );
	}

  }

  // start the voting
  level.teamVoteTime[ cs_offset ] = level.time;
  level.teamVoteYes[ cs_offset ] = 0;
  level.teamVoteNo[ cs_offset ] = 0;

  for( i = 0 ; i < level.maxclients ; i++ )
  {
    if( level.clients[ i ].ps.stats[ STAT_PTEAM ] == team )
      level.clients[ i ].ps.eFlags &= ~EF_TEAMVOTED;
  }

  // the caller always votes yes, except for polls
  if( Q_stricmp( arg1, "poll" ) )
  {
    level.teamVoteYes[ cs_offset ]++;
    ent->client->ps.eFlags |= EF_TEAMVOTED;
  }

  trap_SetConfigstring( CS_TEAMVOTE_TIME + cs_offset, va( "%i", level.teamVoteTime[ cs_offset ] ) );
  trap_SetConfigstring( CS_TEAMVOTE_STRING + cs_offset, level.teamVoteDisplayString[ cs_offset ] );
  trap_SetConfigstring( CS_TEAMVOTE_YES + cs_offset, va( "%i", level.teamVoteYes[ cs_offset ] ) );
  trap_SetConfigstring( CS_TEAMVOTE_NO + cs_offset, va( "%i", level.teamVoteNo[ cs_offset ] ) );
}


/*
==================
Cmd_TeamVote_f
==================
*/
void Cmd_TeamVote_f( gentity_t *ent )
{
  int     cs_offset = 0;
  char    msg[ 64 ];

  if( ent->client->pers.teamSelection == PTE_ALIENS )
    cs_offset = 1;

  if( !level.teamVoteTime[ cs_offset ] )
  {
    trap_SendServerCommand( ent-g_entities, "print \"No team vote in progress\n\"" );
    return;
  }

  if( ent->client->ps.eFlags & EF_TEAMVOTED )
  {
    trap_SendServerCommand( ent-g_entities, "print \"Team vote already cast\n\"" );
    return;
  }

  trap_SendServerCommand( ent-g_entities, "print \"Team vote cast\n\"" );

  ent->client->ps.eFlags |= EF_TEAMVOTED;

  trap_Argv( 1, msg, sizeof( msg ) );

  if( msg[ 0 ] == 'y' || msg[ 1 ] == 'Y' || msg[ 1 ] == '1' )
  {
    level.teamVoteYes[ cs_offset ]++;
    trap_SetConfigstring( CS_TEAMVOTE_YES + cs_offset, va( "%i", level.teamVoteYes[ cs_offset ] ) );
  }
  else
  {
    level.teamVoteNo[ cs_offset ]++;
    trap_SetConfigstring( CS_TEAMVOTE_NO + cs_offset, va( "%i", level.teamVoteNo[ cs_offset ] ) );
  }

  // a majority will be determined in TeamCheckVote, which will also account
  // for players entering or leaving
}


/*
=================
Cmd_SetViewpos_f
=================
*/
void Cmd_SetViewpos_f( gentity_t *ent )
{
  vec3_t  origin, angles;
  char    buffer[ MAX_TOKEN_CHARS ];
  int     i;

  if( trap_Argc( ) != 5 )
  {
    trap_SendServerCommand( ent-g_entities, va( "print \"usage: setviewpos x y z yaw\n\"" ) );
    return;
  }

  VectorClear( angles );

  for( i = 0 ; i < 3 ; i++ )
  {
    trap_Argv( i + 1, buffer, sizeof( buffer ) );
    origin[ i ] = atof( buffer );
  }

  trap_Argv( 4, buffer, sizeof( buffer ) );
  angles[ YAW ] = atof( buffer );

  TeleportPlayer( ent, origin, angles );
}

#define AS_OVER_RT3         ((ALIENSENSE_RANGE*0.5f)/M_ROOT3)

qboolean G_RoomForClassChange( gentity_t *ent, pClass_t class, vec3_t newOrigin )
{
  vec3_t    fromMins, fromMaxs;
  vec3_t    toMins, toMaxs;
  vec3_t    temp;
  trace_t   tr;
  float     nudgeHeight;
  float     maxHorizGrowth;
  pClass_t  oldClass = ent->client->ps.stats[ STAT_PCLASS ];

  BG_FindBBoxForClass( oldClass, fromMins, fromMaxs, NULL, NULL, NULL );
  BG_FindBBoxForClass( class, toMins, toMaxs, NULL, NULL, NULL );

  VectorCopy( ent->s.origin, newOrigin );

  // find max x/y diff
  maxHorizGrowth = toMaxs[ 0 ] - fromMaxs[ 0 ];
  if( toMaxs[ 1 ] - fromMaxs[ 1 ] > maxHorizGrowth )
    maxHorizGrowth = toMaxs[ 1 ] - fromMaxs[ 1 ];
  if( toMins[ 0 ] - fromMins[ 0 ] > -maxHorizGrowth )
    maxHorizGrowth = -( toMins[ 0 ] - fromMins[ 0 ] );
  if( toMins[ 1 ] - fromMins[ 1 ] > -maxHorizGrowth )
    maxHorizGrowth = -( toMins[ 1 ] - fromMins[ 1 ] );

  if( maxHorizGrowth > 0.0f )
  {
    // test by moving the player up the max required on a 60 degree slope
    nudgeHeight = maxHorizGrowth * 2.0f;
  }
  else
  {
    // player is shrinking, so there's no need to nudge them upwards
    nudgeHeight = 0.0f;
  }

  // find what the new origin would be on a level surface
  newOrigin[ 2 ] += fabs( toMins[ 2 ] ) - fabs( fromMins[ 2 ] );

  //compute a place up in the air to start the real trace
  VectorCopy( newOrigin, temp );
  temp[ 2 ] += nudgeHeight;
  trap_Trace( &tr, newOrigin, toMins, toMaxs, temp, ent->s.number, MASK_SHOT );

  //trace down to the ground so that we can evolve on slopes
  VectorCopy( newOrigin, temp );
  temp[ 2 ] += ( nudgeHeight * tr.fraction );
  trap_Trace( &tr, temp, toMins, toMaxs, newOrigin, ent->s.number, MASK_SHOT );
  VectorCopy( tr.endpos, newOrigin );

  //make REALLY sure
  trap_Trace( &tr, newOrigin, toMins, toMaxs, newOrigin,
    ent->s.number, MASK_SHOT );

  //check there is room to evolve
  if( !tr.startsolid && tr.fraction == 1.0f )
    return qtrue;
  else
    return qfalse;
}

/*
=================
Cmd_Class_f
=================
*/
void Cmd_Class_f( gentity_t *ent )
{
  char      s[ MAX_TOKEN_CHARS ];
  int       clientNum;
  int       i;
  vec3_t    infestOrigin;
  int       allowedClasses[ PCL_NUM_CLASSES ];
  int       numClasses = 0;
  pClass_t  currentClass = ent->client->ps.stats[ STAT_PCLASS ];
  pClass_t  newClass;
  int       numLevels;
  int       entityList[ MAX_GENTITIES ];
  vec3_t    range = { AS_OVER_RT3, AS_OVER_RT3, AS_OVER_RT3 };
  vec3_t    mins, maxs;
  int       num;
  gentity_t *other;
  qboolean  humanNear = qfalse;

  if( ent->client->ps.stats[ STAT_HEALTH ] <= 0 )
    return;

  clientNum = ent->client - level.clients;
  trap_Argv( 1, s, sizeof( s ) );

  if( BG_ClassIsAllowed( PCL_ALIEN_BUILDER0 ) )
    allowedClasses[ numClasses++ ] = PCL_ALIEN_BUILDER0;

  if( BG_ClassIsAllowed( PCL_ALIEN_BUILDER0_UPG ) &&
      BG_FindStagesForClass( PCL_ALIEN_BUILDER0_UPG, g_alienStage.integer ) )
    allowedClasses[ numClasses++ ] = PCL_ALIEN_BUILDER0_UPG;

  if( BG_ClassIsAllowed( PCL_ALIEN_LEVEL0 ) )
    allowedClasses[ numClasses++ ] = PCL_ALIEN_LEVEL0;

  if( ent->client->pers.teamSelection == PTE_ALIENS &&
      !( ent->client->ps.stats[ STAT_STATE ] & SS_INFESTING ) &&
      !( ent->client->ps.stats[ STAT_STATE ] & SS_HOVELING ) )
  {
    newClass = BG_FindClassNumForName( s );
    if( newClass == PCL_NONE )
    {
      trap_SendServerCommand( ent-g_entities, va( "print \"Unknown class\n\"" ) );
      return;
    }

    //if we are not currently spectating, we are attempting evolution
    if( ent->client->pers.classSelection != PCL_NONE )
    {

      if( !level.overmindPresent )
      {
        G_TriggerMenu( clientNum, MN_A_NOOVMND_EVOLVE );
        return;
      }
      
      //check there are no humans nearby
      VectorAdd( ent->client->ps.origin, range, maxs );
      VectorSubtract( ent->client->ps.origin, range, mins );
      
      num = trap_EntitiesInBox( mins, maxs, entityList, MAX_GENTITIES );
      for( i = 0; i < num; i++ )
      {
        other = &g_entities[ entityList[ i ] ];

        if( ( other->client && other->client->ps.stats[ STAT_PTEAM ] == PTE_HUMANS ) ||
            ( other->s.eType == ET_BUILDABLE && other->biteam == BIT_HUMANS ) )
        {
          humanNear = qtrue;
        }
        //If its the OM, then ignore all humans.
        if(other->s.eType == ET_BUILDABLE && other->s.modelindex == BA_A_OVERMIND)
        {
          humanNear = qfalse;
          break;
        }
      }
      
      if(humanNear == qtrue)
      {
        G_TriggerMenu( clientNum, MN_A_TOOCLOSE );
        return;
      }
      
      if( ( ent->client->ps.stats[ STAT_STATE ] & SS_WALLCLIMBING ) ||
          ( ent->client->ps.stats[ STAT_STATE ] & SS_WALLCLIMBINGCEILING ) )
      {
        trap_SendServerCommand( ent-g_entities, va( "print \"You cannot evolve while wallwalking\n\"" ) );
        return;
      }

      // denyweapons
      if( newClass >= PCL_ALIEN_LEVEL1 && newClass <= PCL_ALIEN_LEVEL4 &&
        ent->client->pers.denyAlienClasses & ( 1 << newClass ) )
      {
        trap_SendServerCommand( ent-g_entities, va( "print \"You are denied from using this class\n\"" ) );
        return;
      }

      //guard against selling the HBUILD weapons exploit
      if( ( currentClass == PCL_ALIEN_BUILDER0 ||
            currentClass == PCL_ALIEN_BUILDER0_UPG ) &&
          ent->client->ps.stats[ STAT_MISC ] > 0 )
      {
        trap_SendServerCommand( ent-g_entities,
            va( "print \"You cannot evolve until build timer expires\n\"" ) );
        return;
      }

      numLevels = BG_ClassCanEvolveFromTo( currentClass,
                                           newClass,
                                           (short)ent->client->ps.persistant[ PERS_CREDIT ], 0 );

      if( G_RoomForClassChange( ent, newClass, infestOrigin ) )
      {
        //...check we can evolve to that class
        if( numLevels >= 0 &&
            BG_FindStagesForClass( newClass, g_alienStage.integer ) &&
            BG_ClassIsAllowed( newClass ) )
        {
          G_LogOnlyPrintf("ClientTeamClass: %i alien %s\n", clientNum, s);

          ent->client->pers.evolveHealthFraction = (float)ent->client->ps.stats[ STAT_HEALTH ] /
            (float)BG_FindHealthForClass( currentClass );

          if( ent->client->pers.evolveHealthFraction < 0.0f )
            ent->client->pers.evolveHealthFraction = 0.0f;
          else if( ent->client->pers.evolveHealthFraction > 1.0f )
            ent->client->pers.evolveHealthFraction = 1.0f;

          //remove credit
          G_AddCreditToClient( ent->client, -(short)numLevels, qtrue );
          ent->client->pers.classSelection = newClass;
          ClientUserinfoChanged( clientNum );
          VectorCopy( infestOrigin, ent->s.pos.trBase );
          ClientSpawn( ent, ent, ent->s.pos.trBase, ent->s.apos.trBase );
          return;
        }
        else
        {
          trap_SendServerCommand( ent-g_entities,
               va( "print \"You cannot evolve from your current class\n\"" ) );
          return;
        }
      }
      else
      {
        G_TriggerMenu( clientNum, MN_A_NOEROOM );
        return;
      }
    }
    else
    {
      //spawning from an egg
      for( i = 0; i < numClasses; i++ )
      {
        if( allowedClasses[ i ] == newClass &&
            BG_FindStagesForClass( newClass, g_alienStage.integer ) &&
            BG_ClassIsAllowed( newClass ) )
        {
          if( ent->client->pers.denyBuild && ( newClass==PCL_ALIEN_BUILDER0 || newClass==PCL_ALIEN_BUILDER0_UPG ) )
          {
            trap_SendServerCommand( ent-g_entities, "print \"Your building rights have been revoked\n\"" );
            return;
          }	
		
          G_LogOnlyPrintf("ClientTeamClass: %i alien %s\n", clientNum, s);

          ent->client->pers.classSelection =
            ent->client->ps.stats[ STAT_PCLASS ] = newClass;
          G_PushSpawnQueue( &level.alienSpawnQueue, clientNum );
          return;
        }
      }
      trap_SendServerCommand( ent-g_entities, va( "print \"You cannot spawn as this class\n\"" ) );
      return;
    }
  }
  else if( ent->client->pers.teamSelection == PTE_HUMANS )
  {
    //humans cannot use this command whilst alive
    if( ent->client->pers.classSelection != PCL_NONE )
    {
      trap_SendServerCommand( ent-g_entities, va( "print \"You must be dead to use the class command\n\"" ) );
      return;
    }

    ent->client->pers.classSelection =
      ent->client->ps.stats[ STAT_PCLASS ] = PCL_HUMAN;

    //set the item to spawn with
    if( !Q_stricmp( s, BG_FindNameForWeapon( WP_MACHINEGUN ) ) && BG_WeaponIsAllowed( WP_MACHINEGUN ) )
      ent->client->pers.humanItemSelection = WP_MACHINEGUN;
    else if( !Q_stricmp( s, BG_FindNameForWeapon( WP_HBUILD ) ) && BG_WeaponIsAllowed( WP_HBUILD ) )
      ent->client->pers.humanItemSelection = WP_HBUILD;
    else if( !Q_stricmp( s, BG_FindNameForWeapon( WP_HBUILD2 ) ) && BG_WeaponIsAllowed( WP_HBUILD2 ) &&
        BG_FindStagesForWeapon( WP_HBUILD2, g_humanStage.integer ) )
      ent->client->pers.humanItemSelection = WP_HBUILD2;
    else
    {
      ent->client->pers.classSelection = PCL_NONE;
      trap_SendServerCommand( ent-g_entities, va( "print \"Unknown starting item\n\"" ) );
      return;
    }

    G_LogOnlyPrintf("ClientTeamClass: %i human %s\n", clientNum, s);

    G_PushSpawnQueue( &level.humanSpawnQueue, clientNum );
  }
}

/*
=================
Cmd_Class_f
=================
*/
void Cmd_Devolve_f( gentity_t *ent )
{
  int       clientNum;
  int       i;
  vec3_t    infestOrigin;
  int       allowedClasses[ PCL_NUM_CLASSES ];
  int       numClasses = 0;
  pClass_t  currentClass = ent->client->ps.stats[ STAT_PCLASS ];
  pClass_t  newClass;
  int       numLevels;
  int       entityList[ MAX_GENTITIES ];
  vec3_t    range = { AS_OVER_RT3, AS_OVER_RT3, AS_OVER_RT3 };
  vec3_t    mins, maxs;
  int       num;
  gentity_t *other;
  qboolean  nearOM = qfalse;

  if( ent->client->ps.stats[ STAT_HEALTH ] <= 0 )
    return;
  
  if( !g_allowDevolve.integer )
  {
  	trap_SendServerCommand( ent-g_entities, "print \"Devolve has been disabled\n\"" );
  	return;
  }

  clientNum = ent->client - level.clients;

  if( BG_ClassIsAllowed( PCL_ALIEN_BUILDER0 ) )
    allowedClasses[ numClasses++ ] = PCL_ALIEN_BUILDER0;

  if( ent->client->pers.teamSelection == PTE_ALIENS &&
      !( ent->client->ps.stats[ STAT_STATE ] & SS_INFESTING ) &&
      !( ent->client->ps.stats[ STAT_STATE ] & SS_HOVELING ) )
  {
      newClass = PCL_ALIEN_BUILDER0;

      if( !level.overmindPresent )
      {
        G_TriggerMenu( clientNum, MN_A_NOOVMND_EVOLVE );
        return;
      }
      
      // Check if near OM
      VectorAdd( ent->client->ps.origin, range, maxs );
      VectorSubtract( ent->client->ps.origin, range, mins );
      
      num = trap_EntitiesInBox( mins, maxs, entityList, MAX_GENTITIES );
      for( i = 0; i < num; i++ )
      {
        other = &g_entities[ entityList[ i ] ];

        if(other->s.eType == ET_BUILDABLE && other->s.modelindex == BA_A_OVERMIND)
        {
          nearOM = qtrue;
          break;
        }
      }
      
      //Don't devolve if already a granger
      if( !nearOM )
      {
        trap_SendServerCommand( ent-g_entities,
            va( "print \"Must be near the overmind to devolve\n\"" ) );
        return;
      }
      
      if( ( ent->client->ps.stats[ STAT_STATE ] & SS_WALLCLIMBING ) ||
          ( ent->client->ps.stats[ STAT_STATE ] & SS_WALLCLIMBINGCEILING ) )
      {
        trap_SendServerCommand( ent-g_entities, va( "print \"You cannot devolve while wallwalking\n\"" ) );
        return;
      }

      //Don't devolve if already a granger
      if( currentClass == PCL_ALIEN_BUILDER0 || currentClass == PCL_ALIEN_BUILDER0_UPG )
      {
        trap_SendServerCommand( ent-g_entities,
            va( "print \"You are already a granger!\n\"" ) );
        return;
      }
      
      //Don't devolve if not full health
      if( ent->client->ps.stats[ STAT_HEALTH ] != BG_FindHealthForClass( currentClass ) )
      {
        trap_SendServerCommand( ent-g_entities,
            va( "print \"You have to be at full health to devolve\n\"" ) );
        return;
      }

      if( G_RoomForClassChange( ent, newClass, infestOrigin ) )
      {
        
          G_LogOnlyPrintf("ClientTeamClass: %i alien builder\n", clientNum);

          ent->client->pers.evolveHealthFraction = (float)ent->client->ps.stats[ STAT_HEALTH ] /
            (float)BG_FindHealthForClass( currentClass );

          if( ent->client->pers.evolveHealthFraction < 0.0f )
            ent->client->pers.evolveHealthFraction = 0.0f;
          else if( ent->client->pers.evolveHealthFraction > 1.0f )
            ent->client->pers.evolveHealthFraction = 1.0f;

          //remove credit
          ent->client->pers.classSelection = newClass;
          ClientUserinfoChanged( clientNum );
          VectorCopy( infestOrigin, ent->s.pos.trBase );
          ClientSpawn( ent, ent, ent->s.pos.trBase, ent->s.apos.trBase );
          return;
      }
   }
}

/*
=================
DBCommand

Send command to all designated builders of selected team
=================
*/
void DBCommand( gentity_t *builder, pTeam_t team, const char *text )
{
  int i;
  gentity_t *ent;

  if( g_floodMinTime.integer &&
    G_Flood_Limited( builder ) )
  {
    trap_SendServerCommand( builder-g_entities,
      "print \"Your deconstruct attempt is flood-limited; wait before trying again\n\"" );
    return;
  }

  trap_SendServerCommand( builder-g_entities, 
    "print \"This structure is protected by designated builder\n\"" );

  for( i = 0, ent = g_entities + i; i < level.maxclients; i++, ent++ )
  {
    if( !ent->client || ent->client->pers.connected != CON_CONNECTED )
      continue;

    if( ( ent->client->pers.teamSelection == team &&
      ent->client->pers.designatedBuilder ) ||
      ( ent->client->pers.teamSelection == PTE_NONE &&
      G_admin_permission( ent, ADMF_SPEC_ALLCHAT ) ) )
    {
      trap_SendServerCommand( i, text );
    }
  }
}

/*
=================
Cmd_Destroy_f
=================
*/
void Cmd_Destroy_f( gentity_t *ent )
{
  vec3_t      forward, end;
  trace_t     tr;
  gentity_t   *traceEnt;
  char        cmd[ 12 ];
  qboolean    deconstruct = qtrue;

  if( ent->client->pers.paused )
  {
    trap_SendServerCommand( ent-g_entities,
      "print \"You may not deconstruct while paused\n\"" );
    return;
  }


  if( ent->client->pers.denyBuild )
  {
    trap_SendServerCommand( ent-g_entities,
      "print \"Your building rights have been revoked\n\"" );
    return;
  }

  if( g_newbieNoBuild.integer
    && g_newbieNamePrefix.string[ 0 ]
    && Q_stricmpn ( ent->client->pers.netname, g_newbieNamePrefix.string, strlen(g_newbieNamePrefix.string ) ) == 0 )
  {
    trap_SendServerCommand( ent-g_entities,
      va( "print \"You cannot build until you set a name\n%s%s\"",
      ( g_newbieNoBuildMessage.string[ 0 ] ) ? g_newbieNoBuildMessage.string : "",
      ( g_newbieNoBuildMessage.string[ 0 ] ) ? "\n" : "" ) );
    return;
  }

  trap_Argv( 0, cmd, sizeof( cmd ) );
  if( Q_stricmp( cmd, "destroy" ) == 0 )
    deconstruct = qfalse;

  if( ent->client->ps.stats[ STAT_STATE ] & SS_HOVELING )
  {
    if( ( ent->client->hovel->s.eFlags & EF_DBUILDER ) &&
      !ent->client->pers.designatedBuilder )
    {
      DBCommand( ent, ent->client->pers.teamSelection,
        va( "print \"%s^3 has attempted to decon a protected %s!\n\"",
	ent->client->pers.netname,
        BG_FindHumanNameForBuildable( ent->client->hovel->s.modelindex ) ) );
      return;
    }
    G_Damage( ent->client->hovel, ent, ent, forward, ent->s.origin, 
      10000, 0, MOD_SUICIDE );
  }

  if( !( ent->client->ps.stats[ STAT_STATE ] & SS_INFESTING ) )
  {
    AngleVectors( ent->client->ps.viewangles, forward, NULL, NULL );
    VectorMA( ent->client->ps.origin, 100, forward, end );

    trap_Trace( &tr, ent->client->ps.origin, NULL, NULL, end, ent->s.number, MASK_PLAYERSOLID );
    traceEnt = &g_entities[ tr.entityNum ];

    if( tr.fraction < 1.0f &&
        ( traceEnt->s.eType == ET_BUILDABLE ) &&
        ( traceEnt->biteam == ent->client->pers.teamSelection ) &&
        ( ( ent->client->ps.weapon >= WP_ABUILD ) &&
          ( ent->client->ps.weapon <= WP_HBUILD ) ) )
    {
      // Cancel deconstruction
      if( g_markDeconstruct.integer == 1 && traceEnt->deconstruct )
      {
        traceEnt->deconstruct = qfalse;
        return;
      }
      if( ( traceEnt->s.eFlags & EF_DBUILDER ) &&
        !ent->client->pers.designatedBuilder )
      {
        DBCommand( ent, ent->client->pers.teamSelection,
          va( "print \"%s^3 has attempted to decon a protected %s!\n\"",
	  ent->client->pers.netname,
          BG_FindHumanNameForBuildable( traceEnt->s.modelindex ) ) );
        return;
      }

      // Prevent destruction of critical buildables (per g_deconTime)
      if( g_deconTime.integer &&
        ( traceEnt->s.modelindex == BA_A_OVERMIND || traceEnt->s.modelindex == BA_H_REACTOR ) )
      {
        if( g_deconTime.integer > 0 &&
          level.time - level.startTime < g_deconTime.integer * 1000 )
        {
          trap_SendServerCommand( ent-g_entities,
            va( "print \"%s can't be moved until %d:%02d into the game\n\"",
            BG_FindHumanNameForBuildable( traceEnt->s.modelindex ),
            g_deconTime.integer / 60, g_deconTime.integer % 60 ) );
          return;
        }
        if( g_deconTime.integer  < 0 &&
          level.time - level.startTime < 60 * 1000 && level.numConnectedClients )
        {
          int numTeammates;

          if( ent->client->pers.teamSelection == PTE_HUMANS )
            numTeammates = level.numHumanClients;
          else
            numTeammates = level.numAlienClients;

          if( numTeammates * 100 / level.numConnectedClients < 33 )
          {
            trap_SendServerCommand( ent-g_entities,
              va( "print \"%s can't be moved until more players join, or after 1:00\n\"",
              BG_FindHumanNameForBuildable( traceEnt->s.modelindex ) ) );
            return;
          }
        }
      }

      // Prevent destruction of the last spawn
      if( g_markDeconstruct.integer != 1 )
      {
        if( ent->client->pers.teamSelection == PTE_ALIENS &&
            traceEnt->s.modelindex == BA_A_SPAWN )
        {
          if( level.numAlienSpawns <= 1 )
            return;
        }
        else if( ent->client->pers.teamSelection == PTE_HUMANS &&
                 traceEnt->s.modelindex == BA_H_SPAWN )
        {
          if( level.numHumanSpawns <= 1 )
            return;
        }
      }

      // Don't allow destruction of hovel with granger inside
      if( traceEnt->s.modelindex == BA_A_HOVEL && traceEnt->active )
        return;

      // Don't allow destruction of buildables that cannot be rebuilt
      if(g_suddenDeath.integer && traceEnt->health > 0 &&
          ( ( g_suddenDeathMode.integer == SDMODE_SELECTIVE &&
              !BG_FindReplaceableTestForBuildable( traceEnt->s.modelindex ) ) ||
            ( g_suddenDeathMode.integer == SDMODE_BP &&
              BG_FindBuildPointsForBuildable( traceEnt->s.modelindex ) ) ||
            g_suddenDeathMode.integer == SDMODE_NO_BUILD ) )
      {
        trap_SendServerCommand( ent-g_entities,
          "print \"During Sudden Death you can only decon buildings that "
          "can be rebuilt\n\"" );
        return;
      }

      if( ent->client->ps.stats[ STAT_MISC ] > 0 )
      {
        G_AddEvent( ent, EV_BUILD_DELAY, ent->client->ps.clientNum );
        return;
      }

      if( traceEnt->health > 0 || g_deconDead.integer )
      {
        if( g_markDeconstruct.integer == 1 )
        {
          traceEnt->deconstruct     = qtrue; // Mark buildable for deconstruction
          traceEnt->deconstructTime = level.time;
        }
        else
        {
          if( traceEnt->health > 0 )
	  {
	  
          buildHistory_t *new;

          new = G_Alloc( sizeof( buildHistory_t ) );
          new->ID = ( ++level.lastBuildID > 1000 ) 
              ? ( level.lastBuildID = 1 ) : level.lastBuildID;
          new->ent = ent;
          new->name[ 0 ] = 0;
          new->buildable = traceEnt->s.modelindex;
          VectorCopy( traceEnt->s.pos.trBase, new->origin );
          VectorCopy( traceEnt->s.angles, new->angles );
          VectorCopy( traceEnt->s.origin2, new->origin2 );
          VectorCopy( traceEnt->s.angles2, new->angles2 );
          new->fate = BF_DECONNED;
          new->next = NULL;
          new->marked = NULL;
          G_LogBuild( new );
	  
	  if(traceEnt->s.modelindex == BA_H_MEDISTAT )
          traceEnt->oldent->client->pers.healing = qfalse;

           G_TeamCommand( ent->client->pers.teamSelection,
             va( "print \"%s ^3DECONSTRUCTED^7 by %s^7\n\"",
               BG_FindHumanNameForBuildable( traceEnt->s.modelindex ), 
               ent->client->pers.netname ) );

           G_LogPrintf( "Decon: %i %i 0: %s^7 deconstructed %s\n",
             ent->client->ps.clientNum,
             traceEnt->s.modelindex,
             ent->client->pers.netname, 
             BG_FindNameForBuildable( traceEnt->s.modelindex ) );
	  }

          if( !deconstruct )
            G_Damage( traceEnt, ent, ent, forward, tr.endpos, 10000, 0, MOD_SUICIDE );
          else
            G_FreeEntity( traceEnt );
	  if( !g_instantBuild.integer ){
          if( !g_cheats.integer ){
            ent->client->ps.stats[ STAT_MISC ] +=
              BG_FindBuildDelayForWeapon( ent->s.weapon ) >> 2;}}
	    
        }
      }
    }
  }
}

void Cmd_Mark_f( gentity_t *ent )
{
  vec3_t      forward, end;
  trace_t     tr;
  gentity_t   *traceEnt;

  if( g_markDeconstruct.integer != 2 )
  {
    trap_SendServerCommand( ent-g_entities,
      "print \"Mark is disabled\n\"" );
    return;
  }

  if( ent->client->pers.paused )
  {
    trap_SendServerCommand( ent-g_entities,
      "print \"You may not mark while paused\n\"" );
    return;
  }

  if( ent->client->pers.denyBuild )
  {
    trap_SendServerCommand( ent-g_entities,
      "print \"Your building rights have been revoked\n\"" );
    return;
  }

  if( g_newbieNoBuild.integer
    && g_newbieNamePrefix.string[ 0 ]
    && Q_stricmpn ( ent->client->pers.netname, g_newbieNamePrefix.string, strlen(g_newbieNamePrefix.string ) ) == 0 )
  {
    trap_SendServerCommand( ent-g_entities,
      va( "print \"You cannot build until you set a name\n%s%s\"",
      ( g_newbieNoBuildMessage.string[ 0 ] ) ? g_newbieNoBuildMessage.string : "",
      ( g_newbieNoBuildMessage.string[ 0 ] ) ? "\n" : "" ) );
    return;
  }

  // can't mark when in hovel
  if( ent->client->ps.stats[ STAT_STATE ] & SS_HOVELING )
    return;

  if( !( ent->client->ps.stats[ STAT_STATE ] & SS_INFESTING ) )
  {
    AngleVectors( ent->client->ps.viewangles, forward, NULL, NULL );
    VectorMA( ent->client->ps.origin, 100, forward, end );

    trap_Trace( &tr, ent->client->ps.origin, NULL, NULL, end, ent->s.number, MASK_PLAYERSOLID );
    traceEnt = &g_entities[ tr.entityNum ];

    if( tr.fraction < 1.0f &&
        ( traceEnt->s.eType == ET_BUILDABLE ) &&
        ( traceEnt->biteam == ent->client->pers.teamSelection ) &&
        ( ( ent->client->ps.weapon >= WP_ABUILD ) &&
          ( ent->client->ps.weapon <= WP_HBUILD ) ) )
    {
      if( ( traceEnt->s.eFlags & EF_DBUILDER ) &&
        !ent->client->pers.designatedBuilder )
      {
        trap_SendServerCommand( ent-g_entities,
          "print \"this structure is protected by a designated builder\n\"" );
        return;
      }

      // Cancel deconstruction
      if( traceEnt->deconstruct )
      {
        traceEnt->deconstruct = qfalse;

        trap_SendServerCommand( ent-g_entities,
          va( "print \"%s no longer marked for deconstruction\n\"",
          BG_FindHumanNameForBuildable( traceEnt->s.modelindex ) ) );
        return;
      }

      // Prevent marking of critical buildables (per g_deconTime)
      if( g_deconTime.integer &&
        ( traceEnt->s.modelindex == BA_A_OVERMIND || traceEnt->s.modelindex == BA_H_REACTOR ) )
      {
        if( g_deconTime.integer > 0 &&
            level.time - level.startTime < g_deconTime.integer * 1000 )
        {
          trap_SendServerCommand( ent-g_entities,
            va( "print \"%s can't be moved until %d:%02d into the game\n\"",
            BG_FindHumanNameForBuildable( traceEnt->s.modelindex ),
            g_deconTime.integer / 60, g_deconTime.integer % 60 ) );
          return;
        }
        if( g_deconTime.integer  < 0 &&
          level.time - level.startTime < 60 * 1000 && level.numConnectedClients )
        {
          int numTeammates;

          if( ent->client->pers.teamSelection == PTE_HUMANS )
            numTeammates = level.numHumanClients;
          else
            numTeammates = level.numAlienClients;

          if( numTeammates * 100 / level.numConnectedClients < 33 )
          {
            trap_SendServerCommand( ent-g_entities,
              va( "print \"%s can't be moved until more players join, or after 1:00\n\"",
              BG_FindHumanNameForBuildable( traceEnt->s.modelindex ) ) );
            return;
          }
        }
      }

      // Don't allow marking of buildables that cannot be rebuilt
      if(g_suddenDeath.integer && traceEnt->health > 0 &&
          ( ( g_suddenDeathMode.integer == SDMODE_SELECTIVE &&
              !BG_FindReplaceableTestForBuildable( traceEnt->s.modelindex ) ) ||
            ( g_suddenDeathMode.integer == SDMODE_BP &&
              BG_FindBuildPointsForBuildable( traceEnt->s.modelindex ) ) ||
            g_suddenDeathMode.integer == SDMODE_NO_BUILD ) )
      {
        trap_SendServerCommand( ent-g_entities,
          "print \"During Sudden Death you can only mark buildings that "
          "can be rebuilt\n\"" );
        return;
      }

      if( traceEnt->health > 0 )
      {
        traceEnt->deconstruct     = qtrue; // Mark buildable for deconstruction
        traceEnt->deconstructTime = level.time;

        trap_SendServerCommand( ent-g_entities,
          va( "print \"%s marked for deconstruction\n\"",
          BG_FindHumanNameForBuildable( traceEnt->s.modelindex ) ) );
      }
    }
  }
}


/*
=================
Cmd_ActivateItem_f

Activate an item
=================
*/
void Cmd_ActivateItem_f( gentity_t *ent )
{
  char  s[ MAX_TOKEN_CHARS ];
  int   upgrade, weapon;

  trap_Argv( 1, s, sizeof( s ) );
  upgrade = BG_FindUpgradeNumForName( s );
  weapon = BG_FindWeaponNumForName( s );

  if( upgrade != UP_NONE && BG_InventoryContainsUpgrade( upgrade, ent->client->ps.stats ) )
    BG_ActivateUpgrade( upgrade, ent->client->ps.stats );
  else if( weapon != WP_NONE && BG_InventoryContainsWeapon( weapon, ent->client->ps.stats ) )
    G_ForceWeaponChange( ent, weapon );
  else
    trap_SendServerCommand( ent-g_entities, va( "print \"You don't have the %s\n\"", s ) );
}


/*
=================
Cmd_DeActivateItem_f

Deactivate an item
=================
*/
void Cmd_DeActivateItem_f( gentity_t *ent )
{
  char  s[ MAX_TOKEN_CHARS ];
  int   upgrade;

  trap_Argv( 1, s, sizeof( s ) );
  upgrade = BG_FindUpgradeNumForName( s );

  if( BG_InventoryContainsUpgrade( upgrade, ent->client->ps.stats ) )
    BG_DeactivateUpgrade( upgrade, ent->client->ps.stats );
  else
    trap_SendServerCommand( ent-g_entities, va( "print \"You don't have the %s\n\"", s ) );
}


/*
=================
Cmd_ToggleItem_f
=================
*/
void Cmd_ToggleItem_f( gentity_t *ent )
{
  char  s[ MAX_TOKEN_CHARS ];
  int   upgrade, weapon, i;

  trap_Argv( 1, s, sizeof( s ) );
  upgrade = BG_FindUpgradeNumForName( s );
  weapon = BG_FindWeaponNumForName( s );

  if( weapon != WP_NONE )
  {
    //special case to allow switching between
    //the blaster and the primary weapon

    if( ent->client->ps.weapon != WP_BLASTER )
      weapon = WP_BLASTER;
    else
    {
      //find a held weapon which isn't the blaster
      for( i = WP_NONE + 1; i < WP_NUM_WEAPONS; i++ )
      {
        if( i == WP_BLASTER )
          continue;

        if( BG_InventoryContainsWeapon( i, ent->client->ps.stats ) )
        {
          weapon = i;
          break;
        }
      }

      if( i == WP_NUM_WEAPONS )
        weapon = WP_BLASTER;
    }

    G_ForceWeaponChange( ent, weapon );
  }
  else if( BG_InventoryContainsUpgrade( upgrade, ent->client->ps.stats ) )
  {
    if( BG_UpgradeIsActive( upgrade, ent->client->ps.stats ) )
      BG_DeactivateUpgrade( upgrade, ent->client->ps.stats );
    else
      BG_ActivateUpgrade( upgrade, ent->client->ps.stats );
  }
  else
    trap_SendServerCommand( ent-g_entities, va( "print \"You don't have the %s\n\"", s ) );
}

/*
=================
Cmd_Buy_f
=================
*/
void Cmd_Buy_f( gentity_t *ent )
{
  char      s[ MAX_TOKEN_CHARS ];
  int       i;
  int       weapon, upgrade, numItems = 0;
  int       maxAmmo, maxClips;
  qboolean  buyingEnergyAmmo = qfalse;
  qboolean  hasEnergyWeapon = qfalse;
  
  BG_UpdateWeaponData(g_proximityMines.integer, g_proximityMinesPrice.integer);
  BG_UpdateUpgradeData(g_proximityMines.integer, g_proximityMinesPrice.integer);
  
  for( i = UP_NONE; i < UP_NUM_UPGRADES; i++ )
  {
    if( BG_InventoryContainsUpgrade( i, ent->client->ps.stats ) )
      numItems++;
  }

  for( i = WP_NONE; i < WP_NUM_WEAPONS; i++ )
  {
    if( BG_InventoryContainsWeapon( i, ent->client->ps.stats ) )
    {
      if( BG_FindUsesEnergyForWeapon( i ) )
        hasEnergyWeapon = qtrue;
      numItems++;
    }
  }

  trap_Argv( 1, s, sizeof( s ) );

  weapon = BG_FindWeaponNumForName( s );
  upgrade = BG_FindUpgradeNumForName( s );

  if ( ent->client->ps.pm_flags&PMF_DUCKED && upgrade == UP_BATTLESUIT )
  {
  trap_SendServerCommand( ent-g_entities, va(
        "print \"Cannot buy a battlesuit while crouching\n\"" ) );
  return;
  }

  //special case to keep norf happy
  if( weapon == WP_NONE && upgrade == UP_AMMO )
  {
    buyingEnergyAmmo = hasEnergyWeapon;
  }

  if( buyingEnergyAmmo )
  {
    //no armoury nearby
    if( !G_BuildableRange( ent->client->ps.origin, 100, BA_H_REACTOR ) &&
        !G_BuildableRange( ent->client->ps.origin, 100, BA_H_REPEATER ) &&
        !G_BuildableRange( ent->client->ps.origin, 100, BA_H_ARMOURY ) )
    {
      trap_SendServerCommand( ent-g_entities, va(
        "print \"You must be near a reactor, repeater or armoury\n\"" ) );
      return;
    }
  }
  else
  {
    //no armoury nearby
    if( !G_BuildableRange( ent->client->ps.origin, 100, BA_H_ARMOURY ) )
    {
      trap_SendServerCommand( ent-g_entities, va( "print \"You must be near a powered armoury\n\"" ) );
      return;
    }
  }

  if( weapon != WP_NONE )
  {
    //already got this?
    
    if( BG_InventoryContainsWeapon( weapon, ent->client->ps.stats ) )
    {
      G_TriggerMenu( ent->client->ps.clientNum, MN_H_ITEMHELD );
      return;
    }

    // denyweapons
    if( weapon >= WP_PAIN_SAW && weapon <= WP_GRENADE &&
        ent->client->pers.denyHumanWeapons & ( 1 << (weapon - WP_BLASTER) ) )
    {
      trap_SendServerCommand( ent-g_entities, va( "print \"You are denied from buying this weapon\n\"" ) );
      return;
    }

    //can afford this?
    if( BG_FindPriceForWeapon( weapon ) > (short)ent->client->ps.persistant[ PERS_CREDIT ] )
    {
      G_TriggerMenu( ent->client->ps.clientNum, MN_H_NOFUNDS );
      return;
    }
    if(!g_multipleWeapons.integer)
    {
    //have space to carry this?
    if( BG_FindSlotsForWeapon( weapon ) & ent->client->ps.stats[ STAT_SLOTS ] )
    {
      G_TriggerMenu( ent->client->ps.clientNum, MN_H_NOSLOTS );
      return;
    }
    }
    
    if(!g_buyAll.integer)
    {
    if( BG_FindTeamForWeapon( weapon ) != WUT_HUMANS )
    {
      //shouldn't need a fancy dialog
      trap_SendServerCommand( ent-g_entities, va( "print \"You can't buy alien items\n\"" ) );
      return;
    }
    }
    
    if(!g_buyAll.integer)
    {
    //are we /allowed/ to buy this?
    if( !BG_FindPurchasableForWeapon( weapon ) )
    {
      trap_SendServerCommand( ent-g_entities, va( "print \"You can't buy this item\n\"" ) );
      return;
    }
    }

    //are we /allowed/ to buy this?
    if( !BG_FindStagesForWeapon( weapon, g_humanStage.integer ) || !BG_WeaponIsAllowed( weapon ) )
    {
      trap_SendServerCommand( ent-g_entities, va( "print \"You can't buy this item\n\"" ) );
      return;
    }

    //add to inventory
    BG_AddWeaponToInventory( weapon, ent->client->ps.stats );
    BG_FindAmmoForWeapon( weapon, &maxAmmo, &maxClips );

    if( BG_FindUsesEnergyForWeapon( weapon ) &&
        BG_InventoryContainsUpgrade( UP_BATTPACK, ent->client->ps.stats ) )
      maxAmmo = (int)( (float)maxAmmo * BATTPACK_MODIFIER );

    BG_PackAmmoArray( weapon, ent->client->ps.ammo, ent->client->ps.powerups,
                      maxAmmo, maxClips );

    G_ForceWeaponChange( ent, weapon );

    //set build delay/pounce etc to 0
    ent->client->ps.stats[ STAT_MISC ] = 0;

    //subtract from funds
    G_AddCreditToClient( ent->client, -(short)BG_FindPriceForWeapon( weapon ), qfalse );
  }
  else if( upgrade != UP_NONE )
  {
    //already got this?
    if( BG_InventoryContainsUpgrade( upgrade, ent->client->ps.stats ) )
    {
      G_TriggerMenu( ent->client->ps.clientNum, MN_H_ITEMHELD );
      return;
    }

    // denyweapons
    if( upgrade == UP_GRENADE &&
        ent->client->pers.denyHumanWeapons & ( 1 << (WP_GRENADE - WP_BLASTER) ) )
    {
      trap_SendServerCommand( ent-g_entities, va( "print \"You are denied from buying this upgrade\n\"" ) );
      return;
    }

    //can afford this?
    if( BG_FindPriceForUpgrade( upgrade ) > (short)ent->client->ps.persistant[ PERS_CREDIT ] )
    {
      G_TriggerMenu( ent->client->ps.clientNum, MN_H_NOFUNDS );
      return;
    }

    //have space to carry this?
    if( BG_FindSlotsForUpgrade( upgrade ) & ent->client->ps.stats[ STAT_SLOTS ] )
    {
      G_TriggerMenu( ent->client->ps.clientNum, MN_H_NOSLOTS );
      return;
    }

    if( BG_FindTeamForUpgrade( upgrade ) != WUT_HUMANS )
    {
      //shouldn't need a fancy dialog
      trap_SendServerCommand( ent-g_entities, va( "print \"You can't buy alien items\n\"" ) );
      return;
    }

    //are we /allowed/ to buy this?
    if( !BG_FindPurchasableForUpgrade( upgrade ) )
    {
      trap_SendServerCommand( ent-g_entities, va( "print \"You can't buy this item\n\"" ) );
      return;
    }

    //are we /allowed/ to buy this?
    if( !BG_FindStagesForUpgrade( upgrade, g_humanStage.integer ) || !BG_UpgradeIsAllowed( upgrade ) )
    {
      trap_SendServerCommand( ent-g_entities, va( "print \"You can't buy this item\n\"" ) );
      return;
    }

    if( upgrade == UP_AMMO )
      G_GiveClientMaxAmmo( ent, buyingEnergyAmmo );
    else
    {
      //add to inventory
      BG_AddUpgradeToInventory( upgrade, ent->client->ps.stats );
    }

    if( upgrade == UP_BATTPACK )
      G_GiveClientMaxAmmo( ent, qtrue );

    //subtract from funds
    G_AddCreditToClient( ent->client, -(short)BG_FindPriceForUpgrade( upgrade ), qfalse );
  }
  else
  {
    trap_SendServerCommand( ent-g_entities, va( "print \"Unknown item\n\"" ) );
  }

  if( trap_Argc( ) >= 2 )
  {
    trap_Argv( 2, s, sizeof( s ) );

    //retrigger the armoury menu
    if( !Q_stricmp( s, "retrigger" ) )
      ent->client->retriggerArmouryMenu = level.framenum + RAM_FRAMES;
  }

  //update ClientInfo
  ClientUserinfoChanged( ent->client->ps.clientNum );
}


/*
=================
Cmd_Sell_f
=================
*/
void Cmd_Sell_f( gentity_t *ent )
{
  char      s[ MAX_TOKEN_CHARS ];
  int       i;
  int       weapon, upgrade;

  trap_Argv( 1, s, sizeof( s ) );

  //no armoury nearby
  if( !G_BuildableRange( ent->client->ps.origin, 100, BA_H_ARMOURY ) )
  {
    trap_SendServerCommand( ent-g_entities, va( "print \"You must be near a powered armoury\n\"" ) );
    return;
  }

  weapon = BG_FindWeaponNumForName( s );
  upgrade = BG_FindUpgradeNumForName( s );

  if( weapon != WP_NONE )
  {
    //are we /allowed/ to sell this?
    if( !BG_FindPurchasableForWeapon( weapon ) )
    {
      trap_SendServerCommand( ent-g_entities, va( "print \"You can't sell this weapon\n\"" ) );
      return;
    }

    //remove weapon if carried
    if( BG_InventoryContainsWeapon( weapon, ent->client->ps.stats ) )
    {
      //guard against selling the HBUILD weapons exploit
      if( ( weapon == WP_HBUILD || weapon == WP_HBUILD2 ) &&
          ent->client->ps.stats[ STAT_MISC ] > 0 )
      {
        trap_SendServerCommand( ent-g_entities, va( "print \"Cannot sell until build timer expires\n\"" ) );
        return;
      }

      BG_RemoveWeaponFromInventory( weapon, ent->client->ps.stats );

      //add to funds
      G_AddCreditToClient( ent->client, (short)BG_FindPriceForWeapon( weapon ), qfalse );
    }

    //if we have this weapon selected, force a new selection
    if( weapon == ent->client->ps.weapon )
      G_ForceWeaponChange( ent, WP_NONE );
  }
  else if( upgrade != UP_NONE )
  {
    //are we /allowed/ to sell this?
    if( !BG_FindPurchasableForUpgrade( upgrade ) )
    {
      trap_SendServerCommand( ent-g_entities, va( "print \"You can't sell this item\n\"" ) );
      return;
    }
    //remove upgrade if carried
    if( BG_InventoryContainsUpgrade( upgrade, ent->client->ps.stats ) )
    {
      BG_RemoveUpgradeFromInventory( upgrade, ent->client->ps.stats );

      if( upgrade == UP_BATTPACK )
        G_GiveClientMaxAmmo( ent, qtrue );

      //add to funds
      G_AddCreditToClient( ent->client, (short)BG_FindPriceForUpgrade( upgrade ), qfalse );
    }
  }
  else if( !Q_stricmp( s, "weapons" ) )
  {
    for( i = WP_NONE + 1; i < WP_NUM_WEAPONS; i++ )
    {
      //guard against selling the HBUILD weapons exploit
      if( ( i == WP_HBUILD || i == WP_HBUILD2 ) &&
          ent->client->ps.stats[ STAT_MISC ] > 0 )
      {
        trap_SendServerCommand( ent-g_entities, va( "print \"Cannot sell until build timer expires\n\"" ) );
        continue;
      }

      if( BG_InventoryContainsWeapon( i, ent->client->ps.stats ) &&
          BG_FindPurchasableForWeapon( i ) )
      {
        BG_RemoveWeaponFromInventory( i, ent->client->ps.stats );

        //add to funds
        G_AddCreditToClient( ent->client, (short)BG_FindPriceForWeapon( i ), qfalse );
      }

      //if we have this weapon selected, force a new selection
      if( i == ent->client->ps.weapon )
        G_ForceWeaponChange( ent, WP_NONE );
    }
  }
  else if( !Q_stricmp( s, "upgrades" ) )
  {
    for( i = UP_NONE + 1; i < UP_NUM_UPGRADES; i++ )
    {
      //remove upgrade if carried
      if( BG_InventoryContainsUpgrade( i, ent->client->ps.stats ) &&
          BG_FindPurchasableForUpgrade( i ) )
      {
        BG_RemoveUpgradeFromInventory( i, ent->client->ps.stats );

        if( i == UP_BATTPACK )
        {
          int j;

          //remove energy
          for( j = WP_NONE; j < WP_NUM_WEAPONS; j++ )
          {
            if( BG_InventoryContainsWeapon( j, ent->client->ps.stats ) &&
                BG_FindUsesEnergyForWeapon( j ) &&
                !BG_FindInfinteAmmoForWeapon( j ) )
            {
              BG_PackAmmoArray( j, ent->client->ps.ammo, ent->client->ps.powerups, 0, 0 );
            }
          }
        }

        //add to funds
        G_AddCreditToClient( ent->client, (short)BG_FindPriceForUpgrade( i ), qfalse );
      }
    }
  }
  else
    trap_SendServerCommand( ent-g_entities, va( "print \"Unknown item\n\"" ) );

  if( trap_Argc( ) >= 2 )
  {
    trap_Argv( 2, s, sizeof( s ) );

    //retrigger the armoury menu
    if( !Q_stricmp( s, "retrigger" ) )
      ent->client->retriggerArmouryMenu = level.framenum + RAM_FRAMES;
  }

  //update ClientInfo
  ClientUserinfoChanged( ent->client->ps.clientNum );
}


/*
=================
Cmd_Build_f
=================
*/
void Cmd_Build_f( gentity_t *ent )
{
  char          s[ MAX_TOKEN_CHARS ];
  buildable_t   buildable;
  float         dist;
  vec3_t        origin;
  pTeam_t       team;
  
  if( ent->client->pers.paused )
  {
    trap_SendServerCommand( ent-g_entities,
      "print \"You may not build while paused\n\"" );
    return;
  }

  if( ent->client->pers.denyBuild )
  {
    trap_SendServerCommand( ent-g_entities,
      "print \"Your building rights have been revoked\n\"" );
    return;
  }

  if( g_newbieNoBuild.integer
    && g_newbieNamePrefix.string[ 0 ]
    && Q_stricmpn ( ent->client->pers.netname, g_newbieNamePrefix.string, strlen(g_newbieNamePrefix.string ) ) == 0 )
  {
    trap_SendServerCommand( ent-g_entities,
      va( "print \"You cannot build until you set a name\n%s%s\"",
      ( g_newbieNoBuildMessage.string[ 0 ] ) ? g_newbieNoBuildMessage.string : "",
      ( g_newbieNoBuildMessage.string[ 0 ] ) ? "\n" : "" ) );
    return;
  }

  trap_Argv( 1, s, sizeof( s ) );

  buildable = BG_FindBuildNumForName( s );


  if( g_suddenDeath.integer)
  {
    if( g_suddenDeathMode.integer == SDMODE_SELECTIVE )
    {
      if( !BG_FindReplaceableTestForBuildable( buildable ) )
      {
        trap_SendServerCommand( ent-g_entities,
          "print \"This building type cannot be rebuilt during Sudden Death\n\"" );
        return;
      }
      if( G_BuildingExists( buildable ) )
      {
        trap_SendServerCommand( ent-g_entities,
          "print \"You can only rebuild one of each type of rebuildable building during Sudden Death.\n\"" );
        return;
      }
    }
    else if( g_suddenDeathMode.integer == SDMODE_NO_BUILD )
    {
      trap_SendServerCommand( ent-g_entities,
        "print \"Building is not allowed during Sudden Death\n\"" );
      return;
    }
  }

  team = ent->client->ps.stats[ STAT_PTEAM ];

  if( buildable != BA_NONE &&
      ( ( 1 << ent->client->ps.weapon ) & BG_FindBuildWeaponForBuildable( buildable ) ) &&
      !( ent->client->ps.stats[ STAT_STATE ] & SS_INFESTING ) &&
      !( ent->client->ps.stats[ STAT_STATE ] & SS_HOVELING ) &&
      BG_BuildableIsAllowed( buildable ) &&
      ( ( team == PTE_ALIENS && BG_FindStagesForBuildable( buildable, g_alienStage.integer ) ) ||
        ( team == PTE_HUMANS && BG_FindStagesForBuildable( buildable, g_humanStage.integer ) ) ) )
  {
    dist = BG_FindBuildDistForClass( ent->client->ps.stats[ STAT_PCLASS ] );

    //these are the errors displayed when the builder first selects something to use
    switch( G_CanBuild( ent, buildable, dist, origin ) )
    {
      case IBE_NONE:
      case IBE_TNODEWARN:
      case IBE_RPTWARN:
      case IBE_RPTWARN2:
      case IBE_SPWNWARN:
      case IBE_NOROOM:
      case IBE_NORMAL:
      case IBE_HOVELEXIT:
        ent->client->ps.stats[ STAT_BUILDABLE ] = ( buildable | SB_VALID_TOGGLEBIT );
        break;

      case IBE_NOASSERT:
        G_TriggerMenu( ent->client->ps.clientNum, MN_A_NOASSERT );
        break;

      case IBE_NOOVERMIND:
        G_TriggerMenu( ent->client->ps.clientNum, MN_A_NOOVMND );
        break;

      case IBE_OVERMIND:
        G_TriggerMenu( ent->client->ps.clientNum, MN_A_OVERMIND );
        break;

      case IBE_REACTOR:
        G_TriggerMenu( ent->client->ps.clientNum, MN_H_REACTOR );
        break;

      case IBE_REPEATER:
        G_TriggerMenu( ent->client->ps.clientNum, MN_H_REPEATER );
        break;

      case IBE_NOPOWER:
        G_TriggerMenu( ent->client->ps.clientNum, MN_H_NOPOWER );
        break;

      case IBE_NOCREEP:
        G_TriggerMenu( ent->client->ps.clientNum, MN_A_NOCREEP );
        break;

      case IBE_NODCC:
        G_TriggerMenu( ent->client->ps.clientNum, MN_H_NODCC );
        break;

      default:
        break;
    }
  }
  else
    trap_SendServerCommand( ent-g_entities, va( "print \"Cannot build this item\n\"" ) );
}


/*
=================
Cmd_Boost_f
=================
*/
void Cmd_Boost_f( gentity_t *ent )
{
  if( BG_InventoryContainsUpgrade( UP_JETPACK, ent->client->ps.stats ) &&
      BG_UpgradeIsActive( UP_JETPACK, ent->client->ps.stats ) )
    return;

  if( ent->client->pers.cmd.buttons & BUTTON_WALKING )
    return;

  if( ( ent->client->ps.stats[ STAT_PTEAM ] == PTE_HUMANS ) &&
      ( ent->client->ps.stats[ STAT_STAMINA ] > 0 ) )
    ent->client->ps.stats[ STAT_STATE ] |= SS_SPEEDBOOST;
}

/*
=================
Cmd_Protect_f
=================
*/
void Cmd_Protect_f( gentity_t *ent )
{
  vec3_t      forward, end;
  trace_t     tr;
  gentity_t   *traceEnt;

  if( !ent->client->pers.designatedBuilder )
  {
    trap_SendServerCommand( ent-g_entities, "print \"Only designated"
        " builders can toggle structure protection.\n\"" );
    return;
  }

  AngleVectors( ent->client->ps.viewangles, forward, NULL, NULL );
  VectorMA( ent->client->ps.origin, 100, forward, end );

  trap_Trace( &tr, ent->client->ps.origin, NULL, NULL, end, ent->s.number,
    MASK_PLAYERSOLID );
  traceEnt = &g_entities[ tr.entityNum ];

  if( tr.fraction < 1.0f && ( traceEnt->s.eType == ET_BUILDABLE ) &&
      ( traceEnt->biteam == ent->client->pers.teamSelection ) )
  {
    if( traceEnt->s.eFlags & EF_DBUILDER )
    {
      trap_SendServerCommand( ent-g_entities,
        "print \"Structure protection removed\n\"" );
      traceEnt->s.eFlags &= ~EF_DBUILDER;
    }
    else
    {
      trap_SendServerCommand( ent-g_entities, 
        "print \"Structure protection applied\n\"" );
      traceEnt->s.eFlags |= EF_DBUILDER;

      // adding protection turns off deconstruction mark
      traceEnt->deconstruct = qfalse;
    }
  }
}

/*
=================
Cmd_Builder_f
=================
*/
void Cmd_Builder_f( gentity_t *ent )
{
  vec3_t      forward, right, up;
  vec3_t      start, end;
  trace_t     tr;
  gentity_t   *traceEnt;
  char bdnumbchr[21];

  if( !G_admin_permission( ent, ADMF_BUILDCHECK ) )
    return;
	
  AngleVectors( ent->client->ps.viewangles, forward, right, up );
  if( ent->client->pers.teamSelection != PTE_NONE )
    CalcMuzzlePoint( ent, forward, right, up, start );
  else
    VectorCopy( ent->client->ps.origin, start );
  VectorMA( start, 1000, forward, end );

  trap_Trace( &tr, start, NULL, NULL, end, ent->s.number, MASK_PLAYERSOLID );
  traceEnt = &g_entities[ tr.entityNum ];

  Com_sprintf( bdnumbchr, sizeof(bdnumbchr), "%i", traceEnt->bdnumb );

  if( tr.fraction < 1.0f && ( traceEnt->s.eType == ET_BUILDABLE ) )
  {
    trap_SendServerCommand( ent-g_entities, va(
      "print \"Building was built by: %s^7, buildlog number: %s^7\n\"",
      G_FindBuildLogName( traceEnt->bdnumb ),
      (traceEnt->bdnumb != -1) ? bdnumbchr : "none" ) );
  }
}

 /*
 =================
 Cmd_Resign_f
 =================
 */
 void Cmd_Resign_f( gentity_t *ent )
 {
   if( !ent->client->pers.designatedBuilder )
   {
     trap_SendServerCommand( ent-g_entities,
       "print \"You are not a designated builder\n\"" );
     return;
   }
 
   ent->client->pers.designatedBuilder = qfalse;
   trap_SendServerCommand( -1, va(
     "print \"%s" S_COLOR_WHITE " has resigned\n\"",
     ent->client->pers.netname ) );
   G_CheckDBProtection( );
 }
 
 

/*
=================
Cmd_Reload_f
=================
*/
void Cmd_Reload_f( gentity_t *ent )
{
  if( !ent->client->pers.designatedBuilder && ( ent->client->ps.weapon >= WP_ABUILD ) &&
    ( ent->client->ps.weapon <= WP_HBUILD ) )
  {
  
   Cmd_Mark_f( ent );
   return;
  }
  else if( ( ent->client->ps.weapon >= WP_ABUILD ) &&
    ( ent->client->ps.weapon <= WP_HBUILD ) )
  {
    Cmd_Protect_f( ent );
  }
  else if( ent->client->ps.weaponstate != WEAPON_RELOADING )
    ent->client->ps.pm_flags |= PMF_WEAPON_RELOAD;
}


/*
=================
Cmd_MyStats_f
=================
*/
void Cmd_MyStats_f( gentity_t *ent )
{

   if(!ent) return;
	   

   if( !level.intermissiontime && ent->client->pers.statscounters.timeLastViewed && (level.time - ent->client->pers.statscounters.timeLastViewed) <60000 ) 
   {   
     ADMP( "You may only check your stats once per minute and during intermission.\n");
     return;
   }
   
   if( !g_myStats.integer )
   {
    ADMP( "myStats has been disabled\n");
    return;
   }
   
   ADMP( G_statsString( &ent->client->pers.statscounters, &ent->client->pers.teamSelection ) );
   ent->client->pers.statscounters.timeLastViewed = level.time;
  	
  return;
}

char *G_statsString( statsCounters_t *sc, pTeam_t *pt )
{
  char *s;
  
  int percentNearBase=0;
  int percentJetpackWallwalk=0;
  int percentHeadshots=0;
	
  if( sc->timealive )
   percentNearBase = (int)(100 *  (float) sc->timeinbase / ((float) (sc->timealive ) ) );
  
	
  if( *pt == PTE_ALIENS )
  {
    if( sc->dretchbasytime > 0 )
     percentJetpackWallwalk = (int)(100 *  (float) sc->jetpackusewallwalkusetime / ((float) ( sc->dretchbasytime) ) );
    
    if( sc->hitslocational )
      percentHeadshots = (int)(100 * (float) sc->headshots / ((float) (sc->hitslocational) ) );
    
    s = va( "^3Kills:^7 %3i ^3StructKills:^7 %3i ^3Assists:^7 %3i^7 ^3Poisons:^7 %3i ^3Headshots:^7 %3i (%3i)\n^3Deaths:^7 %3i ^3Feeds:^7 %3i ^3Suicides:^7 %3i ^3Teamkills:^7 %3i\n^3Damage to:^7 ^3Enemies:^7 %5i ^3Structs:^7 %5i ^3Friendlies:^7 %3i \n^3Structs Built:^7 %3i ^3Time Near Base:^7 %3i ^3Time wallwalking:^7 %i\n",
     sc->kills,
     sc->structskilled,
     sc->assists,
     sc->repairspoisons,
     sc->headshots,
     percentHeadshots,
     sc->deaths,
     sc->feeds,
     sc->suicides,
     sc->teamkills,
     sc->dmgdone,
     sc->structdmgdone,
     sc->ffdmgdone,
     sc->structsbuilt,
     percentNearBase,
     percentJetpackWallwalk
         );	
  }
  else if( *pt == PTE_HUMANS )
  {
    if( sc->timealive )
     percentJetpackWallwalk = (int)(100 *  (float) sc->jetpackusewallwalkusetime / ((float) ( sc->timealive ) ) );
    s = va( "^3Kills:^7 %3i ^3StructKills:^7 %3i ^3Assists:^7 %3i \n^3Deaths:^7 %3i ^3Feeds:^7 %3i ^3Suicides:^7 %3i ^3Teamkills:^7 %3i\n^3Damage to:^7 ^3Enemies:^7 %5i ^3Structs:^7 %5i ^3Friendlies:^7 %3i \n^3Structs Built:^7 %3i ^3Repairs:^7 %4i ^3Time Near Base:^7 %3i ^3Time Jetpacking:^7 %3i\n",
     sc->kills,
     sc->structskilled,
     sc->assists,
     sc->deaths,
     sc->feeds,
     sc->suicides,
     sc->teamkills,
     sc->dmgdone,
     sc->structdmgdone,
     sc->ffdmgdone,
     sc->structsbuilt,
     sc->repairspoisons,
     percentNearBase,
     percentJetpackWallwalk
         );	
  }
  else s="No stats available\n";
	
  return s;
}

  /*
 =================
 Cmd_AllStats_f
 =================
 */
 void Cmd_AllStats_f( gentity_t *ent )
 {
    int i;
    int NextViewTime;
    int NumResults;
    int Teamcolor;
    gentity_t *tmpent;
 
    //check if ent exists
    if(!ent) return;
 
    NextViewTime = ent->client->pers.statscounters.AllstatstimeLastViewed + g_AllStatsTime.integer * 1000;
    //check if you can use the cmd at this time
    if( !level.intermissiontime && level.time < NextViewTime)
    {
      ADMP( va("You may only check your stats every %i Seconds and during intermission. Next valid time is %d:%02d\n",( g_AllStatsTime.integer ) ? ( g_AllStatsTime.integer ) : 60, ( NextViewTime / 60000 ), ( NextViewTime / 1000 ) % 60 ) );
      return;
    }
    //see if allstats is enabled
    if( !g_AllStats.integer )
    {
     ADMP( "AllStats has been disabled\n");
     return;
    }
    ADMP("^3K^2=^7Kills ^3A^2=^7Assists ^3SK^2=^7StructKills\n^3D^2=^7Deaths ^3F^2=^7Feeds ^3S^2=^7Suicides ^3TK^2=^7Teamkills\n^3DD^2=^7Damage done ^3TDD^2=^7Team Damage done\n^3SB^2=^7Structs Built\n\n" );
    //display a header describing the data
    ADMP( "^3 #|  K   A  SK|  D   F   S  TK|   DD   TDD| SB| Name\n" );
    NumResults = 0;
    //loop through the clients that are connected
    for( i = 0; i < level.numConnectedClients; i++ ) 
    {
       //assign a tempent 4 the hell of it
       tmpent = &g_entities[ level.sortedClients[ i ] ];
 
       //check for what mode we are working in and display relevent data
       if( g_AllStats.integer == 1 )
       {
            //check if client is connected and on same team
            if( tmpent->client && tmpent->client->pers.connected == CON_CONNECTED && tmpent->client->pers.teamSelection == ent->client->pers.teamSelection && tmpent->client->pers.teamSelection != PTE_NONE )
            {
               NumResults++;
               if( tmpent->client->pers.teamSelection == PTE_ALIENS ) Teamcolor = 1;
               if( tmpent->client->pers.teamSelection == PTE_HUMANS ) Teamcolor = 5;
               ADMP( va( "^%i%2i^3|^%i%3i %3i %3i^3|^%i%3i %3i %3i %3i^3|^%i%5i %5i^3|^%i%3i^3|^7 %s\n",
               Teamcolor,
               NumResults,
               Teamcolor,
               ( tmpent->client->pers.statscounters.kills ) ? tmpent->client->pers.statscounters.kills : 0,
               ( tmpent->client->pers.statscounters.assists ) ? tmpent->client->pers.statscounters.assists : 0,
               ( tmpent->client->pers.statscounters.structskilled ) ? tmpent->client->pers.statscounters.structskilled : 0,
               Teamcolor,
               ( tmpent->client->pers.statscounters.deaths ) ? tmpent->client->pers.statscounters.deaths : 0,
               ( tmpent->client->pers.statscounters.feeds ) ? tmpent->client->pers.statscounters.feeds : 0,
               ( tmpent->client->pers.statscounters.suicides ) ? tmpent->client->pers.statscounters.suicides : 0,
               ( tmpent->client->pers.statscounters.teamkills ) ? tmpent->client->pers.statscounters.teamkills : 0,
               Teamcolor,
               ( tmpent->client->pers.statscounters.dmgdone ) ? tmpent->client->pers.statscounters.dmgdone : 0,
               ( tmpent->client->pers.statscounters.ffdmgdone ) ? tmpent->client->pers.statscounters.ffdmgdone : 0,
               Teamcolor,
               ( tmpent->client->pers.statscounters.structsbuilt ) ? tmpent->client->pers.statscounters.structsbuilt : 0,
               ( tmpent->client->pers.netname ) ? tmpent->client->pers.netname : "Unknown" ) );
            }
       }
       else if( g_AllStats.integer == 2 )
       {
            //check if client is connected and has some stats or atleast is on a team
            if( tmpent->client && tmpent->client->pers.connected == CON_CONNECTED && ( tmpent->client->pers.teamSelection != PTE_NONE ) )
            {
               NumResults++;
               if( tmpent->client->pers.teamSelection == PTE_ALIENS ) Teamcolor = 1;
               if( tmpent->client->pers.teamSelection == PTE_HUMANS ) Teamcolor = 5;
               ADMP( va( "^%i%2i^3|^%i%3i %3i %3i^3|^%i%3i %3i %3i %3i^3|^%i%5i %5i^3|^%i%3i^3|^7 %s\n",
               Teamcolor,
               NumResults,
               Teamcolor,
               ( tmpent->client->pers.statscounters.kills ) ? tmpent->client->pers.statscounters.kills : 0,
               ( tmpent->client->pers.statscounters.assists ) ? tmpent->client->pers.statscounters.assists : 0,
               ( tmpent->client->pers.statscounters.structskilled ) ? tmpent->client->pers.statscounters.structskilled : 0,
               Teamcolor,
               ( tmpent->client->pers.statscounters.deaths ) ? tmpent->client->pers.statscounters.deaths : 0,
               ( tmpent->client->pers.statscounters.feeds ) ? tmpent->client->pers.statscounters.feeds : 0,
               ( tmpent->client->pers.statscounters.suicides ) ? tmpent->client->pers.statscounters.suicides : 0,
               ( tmpent->client->pers.statscounters.teamkills ) ? tmpent->client->pers.statscounters.teamkills : 0,
               Teamcolor,
               ( tmpent->client->pers.statscounters.dmgdone ) ? tmpent->client->pers.statscounters.dmgdone : 0,
               ( tmpent->client->pers.statscounters.ffdmgdone ) ? tmpent->client->pers.statscounters.ffdmgdone : 0,
               Teamcolor,
               ( tmpent->client->pers.statscounters.structsbuilt ) ? tmpent->client->pers.statscounters.structsbuilt : 0,
               ( tmpent->client->pers.netname ) ? tmpent->client->pers.netname : "Unknown" ) );
            }
       }
    }
    if( NumResults == 0 ) {
       ADMP( "   ^3EMPTY!\n" );
    } else {
       ADMP( va( "^7 %i Players found!\n", NumResults ) );
    }
    //update time last viewed

    ent->client->pers.statscounters.AllstatstimeLastViewed = level.time;
    return;
}

/*
=================
Cmd_TeamStatus_f
=================
*/
void Cmd_TeamStatus_f( gentity_t *ent )
{
 int i;
 int builders = 0;
 int arm = 0, medi = 0, boost = 0;
 int omrc = 0;
 qboolean omrcbuild = qfalse;
 gentity_t *tmp;
 
 if( !g_teamStatus.integer )
 return;
 
 if( ent->client->pers.muted )
  {
    trap_SendServerCommand( ent - g_entities,
      "print \"You are muted and cannot use message commands.\n\"" );
    return;
  }
  
 if( g_teamStatusTime.integer && ent->client->pers.lastTeamStatus && (level.time - ent->client->pers.lastTeamStatus) < g_teamStatusTime.integer * 1000 )
  {
   ADMP( va("You may only check your team's status once every %i seconds.\n", g_teamStatusTime.integer  ));
   return;
  }
  
 ent->client->pers.lastTeamStatus = level.time;
 
 if( ent->client->pers.teamSelection == PTE_ALIENS ){
 
 //OM detection
 for ( i = 1, tmp = g_entities + i; i < level.num_entities; i++, tmp++ )
  {
    if( tmp->s.eType != ET_BUILDABLE )
      continue;
    if( tmp->s.modelindex == BA_A_OVERMIND ){
      omrc = tmp->health;
      omrcbuild = tmp->spawned;
      break;
      }
  }
  
 //Builder detection
 for( i = 0; i < level.maxclients; i++ )
  {
    tmp = &g_entities[ i ];
    if( !tmp->client )
      continue;
    if( tmp->client->pers.connected != CON_CONNECTED )
      continue;
    if( tmp->client->pers.teamSelection == PTE_NONE )
      continue;
    if( ( tmp->client->ps.stats[ STAT_PCLASS ] == PCL_ALIEN_BUILDER0 ||
       tmp->client->ps.stats[ STAT_PCLASS ] == PCL_ALIEN_BUILDER0_UPG ) ){
       builders++;
       }
  }
  
  //Booster detection
  for ( i = 1, tmp = g_entities + i; i < level.num_entities; i++, tmp++ )
  {
    if( tmp->s.eType != ET_BUILDABLE )
      continue;
    if( tmp->s.modelindex == BA_A_BOOSTER ){
       boost++;
      }
  }
  
    G_Say( ent, NULL, SAY_TEAM, va("^3OM: %s(%d) ^3Eggs: ^6%i^3 Builders: ^6%i^3 Boosters: ^6%i^7", ( omrc <= 0 ) ? "^1Down" : ( omrcbuild ) ? "^2Up" : "^5Building", ( omrc > 0 ) ? omrc * 100 / OVERMIND_HEALTH : 0, level.numAlienSpawns, builders, boost ));
 }
 
 if( ent->client->pers.teamSelection == PTE_HUMANS ){
 
 //RC detection
 for ( i = 1, tmp = g_entities + i; i < level.num_entities; i++, tmp++ )
  {
    if( tmp->s.eType != ET_BUILDABLE )
      continue;
    if( tmp->s.modelindex == BA_H_REACTOR ){
      omrc = tmp->health;
      omrcbuild = tmp->spawned;
      break;
      }
  }
 
 //Builder detection
 for( i = 0; i < level.maxclients; i++ )
  {
    tmp = &g_entities[ i ];
    if( !tmp->client )
      continue;
    if( tmp->client->pers.connected != CON_CONNECTED )
      continue;
    if( tmp->client->pers.teamSelection == PTE_NONE )
      continue;
    if( ( BG_InventoryContainsWeapon( WP_HBUILD, tmp->client->ps.stats ) ||
       BG_InventoryContainsWeapon( WP_HBUILD2, tmp->client->ps.stats ) ) ){
       builders++;
       }
  }
  
 //Arm detection
 for ( i = 1, tmp = g_entities + i; i < level.num_entities; i++, tmp++ )
  {
    if( tmp->s.eType != ET_BUILDABLE )
      continue;
    if( tmp->s.modelindex == BA_H_ARMOURY ){
      arm++;
      }
  }
  
 //Medi detection
 for ( i = 1, tmp = g_entities + i; i < level.num_entities; i++, tmp++ )
  {
    if( tmp->s.eType != ET_BUILDABLE )
      continue;
    if( tmp->s.modelindex == BA_H_MEDISTAT ){
      medi++;
      }
  }

    G_Say( ent, NULL, SAY_TEAM, va("^3RC: %s(%d) ^3Telenodes: ^6%i^3 Builders: ^6%i^3 Armouries: ^6%i^3 Medistations: ^6%i^7", ( omrc <= 0 ) ? "^1Down" : ( omrcbuild ) ? "^2Up" : "^5Building",
     ( omrc > 0 ) ? omrc * 100 / REACTOR_HEALTH : 0, level.numHumanSpawns, builders, arm, medi ));
 }
 
 return;
}

/*
=================
G_StopFollowing

If the client being followed leaves the game, or you just want to drop
to free floating spectator mode
=================
*/
void G_StopFollowing( gentity_t *ent )
{
  ent->client->ps.persistant[ PERS_TEAM ] = TEAM_SPECTATOR;
  ent->client->sess.sessionTeam = TEAM_SPECTATOR;
  ent->client->sess.spectatorState = SPECTATOR_FREE;
  ent->client->sess.spectatorClient = -1;
  ent->client->ps.pm_flags &= ~PMF_FOLLOW;
  ent->client->ps.stats[ STAT_PTEAM ] = PTE_NONE;

  ent->client->ps.stats[ STAT_STATE ] &= ~SS_WALLCLIMBING;
  ent->client->ps.stats[ STAT_STATE ] &= ~SS_WALLCLIMBINGCEILING;
  ent->client->ps.eFlags &= ~EF_WALLCLIMB;
  ent->client->ps.viewangles[ PITCH ] = 0.0f;

  ent->client->ps.clientNum = ent - g_entities;

  CalculateRanks( );
}

/*
=================
G_FollowNewClient

This was a really nice, elegant function. Then I fucked it up.
=================
*/
qboolean G_FollowNewClient( gentity_t *ent, int dir )
{
  int       clientnum = ent->client->sess.spectatorClient;
  int       original = clientnum;
  qboolean  selectAny = qfalse;

  if( dir > 1 )
    dir = 1;
  else if( dir < -1 )
    dir = -1;
  else if( dir == 0 )
    return qtrue;

  if( ent->client->sess.sessionTeam != TEAM_SPECTATOR )
    return qfalse;

  // select any if no target exists
  if( clientnum < 0 || clientnum >= level.maxclients )
  {
    clientnum = original = 0;
    selectAny = qtrue;
  }

  do
  {
    clientnum += dir;

    if( clientnum >= level.maxclients )
      clientnum = 0;

    if( clientnum < 0 )
      clientnum = level.maxclients - 1;

    // avoid selecting existing follow target
    if( clientnum == original && !selectAny )
      continue; //effectively break;

    // can't follow self
    if( &level.clients[ clientnum ] == ent->client )
      continue;

    // can only follow connected clients
    if( level.clients[ clientnum ].pers.connected != CON_CONNECTED )
      continue;

    // can't follow another spectator
    if( level.clients[ clientnum ].pers.teamSelection == PTE_NONE && !g_specAspec.integer )
      continue;

    // this is good, we can use it
    ent->client->sess.spectatorClient = clientnum;
    ent->client->sess.spectatorState = SPECTATOR_FOLLOW;
    return qtrue;

  } while( clientnum != original );

  return qfalse;
}

/*
=================
G_ToggleFollow
=================
*/
void G_ToggleFollow( gentity_t *ent )
{
  if( level.mapRotationVoteTime )
  {
    G_IntermissionMapVoteCommand( ent, qfalse, qtrue );
    return;
  }

  if( ent->client->sess.spectatorState == SPECTATOR_FOLLOW )
    G_StopFollowing( ent );
  else if( ent->client->sess.spectatorState == SPECTATOR_FREE )
    G_FollowNewClient( ent, 1 );
}

/*
=================
Cmd_Follow_f
=================
*/
void Cmd_Follow_f( gentity_t *ent )
{
  int   i;
  int   pids[ MAX_CLIENTS ];
  char  arg[ MAX_TOKEN_CHARS ];

  if( trap_Argc( ) != 2 )
  {
    G_ToggleFollow( ent );
  }
  else if( ent->client->sess.spectatorState == SPECTATOR_FREE ||
           ent->client->sess.spectatorState == SPECTATOR_FOLLOW )
  {
    trap_Argv( 1, arg, sizeof( arg ) );
    if( G_ClientNumbersFromString( arg, pids ) == 1 )
    {
      i = pids[ 0 ];
    }
    else
    {
      i = G_ClientNumberFromString( ent, arg );

      if( i == -1 )
      {
        trap_SendServerCommand( ent - g_entities,
          "print \"follow: invalid player\n\"" );
        return;
      }
    }

    // can't follow self
    if( &level.clients[ i ] == ent->client )
      return;

    // can't follow another spectator
    if( level.clients[ i ].pers.teamSelection == PTE_NONE && !g_specAspec.integer  )
      return;

    ent->client->sess.spectatorState = SPECTATOR_FOLLOW;
    ent->client->sess.spectatorClient = i;
  }
}

/*
=================
Cmd_FollowCycle_f
=================
*/
void Cmd_FollowCycle_f( gentity_t *ent )
{
  char args[ 11 ];
  int  dir = 1;

  trap_Argv( 0, args, sizeof( args ) );
  if( Q_stricmp( args, "followprev" ) == 0 )
    dir = -1;

  // won't work unless spectating
  if( ent->client->sess.spectatorState == SPECTATOR_NOT )
    return;

  G_FollowNewClient( ent, dir );
}

/*
=================
Cmd_PTRCVerify_f

Check a PTR code is valid
=================
*/
void Cmd_PTRCVerify_f( gentity_t *ent )
{
  connectionRecord_t  *connection;
  char                s[ MAX_TOKEN_CHARS ] = { 0 };
  int                 code;
  int		      i;
  gentity_t *players;

  trap_Argv( 1, s, sizeof( s ) );

  if( !strlen( s ) )
    return;

  code = atoi( s );
  
  //Some sanity checks...BECAUSE THIS FUNCTION IS INSANE! lol
  for( i = 0; i < level.maxclients; i++ )
  {
    players = &g_entities[ i ];
    if( !players->client )
      continue;
    if( players->client->pers.connected != CON_CONNECTED )
      continue;
    if( players == ent )
    continue;

    if( players->client->pers.connection->ptrCode == code )
    {
      trap_SendServerCommand( ent - g_entities,
        "print \"This PTR code is already in use\n\"" );
      G_AdminsPrintf("%s^7 attempted to use %s^7's PTR code\n", ent->client->pers.netname, players->client->pers.netname);
      return;
    }
  }

  if( G_VerifyPTRC( code ) )
  {
    connection = G_FindConnectionForCode( code );

    // valid code
    if( connection->clientTeam != PTE_NONE )
      trap_SendServerCommand( ent->client->ps.clientNum, "ptrcconfirm" );

    // restore mapping
    ent->client->pers.connection = connection;
  }
  else
  {
    // invalid code -- generate a new one
    connection = G_GenerateNewConnection( ent->client );

    if( connection )
    {
      trap_SendServerCommand( ent->client->ps.clientNum,
        va( "ptrcissue %d", connection->ptrCode ) );
    }
  }
}

/*
=================
Cmd_PTRCRestore_f

Restore against a PTR code
=================
*/
void Cmd_PTRCRestore_f( gentity_t *ent )
{
  char                s[ MAX_TOKEN_CHARS ] = { 0 };
  int                 code;
  connectionRecord_t  *connection;
  int i;
  gentity_t *players;

  trap_Argv( 1, s, sizeof( s ) );

  if( !strlen( s ) )
    return;

  code = atoi( s );
  
  //Some sanity checks..
  for( i = 0; i < level.maxclients; i++ )
  {
    players = &g_entities[ i ];
    if( !players->client )
      continue;
    if( players->client->pers.connected != CON_CONNECTED )
      continue;
    if( players == ent )
    continue;

    if( players->client->pers.connection->ptrCode == code )
    {
      trap_SendServerCommand( ent - g_entities,
        "print \"This PTR code is already in use\n\"" );
      G_AdminsPrintf("%s^7 attempted to use %s^7's PTR code\n", ent->client->pers.netname, players->client->pers.netname);
      return;
    }
  }
  

  if( G_VerifyPTRC( code ) )
  {
    if( ent->client->pers.joinedATeam )
    {
      trap_SendServerCommand( ent - g_entities,
        "print \"You cannot use a PTR code after joining a team\n\"" );
    }
    else
    {
      // valid code
      connection = G_FindConnectionForCode( code );

      if( connection )
      {
        // set the correct team
        G_ChangeTeam( ent, connection->clientTeam );

        // set the correct credit
        ent->client->ps.persistant[ PERS_CREDIT ] = 0;
        G_AddCreditToClient( ent->client, connection->clientCredit, qtrue );
      }
    }
  }
  else
  {
    trap_SendServerCommand( ent - g_entities,
      va( "print \"\"%d\" is not a valid PTR code\n\"", code ) );
  }
}

static void Cmd_Ignore_f( gentity_t *ent )
{
  int pids[ MAX_CLIENTS ];
  char name[ MAX_NAME_LENGTH ];
  char cmd[ 9 ];
  int matches = 0;
  int i;
  qboolean ignore = qfalse;

  trap_Argv( 0, cmd, sizeof( cmd ) );
  if( Q_stricmp( cmd, "ignore" ) == 0 )
    ignore = qtrue;

  if( trap_Argc() < 2 )
  {
    trap_SendServerCommand( ent-g_entities, va( "print \"[skipnotify]"
      "%s: usage \\%s [clientNum | partial name match]\n\"", cmd, cmd ) );
    return;
  }

  Q_strncpyz( name, ConcatArgs( 1 ), sizeof( name ) );
  matches = G_ClientNumbersFromString( name, pids ); 
  if( matches < 1 )
  {
    trap_SendServerCommand( ent-g_entities, va( "print \"[skipnotify]"
      "%s: no clients match the name '%s'\n\"", cmd, name ) );
    return;
  }

  for( i = 0; i < matches; i++ )
  {
    if( ignore )
    {
      if( !BG_ClientListTest( &ent->client->sess.ignoreList, pids[ i ] ) )
      {
        BG_ClientListAdd( &ent->client->sess.ignoreList, pids[ i ] );
        ClientUserinfoChanged( ent->client->ps.clientNum );
        trap_SendServerCommand( ent-g_entities, va( "print \"[skipnotify]"
          "ignore: added %s^7 to your ignore list\n\"",
          level.clients[ pids[ i ] ].pers.netname ) );
      }
      else
      {
        trap_SendServerCommand( ent-g_entities, va( "print \"[skipnotify]"
          "ignore: %s^7 is already on your ignore list\n\"",
          level.clients[ pids[ i ] ].pers.netname ) );
      }
    }
    else
    {
      if( BG_ClientListTest( &ent->client->sess.ignoreList, pids[ i ] ) )
      {
        BG_ClientListRemove( &ent->client->sess.ignoreList, pids[ i ] );
        ClientUserinfoChanged( ent->client->ps.clientNum );
        trap_SendServerCommand( ent-g_entities, va( "print \"[skipnotify]"
          "unignore: removed %s^7 from your ignore list\n\"",
          level.clients[ pids[ i ] ].pers.netname ) );
      }
      else
      {
        trap_SendServerCommand( ent-g_entities, va( "print \"[skipnotify]"
          "unignore: %s^7 is not on your ignore list\n\"",
          level.clients[ pids[ i ] ].pers.netname ) );
      }
    }
  }
}

 /*
 =================
 Cmd_Share_f
 =================
 */
 void Cmd_Share_f( gentity_t *ent )
 {
   int   i, clientNum = 0, creds = 0, skipargs = 0;
   int   clientNums[ MAX_CLIENTS ] = { -1 };
   char  cmd[ 12 ];
   char  arg1[ MAX_STRING_TOKENS ];
   char  arg2[ MAX_STRING_TOKENS ];
   pTeam_t team;
 
   if( !ent || !ent->client || ( ent->client->pers.teamSelection == PTE_NONE ) )
   {
     return;
   }
   
   if( !g_allowShare.integer )
   {
 	  
    trap_SendServerCommand( ent-g_entities,
    "print \"Share has been disabled.\n\"" );
 
     return;
   }

   if( g_floodMinTime.integer &&
     G_Flood_Limited( ent ) )
   {
     trap_SendServerCommand( ent-g_entities, "print \"Your donations are flood-limited; wait before trying again\n\"" );
     return;
   }
 
   team = ent->client->pers.teamSelection;
 
   G_SayArgv( 0, cmd, sizeof( cmd ) );
   if( !Q_stricmp( cmd, "say" ) || !Q_stricmp( cmd, "say_team" ) )
   {
     skipargs = 1;
     G_SayArgv( 1, cmd, sizeof( cmd ) );
   }
 
   // target player name is in arg1
   G_SayArgv( 1+skipargs, arg1, sizeof( arg1 ) );
   // amount to be shared is in arg2
   G_SayArgv( 2+skipargs, arg2, sizeof( arg2 ) );
 
   if( arg1[0] && !strchr( arg1, ';' ) && Q_stricmp( arg1, "target_in_aim" ) )
   {
     //check arg1 is a number
     for( i = 0; arg1[ i ]; i++ )
     {
       if( arg1[ i ] < '0' || arg1[ i ] > '9' )
       {
         clientNum = -1;
         break;
       }
     }
 
     if( clientNum >= 0 )
     {
       clientNum = atoi( arg1 );
     }
     else if( G_ClientNumbersFromString( arg1, clientNums ) == 1 )
     {
       // there was one partial name match
       clientNum = clientNums[ 0 ]; 
     }
     else
     {
       // look for an exact name match before bailing out
       clientNum = G_ClientNumberFromString( ent, arg1 );
       if( clientNum == -1 )
       {
         trap_SendServerCommand( ent-g_entities,
           "print \"share: invalid player name specified.\n\"" );
         return;
       }
     }
   }
   else // arg1 not set
   {
     vec3_t      forward, end;
     trace_t     tr;
     gentity_t   *traceEnt;
 
     
     // trace a teammate
     AngleVectors( ent->client->ps.viewangles, forward, NULL, NULL );
     VectorMA( ent->client->ps.origin, 8192 * 16, forward, end );
 
     trap_Trace( &tr, ent->client->ps.origin, NULL, NULL, end, ent->s.number, MASK_PLAYERSOLID );
     traceEnt = &g_entities[ tr.entityNum ];
 
     if( tr.fraction < 1.0f && traceEnt->client &&
       ( traceEnt->client->pers.teamSelection == team ) )
     {
       clientNum = traceEnt - g_entities;
     }
     else
     {
       trap_SendServerCommand( ent-g_entities,
         va( "print \"share: aim at a teammate to share %s.\n\"",
         ( team == PTE_HUMANS ) ? "credits" : "evolvepoints" ) );
       return;
     }
   }
 
   // verify target player team
   if( ( clientNum < 0 ) || ( clientNum >= level.maxclients ) ||
       ( level.clients[ clientNum ].pers.teamSelection != team ) )
   {
     trap_SendServerCommand( ent-g_entities,
       "print \"share: not a valid player of your team.\n\"" );
     return;
   }
 
   if( !arg2[0] || strchr( arg2, ';' ) )
   {
     // default credit count
     if( team == PTE_HUMANS )
     {
       creds = FREEKILL_HUMAN;
     }
     else if( team == PTE_ALIENS )
     {
       creds = FREEKILL_ALIEN;
     }
   }
   else
   {
     //check arg2 is a number
     for( i = 0; arg2[ i ]; i++ )
     {
       if( arg2[ i ] < '0' || arg2[ i ] > '9' )
       {
         trap_SendServerCommand( ent-g_entities,
           "print \"usage: share [name|slot#] [amount]\n\"" );
         break;
       }
     }
 
     // credit count from parameter
     creds = atoi( arg2 );
   }
 
   // player specified "0" to transfer
   if( creds <= 0 )
   {
     trap_SendServerCommand( ent-g_entities,
       "print \"Ooh, you are a generous one, indeed!\n\"" );
     return;
   }
 
   // transfer only credits the player really has
   if( creds > ent->client->ps.persistant[ PERS_CREDIT ] )
   {
     creds = ent->client->ps.persistant[ PERS_CREDIT ];
   }
 
   // player has no credits
   if( creds <= 0 )
   {
     trap_SendServerCommand( ent-g_entities,
       "print \"Earn some first, lazy gal!\n\"" );
     return;
   }
 
   // allow transfers only up to the credit/evo limit
   if( ( team == PTE_HUMANS ) && 
       ( creds > HUMAN_MAX_CREDITS - level.clients[ clientNum ].ps.persistant[ PERS_CREDIT ] ) )
   {
     creds = HUMAN_MAX_CREDITS - level.clients[ clientNum ].ps.persistant[ PERS_CREDIT ];
   }
   else if( ( team == PTE_ALIENS ) && 
       ( creds > ALIEN_MAX_KILLS - level.clients[ clientNum ].ps.persistant[ PERS_CREDIT ] ) )
   {
     creds = ALIEN_MAX_KILLS - level.clients[ clientNum ].ps.persistant[ PERS_CREDIT ];
   }
 
   // target cannot take any more credits
   if( creds <= 0 )
   {
     trap_SendServerCommand( ent-g_entities,
       va( "print \"share: player cannot receive any more %s.\n\"",
         ( team == PTE_HUMANS ) ? "credits" : "evolvepoints" ) );
     return;
   }
 
   // transfer credits
   ent->client->ps.persistant[ PERS_CREDIT ] -= creds;
   trap_SendServerCommand( ent-g_entities,
     va( "print \"share: transferred %d %s to %s^7.\n\"", creds,
       ( team == PTE_HUMANS ) ? "credits" : "evolvepoints",
       level.clients[ clientNum ].pers.netname ) );
   level.clients[ clientNum ].ps.persistant[ PERS_CREDIT ] += creds;
   trap_SendServerCommand( clientNum,
     va( "print \"You have received %d %s from %s^7.\n\"", creds,
       ( team == PTE_HUMANS ) ? "credits" : "evolvepoints",
       ent->client->pers.netname ) );
 
   G_LogPrintf( "Share: %i %i %i %d: %s^7 transferred %d%s to %s^7\n",
     ent->client->ps.clientNum,
     clientNum,
     team,
     creds,
     ent->client->pers.netname,
     creds,
     ( team == PTE_HUMANS ) ? "c" : "e",
     level.clients[ clientNum ].pers.netname );
 }
 
 /*
 =================
 Cmd_Donate_f
 
 Alms for the poor
 =================
 */
 void Cmd_Donate_f( gentity_t *ent ) {
   char s[ MAX_TOKEN_CHARS ] = "", *type = "evo(s)";
   int i, value, divisor, portion, new_credits, total=0,
     max = ALIEN_MAX_KILLS, *amounts;
   qboolean donated = qtrue;
 
   if( !ent->client ) return;
 	  
   if( !g_allowShare.integer )
   {
 	  
    trap_SendServerCommand( ent-g_entities,
    "print \"Donate has been disabled.\n\"" );
 
     return;
   }

   if( g_floodMinTime.integer &&
     G_Flood_Limited( ent ) )
   {
     trap_SendServerCommand( ent-g_entities, "print \"Your donations are flood-limited; wait before trying again\n\"" );
     return;
   }
 
   if( ent->client->pers.teamSelection == PTE_ALIENS )
     divisor = level.numAlienClients-1;
   else if( ent->client->pers.teamSelection == PTE_HUMANS ) {
     divisor = level.numHumanClients-1;
     max = HUMAN_MAX_CREDITS;
     type = "credit(s)";
   } else {
     trap_SendServerCommand( ent-g_entities,
       va( "print \"donate: spectators cannot be so gracious\n\"" ) );
     return;
   }
 
   if( divisor < 1 ) {
     trap_SendServerCommand( ent-g_entities,
       "print \"donate: get yourself some teammates first\n\"" );
     return;
   }
 
   trap_Argv( 1, s, sizeof( s ) );
   value = atoi(s);
   if( value <= 0 ) {
     trap_SendServerCommand( ent-g_entities,
       "print \"donate: very funny\n\"" );
     return;
   }
   if( value > ent->client->ps.persistant[ PERS_CREDIT ] )
     value = ent->client->ps.persistant[ PERS_CREDIT ];
 
   // allocate memory for distribution amounts
   amounts = G_Alloc( level.maxclients * sizeof( int ) );
   for( i = 0; i < level.maxclients; i++ ) amounts[ i ] = 0;
 
   // determine donation amounts for each client
   total = value;
   while( donated && value ) {
     donated = qfalse;
     portion = value / divisor;
     if( portion < 1 ) portion = 1;
     for( i = 0; i < level.maxclients; i++ )
       if( level.clients[ i ].pers.connected == CON_CONNECTED &&
            ent->client != level.clients + i &&
            level.clients[ i ].pers.teamSelection ==
            ent->client->pers.teamSelection ) {
         new_credits = level.clients[ i ].ps.persistant[ PERS_CREDIT ] + portion;
         amounts[ i ] = portion;
         if( new_credits > max ) {
           amounts[ i ] -= new_credits - max;
           new_credits = max;
         }
         if( amounts[ i ] ) {
           level.clients[ i ].ps.persistant[ PERS_CREDIT ] = new_credits;
           donated = qtrue;
           value -= amounts[ i ];
           if( value < portion ) break;
         }
       }
   }
 
   // transfer funds
   G_AddCreditToClient( ent->client, value - total, qtrue );
   for( i = 0; i < level.maxclients; i++ )
     if( amounts[ i ] ) {
       trap_SendServerCommand( i,
         va( "print \"%s^7 donated %d %s to you, don't forget to say 'thank you'!\n\"",
         ent->client->pers.netname, amounts[ i ], type ) );
     }
 
   G_Free( amounts );
 
   trap_SendServerCommand( ent-g_entities,
     va( "print \"Donated %d %s to the cause.\n\"",
     total-value, type ) );
 }
 

commands_t cmds[ ] = {
  // normal commands
  { "team", 0, Cmd_Team_f },
  { "vote", CMD_INTERMISSION, Cmd_Vote_f },
  { "ignore", 0, Cmd_Ignore_f },
  { "unignore", 0, Cmd_Ignore_f },

  // communication commands
  { "tell", CMD_MESSAGE, Cmd_Tell_f },
  { "callvote", CMD_MESSAGE, Cmd_CallVote_f },
  { "callteamvote", CMD_MESSAGE|CMD_TEAM, Cmd_CallTeamVote_f },
  { "say_area", CMD_MESSAGE|CMD_TEAM, Cmd_SayArea_f },
  // can be used even during intermission
  { "say", CMD_MESSAGE|CMD_INTERMISSION, Cmd_Say_f },
  { "say_team", CMD_MESSAGE|CMD_INTERMISSION, Cmd_Say_f },
  { "say_admins", CMD_MESSAGE|CMD_INTERMISSION, Cmd_Say_f },
  { "a", CMD_MESSAGE|CMD_INTERMISSION, Cmd_Say_f },
  { "m", CMD_MESSAGE|CMD_INTERMISSION, G_PrivateMessage },
  { "mt", CMD_MESSAGE|CMD_INTERMISSION, G_PrivateMessage },
  { "me", CMD_MESSAGE|CMD_INTERMISSION, Cmd_Say_f },
  { "me_team", CMD_MESSAGE|CMD_INTERMISSION, Cmd_Say_f },

  // channel chat
  { "join", CMD_MESSAGE|CMD_INTERMISSION, Cmd_Join_f },
  { "part", CMD_MESSAGE|CMD_INTERMISSION, Cmd_Part_f },
  { "0", CMD_MESSAGE|CMD_INTERMISSION, Cmd_Channel_f },
  { "1", CMD_MESSAGE|CMD_INTERMISSION, Cmd_Channel_f },
  { "2", CMD_MESSAGE|CMD_INTERMISSION, Cmd_Channel_f },
  { "3", CMD_MESSAGE|CMD_INTERMISSION, Cmd_Channel_f },
  { "4", CMD_MESSAGE|CMD_INTERMISSION, Cmd_Channel_f },
  { "5", CMD_MESSAGE|CMD_INTERMISSION, Cmd_Channel_f },
  { "6", CMD_MESSAGE|CMD_INTERMISSION, Cmd_Channel_f },
  { "7", CMD_MESSAGE|CMD_INTERMISSION, Cmd_Channel_f },
  { "8", CMD_MESSAGE|CMD_INTERMISSION, Cmd_Channel_f },
  { "9", CMD_MESSAGE|CMD_INTERMISSION, Cmd_Channel_f },

  { "score", CMD_INTERMISSION, ScoreboardMessage },
  { "mystats", CMD_TEAM|CMD_INTERMISSION, Cmd_MyStats_f },
  { "allstats", 0|CMD_INTERMISSION, Cmd_AllStats_f },
  { "teamstatus", CMD_TEAM, Cmd_TeamStatus_f },


  // cheats
  { "give", CMD_CHEAT|CMD_TEAM|CMD_LIVING, Cmd_Give_f },
  { "god", CMD_CHEAT|CMD_TEAM|CMD_LIVING, Cmd_God_f },
  { "notarget", CMD_CHEAT|CMD_TEAM|CMD_LIVING, Cmd_Notarget_f },
  { "noclip", CMD_CHEAT|CMD_TEAM|CMD_LIVING, Cmd_Noclip_f },
  { "levelshot", CMD_CHEAT, Cmd_LevelShot_f },
  { "setviewpos", CMD_CHEAT, Cmd_SetViewpos_f },
  { "destroy", CMD_CHEAT|CMD_TEAM|CMD_LIVING, Cmd_Destroy_f },

  { "kill", CMD_TEAM|CMD_LIVING, Cmd_Kill_f },

  // game commands
  { "ptrcverify", 0, Cmd_PTRCVerify_f },
  { "ptrcrestore", 0, Cmd_PTRCRestore_f },

  { "follow", CMD_NOTEAM, Cmd_Follow_f },
  { "follownext", CMD_NOTEAM, Cmd_FollowCycle_f },
  { "followprev", CMD_NOTEAM, Cmd_FollowCycle_f },

  { "where", CMD_TEAM, Cmd_Where_f },
  { "teamvote", CMD_TEAM, Cmd_TeamVote_f },
  { "class", CMD_TEAM, Cmd_Class_f },
  { "devolve", CMD_TEAM, Cmd_Devolve_f },
  
  { "build", CMD_TEAM|CMD_LIVING, Cmd_Build_f },
  { "deconstruct", CMD_TEAM|CMD_LIVING, Cmd_Destroy_f },
  { "mark", CMD_TEAM|CMD_LIVING, Cmd_Mark_f },

  { "buy", CMD_HUMAN|CMD_LIVING, Cmd_Buy_f },
  { "sell", CMD_HUMAN|CMD_LIVING, Cmd_Sell_f },
  { "itemact", CMD_HUMAN|CMD_LIVING, Cmd_ActivateItem_f },
  { "itemdeact", CMD_HUMAN|CMD_LIVING, Cmd_DeActivateItem_f },
  { "itemtoggle", CMD_HUMAN|CMD_LIVING, Cmd_ToggleItem_f },
  { "reload", CMD_TEAM|CMD_LIVING, Cmd_Reload_f },
  { "boost", 0, Cmd_Boost_f },
  { "share", CMD_TEAM, Cmd_Share_f },
  { "donate", CMD_TEAM, Cmd_Donate_f },
  { "protect", CMD_TEAM|CMD_LIVING, Cmd_Protect_f },
  { "resign", CMD_TEAM, Cmd_Resign_f },
  { "builder", 0, Cmd_Builder_f }
};
static int numCmds = sizeof( cmds ) / sizeof( cmds[ 0 ] );

/*
=================
ClientCommand
=================
*/
void ClientCommand( int clientNum )
{
  gentity_t *ent;
  char      cmd[ MAX_TOKEN_CHARS ];
  int       i;

  ent = g_entities + clientNum;
  if( !ent->client )
    return;   // not fully in game yet

  trap_Argv( 0, cmd, sizeof( cmd ) );

  for( i = 0; i < numCmds; i++ )
  {
    if( Q_stricmp( cmd, cmds[ i ].cmdName ) == 0 )
      break;
  }

  if( i == numCmds )
  {
    if( !G_admin_cmd_check( ent, qfalse ) )
      trap_SendServerCommand( clientNum,
        va( "print \"Unknown command %s\n\"", cmd ) );
    return;
  }

  // do tests here to reduce the amount of repeated code

  if( !( cmds[ i ].cmdFlags & CMD_INTERMISSION ) && ( level.intermissiontime || level.paused ) )
    return;

  if( cmds[ i ].cmdFlags & CMD_CHEAT && !g_cheats.integer )
  {
    trap_SendServerCommand( clientNum,
      "print \"Cheats are not enabled on this server\n\"" );
    return;
  }

  if( cmds[ i ].cmdFlags & CMD_MESSAGE && ent->client->pers.muted )
  {
    trap_SendServerCommand( clientNum,
      "print \"You are muted and cannot use message commands.\n\"" );
    return;
  }

  if( cmds[ i ].cmdFlags & CMD_TEAM &&
      ent->client->pers.teamSelection == PTE_NONE )
  {
    trap_SendServerCommand( clientNum, "print \"Join a team first\n\"" );
    return;
  }

  if( cmds[ i ].cmdFlags & CMD_NOTEAM &&
      ent->client->pers.teamSelection != PTE_NONE )
  {
    trap_SendServerCommand( clientNum,
      "print \"Cannot use this command when on a team\n\"" );
    return;
  }

  if( cmds[ i ].cmdFlags & CMD_ALIEN &&
      ent->client->pers.teamSelection != PTE_ALIENS )
  {
    trap_SendServerCommand( clientNum,
      "print \"Must be alien to use this command\n\"" );
    return;
  }

  if( cmds[ i ].cmdFlags & CMD_HUMAN &&
      ent->client->pers.teamSelection != PTE_HUMANS )
  {
    trap_SendServerCommand( clientNum,
      "print \"Must be human to use this command\n\"" );
    return;
  }

  if( cmds[ i ].cmdFlags & CMD_LIVING &&
    ( ent->client->ps.stats[ STAT_HEALTH ] <= 0 ||
      ent->client->sess.sessionTeam == TEAM_SPECTATOR ) )
  {
    trap_SendServerCommand( clientNum,
      "print \"Must be living to use this command\n\"" );
    return;
  }

  cmds[ i ].cmdHandler( ent );
}

int G_SayArgc()
{
  int c = 1;
  char *s;

  s = ConcatArgs( 0 );
  if( !*s )
     return 0;
  while( *s )
  {
    if( *s == ' ' )
    {
      s++; 
      if( *s != ' ' )
      {
        c++;
        continue;
      }
      while( *s && *s == ' ' )
        s++;
      c++;
    }
    if( *s )
      s++;
  }
  return c;
}

qboolean G_SayArgv( int n, char *buffer, int bufferLength )
{
  int bc = 1;
  int c = 0;
  char *s;

  if( bufferLength < 1 )
    return qfalse;
  if(n < 0)
    return qfalse;
  *buffer = '\0';
  s = ConcatArgs( 0 );
  while( *s )
  {
    if( c == n )
    {
      while( *s && ( bc < bufferLength ) )
      {
        if( *s == ' ' )
        {
          *buffer = '\0';
          return qtrue;
        }
        *buffer = *s;
        buffer++;
        s++;
        bc++;
      }
      *buffer = '\0';
      return qtrue;
    }
    if( *s == ' ' )
    {
      s++;
      if( *s != ' ' )
      {
        c++;
        continue;
      }
      while( *s && *s == ' ' )
        s++;
      c++;
    }
    if( *s )
      s++;
  }
  return qfalse;
}

char *G_SayConcatArgs(int start)
{
  char *s;
  int c = 0;

  s = ConcatArgs( 0 );
  while( *s ) {
    if( c == start )
      return s;
    if( *s == ' ' )
    {
      s++;
      if( *s != ' ' )
      {
        c++;
        continue;
      }
      while( *s && *s == ' ' )
        s++;
      c++;
    }
    if( *s )
      s++;
  }
  return s;
}

void G_DecolorString( char *in, char *out )
{   
  while( *in ) {
    if( *in == 27 || *in == '^' ) {
      in++;
      if( *in )
        in++;
      continue;
    }
    *out++ = *in++;
  }
  *out = '\0';
}

void G_ParseEscapedString( char *buffer )
{
  int i = 0;
  int j = 0;

  while( buffer[i] )
  {
    if(!buffer[i]) break;

    if(buffer[i] == '\\')
    {
      if(buffer[i + 1] == '\\')
        buffer[j] = buffer[++i];
      else if(buffer[i + 1] == 'n')
      {
        buffer[j] = '\n';
        i++;
      }
      else
        buffer[j] = buffer[i];
    }
    else
      buffer[j] = buffer[i];

    i++;
    j++;
  }
  buffer[j] = 0;
}

void G_PrivateMessage( gentity_t *ent )
{
  int pids[ MAX_CLIENTS ];
  int ignoreids[ MAX_CLIENTS ];
  char name[ MAX_NAME_LENGTH ];
  char cmd[ 12 ];
  char str[ MAX_STRING_CHARS ];
  char *msg;
  char color;
  int pcount, matches, ignored = 0;
  int i;
  int skipargs = 0;
  qboolean teamonly = qfalse;
  gentity_t *tmpent;

  if( !g_privateMessages.integer && ent )
  {
    ADMP( "Sorry, but private messages have been disabled\n" );
    return;
  }
  
  if( g_floodMinTime.integer )
   if ( G_Flood_Limited( ent ) )
   {
    trap_SendServerCommand( ent-g_entities, "print \"Your chat is flood-limited; wait before chatting again\n\"" );
    return;
   }

  G_SayArgv( 0, cmd, sizeof( cmd ) );
  if( !Q_stricmp( cmd, "say" ) || !Q_stricmp( cmd, "say_team" ) )
  {
    skipargs = 1;
    G_SayArgv( 1, cmd, sizeof( cmd ) );
  }
  if( G_SayArgc( ) < 3+skipargs )
  {
    ADMP( va( "usage: %s [name|slot#] [message]\n", cmd ) );
    return;
  }

  if( !Q_stricmp( cmd, "mt" ) || !Q_stricmp( cmd, "/mt" ) )
    teamonly = qtrue;

  G_SayArgv( 1+skipargs, name, sizeof( name ) );
  msg = G_SayConcatArgs( 2+skipargs );
  pcount = G_ClientNumbersFromString( name, pids );

  if( ent )
  {
    int count = 0;

    for( i=0; i < pcount; i++ )
    {
      tmpent = &g_entities[ pids[ i ] ];

      if( teamonly && !OnSameTeam( ent, tmpent ) )
        continue;

      if( BG_ClientListTest( &tmpent->client->sess.ignoreList,
        ent-g_entities ) )
      {
        ignoreids[ ignored++ ] = pids[ i ];
        continue;
      }

      pids[ count ] = pids[ i ];
      count++;
    }
    matches = count;
  }
  else
  {
    matches = pcount;
  }

  color = teamonly ? COLOR_CYAN : COLOR_YELLOW;

  if( !Q_stricmp( name, "console" ) )
  {
    ADMP( va( "^%cPrivate message: ^7%s\n", color, msg ) );
    ADMP( va( "^%csent to Console.\n", color ) );

    G_LogPrintf( "privmsg: %s^7: Console: ^6%s^7\n",
      ( ent ) ? ent->client->pers.netname : "Console", msg );

    return;
  }

  Q_strncpyz( str,
    va( "^%csent to %i player%s: ^7", color, matches,
      ( matches == 1 ) ? "" : "s" ),
    sizeof( str ) );

  for( i=0; i < matches; i++ )
  {
    tmpent = &g_entities[ pids[ i ] ];

    if( i > 0 )
      Q_strcat( str, sizeof( str ), "^7, " );
    Q_strcat( str, sizeof( str ), tmpent->client->pers.netname );
    trap_SendServerCommand( pids[ i ], va(
      "chat \"%s^%c -> ^7%s^7: (%d recipients): ^%c%s^7\" %i",
      ( ent ) ? ent->client->pers.netname : "console",
      color,
      name,
      matches,
      color,
      msg,
      ent ? ent-g_entities : -1 ) );

    trap_SendServerCommand( pids[ i ], va( 
      "cp \"^%cprivate message from ^7%s^7\"", color,
      ( ent ) ? ent->client->pers.netname : "console" ) );
  }

  if( !matches )
    ADMP( va( "^3No player matching ^7\'%s^7\' ^3to send message to.\n",
      name ) );
  else
  {
    if( ent )
      ADMP( va( "^%cPrivate message: ^7%s\n", color, msg ) );

    ADMP( va( "%s\n", str ) );

    G_LogPrintf( "%s: %s^7: %s^7: %s^7\n",
      ( teamonly ) ? "tprivmsg" : "privmsg",
      ( ent ) ? ent->client->pers.netname : "console",
      name, msg );
  }

  if( ignored )
  {
    Q_strncpyz( str, va( "^%cignored by %i player%s: ^7", color, ignored,
      ( ignored == 1 ) ? "" : "s" ), sizeof( str ) );
    for( i=0; i < ignored; i++ )
    {
      tmpent = &g_entities[ ignoreids[ i ] ];
      if( i > 0 )
        Q_strcat( str, sizeof( str ), "^7, " );
      Q_strcat( str, sizeof( str ), tmpent->client->pers.netname );
    }
    ADMP( va( "%s\n", str ) );
  }
}

