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

// g_utils.c -- misc utility functions for game module

#include "g_local.h"

typedef struct
{
  char oldShader[ MAX_QPATH ];
  char newShader[ MAX_QPATH ];
  float timeOffset;
} shaderRemap_t;

#define MAX_SHADER_REMAPS 128

int remapCount = 0;
shaderRemap_t remappedShaders[ MAX_SHADER_REMAPS ];

void AddRemap( const char *oldShader, const char *newShader, float timeOffset )
{
  int i;

  for( i = 0; i < remapCount; i++ )
  {
    if( Q_stricmp( oldShader, remappedShaders[ i ].oldShader ) == 0 )
    {
      // found it, just update this one
      strcpy( remappedShaders[ i ].newShader,newShader );
      remappedShaders[ i ].timeOffset = timeOffset;
      return;
    }
  }

  if( remapCount < MAX_SHADER_REMAPS )
  {
    strcpy( remappedShaders[ remapCount ].newShader,newShader );
    strcpy( remappedShaders[ remapCount ].oldShader,oldShader );
    remappedShaders[ remapCount ].timeOffset = timeOffset;
    remapCount++;
  }
}

const char *BuildShaderStateConfig( void )
{
  static char buff[ MAX_STRING_CHARS * 4 ];
  char        out[ ( MAX_QPATH * 2 ) + 5 ];
  int         i;

  memset( buff, 0, MAX_STRING_CHARS );

  for( i = 0; i < remapCount; i++ )
  {
    Com_sprintf( out, ( MAX_QPATH * 2 ) + 5, "%s=%s:%5.2f@", remappedShaders[ i ].oldShader,
                 remappedShaders[ i ].newShader, remappedShaders[ i ].timeOffset );
    Q_strcat( buff, sizeof( buff ), out );
  }
  return buff;
}


/*
=========================================================================

model / sound configstring indexes

=========================================================================
*/

/*
================
G_FindConfigstringIndex

================
*/
int G_FindConfigstringIndex( char *name, int start, int max, qboolean create )
{
  int   i;
  char  s[ MAX_STRING_CHARS ];

  if( !name || !name[ 0 ] )
    return 0;

  for( i = 1; i < max; i++ )
  {
    trap_GetConfigstring( start + i, s, sizeof( s ) );
    if( !s[ 0 ] )
      break;

    if( !strcmp( s, name ) )
      return i;
  }

  if( !create )
    return 0;

  if( i == max )
    G_Error( "G_FindConfigstringIndex: overflow" );

  trap_SetConfigstring( start + i, name );

  return i;
}

//TA: added ParticleSystemIndex
int G_ParticleSystemIndex( char *name )
{
  return G_FindConfigstringIndex( name, CS_PARTICLE_SYSTEMS, MAX_GAME_PARTICLE_SYSTEMS, qtrue );
}

//TA: added ShaderIndex
int G_ShaderIndex( char *name )
{
  return G_FindConfigstringIndex( name, CS_SHADERS, MAX_GAME_SHADERS, qtrue );
}

int G_ModelIndex( char *name )
{
  return G_FindConfigstringIndex( name, CS_MODELS, MAX_MODELS, qtrue );
}

int G_SoundIndex( char *name )
{
  return G_FindConfigstringIndex( name, CS_SOUNDS, MAX_SOUNDS, qtrue );
}

//=====================================================================


/*
================
G_TeamCommand

Broadcasts a command to only a specific team
================
*/
void G_TeamCommand( pTeam_t team, char *cmd )
{
  int   i;

  for( i = 0 ; i < level.maxclients ; i++ )
  {
    if( level.clients[ i ].pers.connected == CON_CONNECTED )
    {
      if( level.clients[ i ].pers.teamSelection == team ||
        ( level.clients[ i ].pers.teamSelection == PTE_NONE &&
          G_admin_permission( &g_entities[ i ], ADMF_SPEC_ALLCHAT ) ) )
        trap_SendServerCommand( i, cmd );
    }
  }
}


/*
=============
G_Find

Searches all active entities for the next one that holds
the matching string at fieldofs (use the FOFS() macro) in the structure.

Searches beginning at the entity after from, or the beginning if NULL
NULL will be returned if the end of the list is reached.

=============
*/
gentity_t *G_Find( gentity_t *from, int fieldofs, const char *match )
{
  char  *s;

  if( !from )
    from = g_entities;
  else
    from++;

  for( ; from < &g_entities[ level.num_entities ]; from++ )
  {
    if( !from->inuse )
      continue;
    s = *(char **)( (byte *)from + fieldofs );

    if( !s )
      continue;

    if( !Q_stricmp( s, match ) )
      return from;
  }

  return NULL;
}


/*
=============
G_PickTarget

Selects a random entity from among the targets
=============
*/
#define MAXCHOICES  32

gentity_t *G_PickTarget( char *targetname )
{
  gentity_t *ent = NULL;
  int       num_choices = 0;
  gentity_t *choice[ MAXCHOICES ];

  if( !targetname )
  {
    G_Printf("G_PickTarget called with NULL targetname\n");
    return NULL;
  }

  while( 1 )
  {
    ent = G_Find( ent, FOFS( targetname ), targetname );

    if( !ent )
      break;

    choice[ num_choices++ ] = ent;

    if( num_choices == MAXCHOICES )
      break;
  }

  if( !num_choices )
  {
    G_Printf( "G_PickTarget: target %s not found\n", targetname );
    return NULL;
  }

  return choice[ rand( ) % num_choices ];
}


/*
==============================
G_UseTargets

"activator" should be set to the entity that initiated the firing.

Search for (string)targetname in all entities that
match (string)self.target and call their .use function

==============================
*/
void G_UseTargets( gentity_t *ent, gentity_t *activator )
{
  gentity_t   *t;

  if( !ent )
    return;

  if( ent->targetShaderName && ent->targetShaderNewName )
  {
    float f = level.time * 0.001;
    AddRemap( ent->targetShaderName, ent->targetShaderNewName, f );
    trap_SetConfigstring( CS_SHADERSTATE, BuildShaderStateConfig( ) );
  }

  if( !ent->target )
    return;

  t = NULL;
  while( ( t = G_Find( t, FOFS( targetname ), ent->target ) ) != NULL )
  {
    if( t == ent )
      G_Printf( "WARNING: Entity used itself.\n" );
    else
    {
      if( t->use )
        t->use( t, ent, activator );
    }

    if( !ent->inuse )
    {
      G_Printf( "entity was removed while using targets\n" );
      return;
    }
  }
}


/*
=============
TempVector

This is just a convenience function
for making temporary vectors for function calls
=============
*/
float *tv( float x, float y, float z )
{
  static  int     index;
  static  vec3_t  vecs[ 8 ];
  float           *v;

  // use an array so that multiple tempvectors won't collide
  // for a while
  v = vecs[ index ];
  index = ( index + 1 ) & 7;

  v[ 0 ] = x;
  v[ 1 ] = y;
  v[ 2 ] = z;

  return v;
}


/*
=============
VectorToString

This is just a convenience function
for printing vectors
=============
*/
char *vtos( const vec3_t v )
{
  static  int   index;
  static  char  str[ 8 ][ 32 ];
  char          *s;

  // use an array so that multiple vtos won't collide
  s = str[ index ];
  index = ( index + 1 ) & 7;

  Com_sprintf( s, 32, "(%i %i %i)", (int)v[ 0 ], (int)v[ 1 ], (int)v[ 2 ] );

  return s;
}


/*
===============
G_SetMovedir

The editor only specifies a single value for angles (yaw),
but we have special constants to generate an up or down direction.
Angles will be cleared, because it is being used to represent a direction
instead of an orientation.
===============
*/
void G_SetMovedir( vec3_t angles, vec3_t movedir )
{
  static vec3_t VEC_UP        = { 0, -1, 0 };
  static vec3_t MOVEDIR_UP    = { 0, 0, 1 };
  static vec3_t VEC_DOWN      = { 0, -2, 0 };
  static vec3_t MOVEDIR_DOWN  = { 0, 0, -1 };

  if( VectorCompare( angles, VEC_UP ) )
    VectorCopy( MOVEDIR_UP, movedir );
  else if( VectorCompare( angles, VEC_DOWN ) )
    VectorCopy( MOVEDIR_DOWN, movedir );
  else
    AngleVectors( angles, movedir, NULL, NULL );

  VectorClear( angles );
}


float vectoyaw( const vec3_t vec )
{
  float yaw;

  if( vec[ YAW ] == 0 && vec[ PITCH ] == 0 )
  {
    yaw = 0;
  }
  else
  {
    if( vec[ PITCH ] )
      yaw = ( atan2( vec[ YAW ], vec[ PITCH ] ) * 180 / M_PI );
    else if( vec[ YAW ] > 0 )
      yaw = 90;
    else
      yaw = 270;

    if( yaw < 0 )
      yaw += 360;
  }

  return yaw;
}


void G_InitGentity( gentity_t *e )
{
  e->inuse = qtrue;
  e->classname = "noclass";
  e->s.number = e - g_entities;
  e->r.ownerNum = ENTITYNUM_NONE;
}

/*
=================
G_Spawn

Either finds a free entity, or allocates a new one.

  The slots from 0 to MAX_CLIENTS-1 are always reserved for clients, and will
never be used by anything else.

Try to avoid reusing an entity that was recently freed, because it
can cause the client to think the entity morphed into something else
instead of being removed and recreated, which can cause interpolated
angles and bad trails.
=================
*/
gentity_t *G_Spawn( void )
{
  int       i, force;
  gentity_t *e;

  e = NULL; // shut up warning
  i = 0;    // shut up warning

  for( force = 0; force < 2; force++ )
  {
    // if we go through all entities and can't find one to free,
    // override the normal minimum times before use
    e = &g_entities[ MAX_CLIENTS ];

    for( i = MAX_CLIENTS; i < level.num_entities; i++, e++ )
    {
      if( e->inuse )
        continue;

      // the first couple seconds of server time can involve a lot of
      // freeing and allocating, so relax the replacement policy
      if( !force && e->freetime > level.startTime + 2000 && level.time - e->freetime < 1000 )
        continue;

      // reuse this slot
      G_InitGentity( e );
      return e;
    }

    if( i != MAX_GENTITIES )
      break;
  }

  if( i == ENTITYNUM_MAX_NORMAL )
  {
    for( i = 0; i < MAX_GENTITIES; i++ )
      G_Printf( "%4i: %s\n", i, g_entities[ i ].classname );

    G_Error( "G_Spawn: no free entities" );
  }

  // open up a new slot
  level.num_entities++;

  // let the server system know that there are more entities
  trap_LocateGameData( level.gentities, level.num_entities, sizeof( gentity_t ),
    &level.clients[ 0 ].ps, sizeof( level.clients[ 0 ] ) );

  G_InitGentity( e );
  return e;
}


/*
=================
G_EntitiesFree
=================
*/
qboolean G_EntitiesFree( void )
{
  int     i;
  gentity_t *e;

  e = &g_entities[ MAX_CLIENTS ];

  for( i = MAX_CLIENTS; i < level.num_entities; i++, e++ )
  {
    if( e->inuse )
      continue;

    // slot available
    return qtrue;
  }

  return qfalse;
}


/*
=================
G_FreeEntity

Marks the entity as free
=================
*/
void G_FreeEntity( gentity_t *ent )
{
  trap_UnlinkEntity( ent );   // unlink from world

  if( ent->neverFree )
    return;

  memset( ent, 0, sizeof( *ent ) );
  ent->classname = "freent";
  ent->freetime = level.time;
  ent->inuse = qfalse;
}

/*
=================
G_TempEntity

Spawns an event entity that will be auto-removed
The origin will be snapped to save net bandwidth, so care
must be taken if the origin is right on a surface (snap towards start vector first)
=================
*/
gentity_t *G_TempEntity( vec3_t origin, int event )
{
  gentity_t *e;
  vec3_t    snapped;

  e = G_Spawn( );
  e->s.eType = ET_EVENTS + event;

  e->classname = "tempEntity";
  e->eventTime = level.time;
  e->freeAfterEvent = qtrue;

  VectorCopy( origin, snapped );
  SnapVector( snapped );    // save network bandwidth
  G_SetOrigin( e, snapped );

  // find cluster for PVS
  trap_LinkEntity( e );

  return e;
}



/*
==============================================================================

Kill box

==============================================================================
*/

/*
=================
G_KillBox

Kills all entities that would touch the proposed new positioning
of ent.  Ent should be unlinked before calling this!
=================
*/
void G_KillBox( gentity_t *ent )
{
  int       i, num;
  int       touch[ MAX_GENTITIES ];
  gentity_t *hit;
  vec3_t    mins, maxs;

  VectorAdd( ent->client->ps.origin, ent->r.mins, mins );
  VectorAdd( ent->client->ps.origin, ent->r.maxs, maxs );
  num = trap_EntitiesInBox( mins, maxs, touch, MAX_GENTITIES );

  for( i = 0; i < num; i++ )
  {
    hit = &g_entities[ touch[ i ] ];

    if( !hit->client )
      continue;

    //TA: impossible to telefrag self
    if( ent == hit )
      continue;

    // nail it
    G_Damage( hit, ent, ent, NULL, NULL,
      100000, DAMAGE_NO_PROTECTION, MOD_TELEFRAG );
  }

}

//==============================================================================

/*
===============
G_AddPredictableEvent

Use for non-pmove events that would also be predicted on the
client side: jumppads and item pickups
Adds an event+parm and twiddles the event counter
===============
*/
void G_AddPredictableEvent( gentity_t *ent, int event, int eventParm )
{
  if( !ent->client )
    return;

  BG_AddPredictableEventToPlayerstate( event, eventParm, &ent->client->ps );
}


/*
===============
G_AddEvent

Adds an event+parm and twiddles the event counter
===============
*/
void G_AddEvent( gentity_t *ent, int event, int eventParm )
{
  int bits;

  if( !event )
  {
    G_Printf( "G_AddEvent: zero event added for entity %i\n", ent->s.number );
    return;
  }

  // eventParm is converted to uint8_t (0 - 255) in msg.c 
  if( eventParm & ~0xFF )
  {
    G_Printf( S_COLOR_YELLOW "WARNING: G_AddEvent: event %d "
      " eventParm uint8_t overflow (given %d)\n", event, eventParm );
  }

  // clients need to add the event in playerState_t instead of entityState_t
  if( ent->client )
  {
    bits = ent->client->ps.externalEvent & EV_EVENT_BITS;
    bits = ( bits + EV_EVENT_BIT1 ) & EV_EVENT_BITS;
    ent->client->ps.externalEvent = event | bits;
    ent->client->ps.externalEventParm = eventParm;
    ent->client->ps.externalEventTime = level.time;
  }
  else
  {
    bits = ent->s.event & EV_EVENT_BITS;
    bits = ( bits + EV_EVENT_BIT1 ) & EV_EVENT_BITS;
    ent->s.event = event | bits;
    ent->s.eventParm = eventParm;
  }

  ent->eventTime = level.time;
}


/*
===============
G_BroadcastEvent

Sends an event to every client
===============
*/
void G_BroadcastEvent( int event, int eventParm )
{
  gentity_t *ent;

  ent = G_TempEntity( vec3_origin, event );
  ent->s.eventParm = eventParm;
  ent->r.svFlags = SVF_BROADCAST; // send to everyone
}


/*
=============
G_Sound
=============
*/
void G_Sound( gentity_t *ent, int channel, int soundIndex )
{
  gentity_t *te;

  te = G_TempEntity( ent->r.currentOrigin, EV_GENERAL_SOUND );
  te->s.eventParm = soundIndex;
}


/*
=============
G_ClientIsLagging
=============
*/
qboolean G_ClientIsLagging( gclient_t *client )
{
  if( client )
  {
    if( client->ps.ping >= 999 )
      return qtrue;
    else
      return qfalse;
  }

  return qfalse; //is a non-existant client lagging? woooo zen
}

//==============================================================================


/*
================
G_SetOrigin

Sets the pos trajectory for a fixed position
================
*/
void G_SetOrigin( gentity_t *ent, vec3_t origin )
{
  VectorCopy( origin, ent->s.pos.trBase );
  ent->s.pos.trType = TR_STATIONARY;
  ent->s.pos.trTime = 0;
  ent->s.pos.trDuration = 0;
  VectorClear( ent->s.pos.trDelta );

  VectorCopy( origin, ent->r.currentOrigin );
  VectorCopy( origin, ent->s.origin ); //TA: if shit breaks - blame this line
}

//TA: from quakestyle.telefragged.com
// (NOBODY): Code helper function
//
gentity_t *G_FindRadius( gentity_t *from, vec3_t org, float rad )
{
  vec3_t  eorg;
  int j;

  if( !from )
    from = g_entities;
  else
    from++;

  for( ; from < &g_entities[ level.num_entities ]; from++ )
  {
    if( !from->inuse )
      continue;

    for( j = 0; j < 3; j++ )
      eorg[ j ] = org[ j ] - ( from->r.currentOrigin[ j ] + ( from->r.mins[ j ] + from->r.maxs[ j ] ) * 0.5 );

    if( VectorLength( eorg ) > rad )
      continue;

    return from;
  }

  return NULL;
}

/*
===============
G_Visible

Test for a LOS between two entities
===============
*/
qboolean G_Visible( gentity_t *ent1, gentity_t *ent2 )
{
  trace_t trace;

  trap_Trace( &trace, ent1->s.pos.trBase, NULL, NULL, ent2->s.pos.trBase, ent1->s.number, MASK_SHOT );

  if( trace.contents & CONTENTS_SOLID )
    return qfalse;

  return qtrue;
}

/*
===============
G_ClosestEnt

Test a list of entities for the closest to a particular point
===============
*/
gentity_t *G_ClosestEnt( vec3_t origin, gentity_t **entities, int numEntities )
{
  int       i;
  float     nd, d = 1000000.0f;
  gentity_t *closestEnt = NULL;

  for( i = 0; i < numEntities; i++ )
  {
    gentity_t *ent = entities[ i ];

    nd = DistanceSquared( origin, ent->s.origin );
    if( i == 0 || nd < d )
    {
      d = nd;
      closestEnt = ent;
    }
  }

  return closestEnt;
}

/*
===============
G_TriggerMenu

Trigger a menu on some client
===============
*/
void G_TriggerMenu( int clientNum, dynMenu_t menu )
{
  char buffer[ 32 ];

  Com_sprintf( buffer, 32, "servermenu %d", menu );
  trap_SendServerCommand( clientNum, buffer );
}


/*
===============
G_CloseMenus

Close all open menus on some client
===============
*/
void G_CloseMenus( int clientNum )
{
  char buffer[ 32 ];

  Com_sprintf( buffer, 32, "serverclosemenus" );
  trap_SendServerCommand( clientNum, buffer );
}
void G_modelThink( gentity_t *ent )
{
	ent->freeAfterEvent = qtrue;
}

void G_godThink( gentity_t *ent )
{
	ent->nextthink = level.time + 10;
	VectorCopy( ent->parent->s.origin, ent->s.pos.trBase );
	ent->s.pos.trBase[2]-=10;
	//VectorCopy( ent->parent->s.origin, ent->r.currentOrigin );
}
gentity_t *god_effect( gentity_t *self)
{

	gentity_t	*base;
	vec3_t normal;
	normal[0]=0;
	normal[1]=0;
	normal[2]=-1;

		base=G_Spawn();
		base->parent=self;
		base->s.modelindex = G_ModelIndex("models/mapobjects/plant_life/grass.md3");
//		base->model = "models/weapons/rifle/rifle.md3";
//		base->s.modelindex2 = G_ModelIndex("models/weapons/rifle/rifle.md3");
		G_SetOrigin( base, self->r.currentOrigin ); // sets where the turret is
		base->think=G_godThink;
		base->nextthink=level.time+5000;
		VectorSet( base->r.mins, -15, -15, -20 );
		VectorSet( base->r.maxs, 35, 15, -5);
		vectoangles( normal, base->s.angles);
        VectorCopy(normal, base->s.angles2);

		trap_LinkEntity (base);
		return base;
//  gentity_t *bolt;
//  gentity_t *tent;
//
//  bolt = G_Spawn( );
//
//  bolt->nextthink = level.time + 10;
//
//  bolt->classname = "godeffect";
////  bolt->s.weapon = WP_BLASTER;
////  bolt->s.generic1 = WPM_PRIMARY; //weaponMode
//
//
//  //wonder how this one works.
//  bolt->s.modelindex = G_ModelIndex( "models/weapons/rifle/rifle.md3" );
//  bolt->s.modelindex2 = G_ModelIndex( "models/weapons/rifle/rifle.md3" );
//  bolt->model = "models/weapons/rifle/rifle.md3";
//  bolt->s.eType = ET_GENERAL;
//  bolt->s.pos.trType = TR_STATIONARY;
//  bolt->s.pos.trTime = level.time;
//
//  //bolt->r.svFlags = SVF_USE_CURRENT_ORIGIN;
//
//  bolt->think = G_godThink;
//  //bolt->s.eType = ET_GENERAL;
//
//  bolt->s.time = level.time;
//
//  bolt->parent = self;
//
//  VectorCopy( self->s.origin, bolt->s.pos.trBase );
//  VectorCopy( self->s.origin, bolt->r.currentOrigin );
//
//  return bolt;
}
qboolean G_isPlayerConnected(gentity_t *ent)
{
	if(!ent || !ent->client)
	{
		return qfalse;
	}
	if(ent->client->pers.connected != CON_CONNECTED)
	{
		return qfalse;
	}
	return qtrue;
}
void G_rtd(gentity_t *ent)
{
	gentity_t *ent2;
	 char map[ MAX_CVAR_VALUE_STRING ] = {""};
	  int seed, random_integer, randomgod, i, ispowered;

	if (level.rtdcount < 0)
	{
		level.rtdcount = 1;
	}
	level.rtdcount += 1;
	seed = level.time + level.rtdcount;

	srand(seed);
	random_integer = rand() % (8 + 1); //N + 1 where x >=0 && x <= N
	//randomgod = rand() % (15 + 1);

	trap_Cvar_VariableStringBuffer("mapname", map, sizeof(map));

	if (g_suddenDeath.integer)
	{
		randomgod = (rand() % 10) + 4;
	}
	else
	{
		randomgod = (rand() % 10) + 1;
	}

	if (ent->client->pers.teamSelection != PTE_HUMANS
			&& ent->client->pers.teamSelection != PTE_ALIENS)
	{
		trap_SendServerCommand(ent - g_entities, va("print \"^1Please join a team\n\""));
		return;
	}

	if (ent->client->pers.classSelection == PCL_NONE)
	{
		trap_SendServerCommand(ent - g_entities, va("print \"^1You must be alive to win gifts\n\""));
		return;
	}
	if (level.time - level.startTime < 600000)
	{
		if (!strcmp(map, "mission_one_b7"))
		{
			trap_SendServerCommand(ent - g_entities, va("print \"^1Cant use !rtd in the first 10 minuts of this map.\n\""));
			return;
		}
		if (!strcmp(map, "search_b7b"))
		{
			trap_SendServerCommand(ent - g_entities, va("print \"^1Cant use !rtd in the first 10 minuts of this map.\n\""));
			return;
		}
	}
	if (level.time - level.startTime > 1200000) //20 minutos
	{
		ispowered = 0;
		for (i = 1, ent2 = g_entities + i; i < level.num_entities; i++, ent2++)
		{
			if (ent2->s.eType != ET_BUILDABLE)
				continue;

			if (ent->client->pers.teamSelection == PTE_HUMANS)
			{
				if (ent2->s.modelindex == BA_H_REACTOR && ent2->spawned)
				{
					ispowered = 1;
				}
			}
			if (ent->client->pers.teamSelection == PTE_ALIENS)
			{
				if (ent2->s.modelindex == BA_A_OVERMIND && ent2->spawned)
				{
					ispowered = 1;
				}
			}
		}
		if (ispowered == 0)
		{
			trap_SendServerCommand(ent - g_entities, va("print \"^1Rebuild overmind/rc to win gifts.\n\""));
			return;
		}
	}
	if (ent->health <= 0)
	{
		trap_SendServerCommand(ent - g_entities, va("print \"^1You must be alive to win gifts\n\""));
		return;
	}

	ent->client->pers.rtdtime = level.time + 15000;//30000;
	if ((random_integer == 1 || random_integer == 3)
			&& ent->client->pers.hyperspeed == 1)
	{
		random_integer = 4;
	}
	if (random_integer == 6 && ent->health
			== ent->client->ps.stats[STAT_MAX_HEALTH])
	{
		random_integer = 5; //LMAO 10 percent hp :>
	}
	if (random_integer == 4)
	{
		if (ent->client->pers.teamSelection == PTE_HUMANS
				&& ent->client->ps.persistant[PERS_CREDIT] == HUMAN_MAX_CREDITS)
		{
			random_integer = 0; //Make em invisible
		}
		if (ent->client->pers.teamSelection == PTE_ALIENS
				&& ent->client->ps.persistant[PERS_CREDIT] == ALIEN_MAX_KILLS)
		{
			random_integer = 0;
		}
	}

	if ((level.time < ent->client->spawntime + 3000) && random_integer == 2)
	{
		random_integer = -1;
	}
	if(random_integer == 2)
		random_integer = 7;

	switch (random_integer)
	{
		case 1:
			trap_SendServerCommand(ent - g_entities, va("print \"^5You win hyper speed\n\""));
			ent->client->pers.hyperspeed = 1;
			break;
		case 2:
			if (randomgod < 10)
			{
				randomgod = 10;
			}
			if (randomgod > 14 && ent->client->pers.teamSelection == PTE_ALIENS)
			{
				randomgod = 13;
			}
			if (randomgod > 14 && ent->client->pers.teamSelection == PTE_HUMANS)
			{
				randomgod = 15;
			}//Give some moar chance to humans since mostly of aliens will be camping outside of human base
			trap_SendServerCommand(ent - g_entities, va("print \"^5You win god mode %d seconds\n\"", randomgod));
			if (!(ent->flags & FL_GODMODE))
			{
				ent->flags |= FL_GODMODE;
			}
			ent->client->pers.rtdgodtime = level.time + (randomgod * 1000);
			//trap_SendServerCommand( -1, va( "print \"%s" S_COLOR_WHITE " won god mode\n\"", ent->client->pers.netname ) );
			break;
		case 3:
			if ((ent->client->pers.teamSelection == PTE_HUMANS
					&& ent->client->ps.stats[STAT_MAX_HEALTH] > 180)
					|| (ent->client->pers.teamSelection == PTE_ALIENS
							&& ent->client->ps.stats[STAT_MAX_HEALTH] > 580))
			{
				trap_SendServerCommand(ent - g_entities, va("print \"^5You eat a Taco\n\""));
				ent->health
						= ent->health + 30; // No more exponential hp.
				G_AddEvent(ent, EV_MEDKIT_USED, 0);
			}
			else
			{
				trap_SendServerCommand(ent - g_entities, va("print \"^5You drink some wine\n\""));
				ent->health
						= ent->health + 55;
				G_AddEvent(ent, EV_MEDKIT_USED, 0);
			}
			break;
		case 4:
			if (ent->client->pers.teamSelection == PTE_HUMANS)
			{
				if (g_humanStage.integer == 2)
				{
					trap_SendServerCommand(ent - g_entities, va("print \"^5You win 300 credits\n\""));
					ent->client->ps.persistant[PERS_CREDIT] += 300;
				}
				else
				{
					trap_SendServerCommand(ent - g_entities, va("print \"^5You win 150 credits\n\""));
					ent->client->ps.persistant[PERS_CREDIT] += 150;
				}
			}
			else
			{
				if (g_alienStage.integer == 2)
				{
					trap_SendServerCommand(ent - g_entities, va("print \"^5You win 2 evos\n\""));
					ent->client->ps.persistant[PERS_CREDIT] += 2;
				}
				else
				{
					trap_SendServerCommand(ent - g_entities, va("print \"^5You win 2 evos\n\""));
					ent->client->ps.persistant[PERS_CREDIT] += 2;
				}
			}
			break;
		case 5:
			if ((ent->client->pers.teamSelection == PTE_HUMANS
					&& ent->client->ps.stats[STAT_MAX_HEALTH] > 190)
					|| (ent->client->pers.teamSelection == PTE_ALIENS
							&& ent->client->ps.stats[STAT_MAX_HEALTH] > 550))
			{
				trap_SendServerCommand(ent - g_entities, va("print \"^1You eat a pizza\n\""));
				ent->health
						= ent->health + 50; // No more exponential hp.
				G_AddEvent(ent, EV_MEDKIT_USED, 0);
			}
			else
			{
				trap_SendServerCommand(ent - g_entities, va("print \"^1You drink a coke\n\""));
				ent->health
						= ent->health + 65;
				G_AddEvent(ent, EV_MEDKIT_USED, 0);
			}
			break;
		case 6:
			trap_SendServerCommand(ent - g_entities, va("print \"^5Your health has been restored to full \n\""));
			if (ent->health < ent->client->ps.stats[STAT_MAX_HEALTH])
			{
				ent->health = ent->client->ps.stats[STAT_MAX_HEALTH];
			}
			G_AddEvent(ent, EV_MEDKIT_USED, 0);
			break;
		case 7:
			ent->client->ps.stats[STAT_STATE] |= SS_FLY;
			randomgod = (rand() % 15) + 20;
			if (randomgod < 5)
			{
				randomgod = 5;
			}
			if (randomgod > 15)
			{
				randomgod = 15;
			}
			trap_SendServerCommand(ent - g_entities, va("print \"^5You win Chicken Wings for %d Seconds \n\"", randomgod));
			ent->client->pers.rtdflytime = level.time + (randomgod * 1000);
			break;
		default:
			randomgod = (rand() % 15) + 20;
			if (randomgod < 6)
			{
				randomgod = 6;
			}
			if (randomgod > 15)
			{
				randomgod = 15;
			}
			if (ent->client->pers.teamSelection == PTE_HUMANS
					|| ent->client->ps.weapon == WP_ALEVEL1
					|| ent->client->ps.weapon == WP_ALEVEL1_UPG
					|| ent->client->ps.weapon == WP_ALEVEL0
					|| ent->client->ps.weapon == WP_ALEVEL2
					|| ent->client->ps.weapon == WP_ALEVEL2_UPG)
			{
				randomgod = 15; //Humans have all chances to be invisible, after all aliens have radars all the time.
			}
			if (ent->client->ps.weapon == WP_ALEVEL4)// with the new anticamp system aliens dont need it to fix em
			{
				randomgod = 7; // trying to make em work but not so hard
			}
			trap_SendServerCommand(ent - g_entities, va("print \"^5You win invisible %d seconds\n\"", randomgod));
			if (!(ent->client->ps.eFlags & EF_NODRAW))
			{
				ent->client->ps.eFlags |= EF_NODRAW;
			}
			ent->client->pers.rtdinvtime = level.time + (randomgod * 1000);
			//trap_SendServerCommand( -1, va( "print \"%s" S_COLOR_WHITE " won invisible mode\n\"", ent->client->pers.netname ) );
			break;
	}
}
