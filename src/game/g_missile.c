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

#define MISSILE_PRESTEP_TIME  50

/*
================
G_BounceMissile

================
*/
void G_BounceMissile( gentity_t *ent, trace_t *trace )
{
  vec3_t  velocity;
  float dot;
  int   hitTime;

  // reflect the velocity on the trace plane
  hitTime = level.previousTime + ( level.time - level.previousTime ) * trace->fraction;
  BG_EvaluateTrajectoryDelta( &ent->s.pos, hitTime, velocity );
  dot = DotProduct( velocity, trace->plane.normal );
  VectorMA( velocity, -2 * dot, trace->plane.normal, ent->s.pos.trDelta );

  if( ent->s.eFlags & EF_BOUNCE_HALF )
  {
    VectorScale( ent->s.pos.trDelta, 0.65, ent->s.pos.trDelta );
    // check for stop
    if( trace->plane.normal[ 2 ] > 0.2 && VectorLength( ent->s.pos.trDelta ) < 40 )
    {
      G_SetOrigin( ent, trace->endpos );
      return;
    }
  }

  VectorAdd( ent->r.currentOrigin, trace->plane.normal, ent->r.currentOrigin );
  VectorCopy( ent->r.currentOrigin, ent->s.pos.trBase );
  ent->s.pos.trTime = level.time;
}


/*
================
G_ExplodeMissile

Explode a missile without an impact
================
*/
void G_ExplodeMissile( gentity_t *ent )
{
  gentity_t *cliente;
  vec3_t    dir;
  vec3_t    dir2;
  vec3_t    origin;

  BG_EvaluateTrajectory( &ent->s.pos, level.time, origin );
  SnapVector( origin );
  G_SetOrigin( ent, origin );

  // we don't have a valid direction, so just point straight up
  dir[ 0 ] = dir[ 1 ] = 0;
  dir[ 2 ] = 1;

    //self = &g_entities[ self->r.ownerNum ];
      cliente = &g_entities[ ent->r.ownerNum ];


  ent->s.eType = ET_GENERAL;

  switch(ent->s.weapon)
  {
    case WP_ABUILD2:
        trap_SendServerCommand( ent->parent-g_entities, "print \"^5Overmind: ^7Your nuke exploded.\n\"" );
		G_AddPredictableEvent( ent, EV_ALIEN_EVOLVE, 0 );
		break;
    case WP_LOCKBLOB_LAUNCHER:
		break;
    case WP_FLAMER:
		break;
    default:G_AddEvent(ent,EV_MISSILE_MISS,DirToByte(dir));break;
  }

  //TA: tired... can't be fucked... hack
  /*
  if( ent->s.weapon != WP_LOCKBLOB_LAUNCHER &&
      ent->s.weapon != WP_FLAMER )
    G_AddEvent( ent, EV_MISSILE_MISS, DirToByte( dir ) );
  */
  ent->freeAfterEvent = qtrue;

  // splash damage
  if( ent->splashDamage )
    G_RadiusDamage( ent->r.currentOrigin, ent->parent, ent->splashDamage,
                    ent->splashRadius, ent, ent->splashMethodOfDeath );

  trap_LinkEntity( ent );

  if(ent->s.weapon == WP_GRENADE && cliente->client && ent->childnade == 0){
      gentity_t *m;
      gentity_t *m1;
      gentity_t *m2;
      gentity_t *m3;

      origin[2] = 0;

      dir2[0] = 3;  dir2[1] = 3;
      dir2[ 2 ] = 1;
      m = launch_grenade( ent, origin, dir2 );

      dir2[0] = -3;  dir2[1] = 3;
      dir2[ 2 ] = 1;
      m1 = launch_grenade( ent, origin, dir2 );

      dir2[0] = -3;  dir2[1] = -3;
      dir2[ 2 ] = 1;
      m2 = launch_grenade( ent, origin, dir2 );

      dir2[0] = 3;  dir2[1] = -3;
      dir2[ 2 ] = 1;
      m3 = launch_grenade( ent, origin, dir2 );

			cliente->client->pers.grenades -= 1;
      //return;
  }
  
}

void AHive_ReturnToHive( gentity_t *self );

/*
================
G_MissileImpact

================
*/
void G_MissileImpact( gentity_t *ent, trace_t *trace )
{
  gentity_t   *other, *attacker;
  qboolean    returnAfterDamage = qfalse;
  vec3_t      dir;


  VectorCopy( ent->s.origin2, dir );

  other = &g_entities[ trace->entityNum ];
  attacker = &g_entities[ ent->r.ownerNum ];
  
  switch(ent->parent->s.weapon)
  {
    case WP_ABUILD2:
		//G_AddPredictableEvent( ent->parent, EV_ALIEN_ACIDTUBE, DirToByte( dir ) ); //this works
		//G_AddEvent( ent->parent, EV_HUMAN_BUILDABLE_EXPLOSION, DirToByte( dir ) );
		//trap_SendServerCommand( -1, va( "print \"SOTHISWORKSTO\n\"") );
		//G_AddPredictableEvent( ent, EV_ALIEN_EVOLVE, 0 );
		break;
  }
  if( !strcmp( ent->classname, "slowblob" ) )
  {
	  /*trap_SendServerCommand( -1, va( "print \"WTFFFFFFFFF\n\"") );

	  G_AddPredictableEvent( ent, EV_HUMAN_BUILDABLE_EXPLOSION, 0 );
	  G_AddEvent( ent, EV_ALIEN_BUILDABLE_EXPLOSION, 0 );*/
  }

  // check for bounce
  if( !other->takedamage &&
      ( ent->s.eFlags & ( EF_BOUNCE | EF_BOUNCE_HALF ) ) )
  {
    G_BounceMissile( ent, trace );

    //only play a sound if requested
    if( !( ent->s.eFlags & EF_NO_BOUNCE_SOUND ) )
      G_AddEvent( ent, EV_GRENADE_BOUNCE, 0 );

    return;
  }

  if( !strcmp( ent->classname, "grenade" ) )
  {
    //grenade doesn't explode on impact
    G_BounceMissile( ent, trace );

    //only play a sound if requested
    if( !( ent->s.eFlags & EF_NO_BOUNCE_SOUND ) )
      G_AddEvent( ent, EV_GRENADE_BOUNCE, 0 );

    return;
  }
  
  if( !strcmp( ent->classname, "lcannon" ) && other->client->pers.teamSelection == PTE_ALIENS && ent->parent->client->pers.teamSelection == PTE_ALIENS)//erm how this will bounce lolz?
  {
		 G_BounceMissile( ent, trace );

    //only play a sound if requested
    /*if( !( ent->s.eFlags & EF_NO_BOUNCE_SOUND ) )
      G_AddEvent( ent, EV_GRENADE_BOUNCE, 0 );*/
      //G_AddEvent( ent, EV_ALIEN_BUILDABLE_EXPLOSION, 1 );

    return;
	}
  
  else if( !strcmp( ent->classname, "lockblob" ) )
  {
    if( other->client && other->client->ps.stats[ STAT_PTEAM ] == PTE_HUMANS )
    {
      other->client->ps.stats[ STAT_STATE ] |= SS_BLOBLOCKED;
      other->client->lastLockTime = level.time;
      AngleVectors( other->client->ps.viewangles, dir, NULL, NULL );
      other->client->ps.stats[ STAT_VIEWLOCK ] = DirToByte( dir );
    }
  }
  else if( !strcmp( ent->classname, "slowblob" ) )
  {
    if( other->client && other->client->ps.stats[ STAT_PTEAM ] == PTE_HUMANS )
    {
      other->client->ps.stats[ STAT_STATE ] |= SS_SLOWLOCKED;
      other->client->lastSlowTime = level.time;
      AngleVectors( other->client->ps.viewangles, dir, NULL, NULL );
      other->client->ps.stats[ STAT_VIEWLOCK ] = DirToByte( dir );
    }
  }
  else if( !strcmp( ent->classname, "hive" ) )
  {
    if( other->s.eType == ET_BUILDABLE && other->s.modelindex == BA_A_HIVE )
    {
      if( !ent->parent )
        G_Printf( S_COLOR_YELLOW "WARNING: hive entity has no parent in G_MissileImpact\n" );
      else
        ent->parent->active = qfalse;

      G_FreeEntity( ent );
      return;
    }
    else
    {
      //prevent collision with the client when returning
      ent->r.ownerNum = other->s.number;

      ent->think = AHive_ReturnToHive;
      ent->nextthink = level.time + FRAMETIME;

      //only damage humans
      if( other->client && other->client->ps.stats[ STAT_PTEAM ] == PTE_HUMANS )
        returnAfterDamage = qtrue;
      else
        return;
    }
  }

  // impact damage
  if( other->takedamage )
  {
    // FIXME: wrong damage direction?
    if( ent->damage )
    {
      vec3_t  velocity;

      BG_EvaluateTrajectoryDelta( &ent->s.pos, level.time, velocity );
      if( VectorLength( velocity ) == 0 )
        velocity[ 2 ] = 1;  // stepped on a grenade

      G_Damage( other, ent, attacker, velocity, ent->s.origin, ent->damage,
        0, ent->methodOfDeath );
    }
  }

  if( returnAfterDamage )
    return;

  // is it cheaper in bandwidth to just remove this ent and create a new
  // one, rather than changing the missile into the explosion?

  if( other->takedamage && other->client )
  {
    G_AddEvent( ent, EV_MISSILE_HIT, DirToByte( trace->plane.normal ) );
    ent->s.otherEntityNum = other->s.number;
  }
  else if( trace->surfaceFlags & SURF_METALSTEPS )
    G_AddEvent( ent, EV_MISSILE_MISS_METAL, DirToByte( trace->plane.normal ) );
  else
    G_AddEvent( ent, EV_MISSILE_MISS, DirToByte( trace->plane.normal ) );

  ent->freeAfterEvent = qtrue;

  // change over to a normal entity right at the point of impact
  ent->s.eType = ET_GENERAL;

  SnapVectorTowards( trace->endpos, ent->s.pos.trBase );  // save net bandwidth

  G_SetOrigin( ent, trace->endpos );

  // splash damage (doesn't apply to person directly hit)
  if( ent->splashDamage )
    G_RadiusDamage( trace->endpos, ent->parent, ent->splashDamage, ent->splashRadius,
                    other, ent->splashMethodOfDeath );

  trap_LinkEntity( ent );
}


/*
================
G_RunMissile

================
*/
void G_RunMissile( gentity_t *ent )
{
  vec3_t    origin;
  trace_t   tr;
  int     passent;

  // get current position
  BG_EvaluateTrajectory( &ent->s.pos, level.time, origin );

  // ignore interactions with the missile owner
  passent = ent->r.ownerNum;

  // trace a line from the previous position to the current position
  trap_Trace( &tr, ent->r.currentOrigin, ent->r.mins, ent->r.maxs, origin, passent, ent->clipmask );

  if( tr.startsolid || tr.allsolid )
  {
    // make sure the tr.entityNum is set to the entity we're stuck in
    trap_Trace( &tr, ent->r.currentOrigin, ent->r.mins, ent->r.maxs, ent->r.currentOrigin, passent, ent->clipmask );
    tr.fraction = 0;
  }
  else
    VectorCopy( tr.endpos, ent->r.currentOrigin );

  ent->r.contents = CONTENTS_SOLID; //trick trap_LinkEntity into...
  trap_LinkEntity( ent );
  ent->r.contents = 0; //...encoding bbox information

  if( tr.fraction != 1 )
  {
    // never explode or bounce on sky
    if( tr.surfaceFlags & SURF_NOIMPACT )
    {
      // If grapple, reset owner
      if( ent->parent && ent->parent->client && ent->parent->client->hook == ent )
        ent->parent->client->hook = NULL;

      G_FreeEntity( ent );
      return;
    }

    G_MissileImpact( ent, &tr );
    if( ent->s.eType != ET_MISSILE )
      return;   // exploded
  }

  // check think function after bouncing
  G_RunThink( ent );
}


//=============================================================================

/*
=================
fire_flamer

=================
*/
gentity_t *fire_flamer( gentity_t *self, vec3_t start, vec3_t dir )
{
  gentity_t *bolt;
  vec3_t    pvel;

  VectorNormalize (dir);

  bolt = G_Spawn();
  bolt->classname = "flame";
  bolt->nextthink = level.time + FLAMER_LIFETIME;
  bolt->think = G_ExplodeMissile;
  bolt->s.eType = ET_MISSILE;
  bolt->r.svFlags = SVF_USE_CURRENT_ORIGIN;
  bolt->s.weapon = WP_FLAMER;
  bolt->s.generic1 = self->s.generic1; //weaponMode
  bolt->r.ownerNum = self->s.number;
  bolt->parent = self;
  bolt->damage = FLAMER_DMG;
  bolt->splashDamage = FLAMER_DMG;
  bolt->splashRadius = FLAMER_RADIUS;
  bolt->methodOfDeath = MOD_FLAMER;
  bolt->splashMethodOfDeath = MOD_FLAMER_SPLASH;
  bolt->clipmask = MASK_SHOT;
  bolt->target_ent = NULL;
  bolt->r.mins[ 0 ] = bolt->r.mins[ 1 ] = bolt->r.mins[ 2 ] = -15.0f;
  bolt->r.maxs[ 0 ] = bolt->r.maxs[ 1 ] = bolt->r.maxs[ 2 ] = 15.0f;

  bolt->s.pos.trType = TR_LINEAR;
  bolt->s.pos.trTime = level.time - MISSILE_PRESTEP_TIME;   // move a bit on the very first frame
  VectorCopy( start, bolt->s.pos.trBase );
  VectorScale( self->client->ps.velocity, FLAMER_LAG, pvel );
  VectorMA( pvel, FLAMER_SPEED, dir, bolt->s.pos.trDelta );
  SnapVector( bolt->s.pos.trDelta );      // save net bandwidth

  VectorCopy( start, bolt->r.currentOrigin );

  return bolt;
}

//=============================================================================

/*
=================
fire_blaster

=================
*/
gentity_t *fire_blaster( gentity_t *self, vec3_t start, vec3_t dir )
{
  gentity_t *bolt;

  VectorNormalize (dir);

  bolt = G_Spawn();
  bolt->classname = "blaster";
  bolt->nextthink = level.time + 10000;
  bolt->think = G_ExplodeMissile;
  bolt->s.eType = ET_MISSILE;
  bolt->r.svFlags = SVF_USE_CURRENT_ORIGIN;
  bolt->s.weapon = WP_BLASTER;
  bolt->s.generic1 = self->s.generic1; //weaponMode
  bolt->r.ownerNum = self->s.number;
  bolt->parent = self;
  bolt->damage = BLASTER_DMG;
  bolt->splashDamage = 0;
  bolt->splashRadius = 0;
  bolt->methodOfDeath = MOD_BLASTER;
  bolt->splashMethodOfDeath = MOD_BLASTER;
  bolt->clipmask = MASK_SHOT;
  bolt->target_ent = NULL;

  bolt->s.pos.trType = TR_LINEAR;
  bolt->s.pos.trTime = level.time - MISSILE_PRESTEP_TIME;   // move a bit on the very first frame
  VectorCopy( start, bolt->s.pos.trBase );
  VectorScale( dir, BLASTER_SPEED, bolt->s.pos.trDelta );
  SnapVector( bolt->s.pos.trDelta );      // save net bandwidth

  VectorCopy( start, bolt->r.currentOrigin );

  return bolt;
}

//=============================================================================

/*
=================
fire_pulseRifle

=================
*/
gentity_t *fire_pulseRifle( gentity_t *self, vec3_t start, vec3_t dir )
{
  gentity_t *bolt;

  VectorNormalize (dir);

  bolt = G_Spawn();
  bolt->classname = "pulse";
  bolt->nextthink = level.time + 10000;
  bolt->think = G_ExplodeMissile;
  bolt->s.eType = ET_MISSILE;
  bolt->r.svFlags = SVF_USE_CURRENT_ORIGIN;
  bolt->s.weapon = WP_PULSE_RIFLE;
  bolt->s.generic1 = self->s.generic1; //weaponMode
  bolt->r.ownerNum = self->s.number;
  bolt->parent = self;
  bolt->damage = PRIFLE_DMG;
  bolt->splashDamage = 0;
  bolt->splashRadius = 0;
  bolt->methodOfDeath = MOD_PRIFLE;
  bolt->splashMethodOfDeath = MOD_PRIFLE;
  bolt->clipmask = MASK_SHOT;
  bolt->target_ent = NULL;

  bolt->s.pos.trType = TR_LINEAR;
  bolt->s.pos.trTime = level.time - MISSILE_PRESTEP_TIME;   // move a bit on the very first frame
  VectorCopy( start, bolt->s.pos.trBase );
  VectorScale( dir, PRIFLE_SPEED, bolt->s.pos.trDelta );
  SnapVector( bolt->s.pos.trDelta );      // save net bandwidth

  VectorCopy( start, bolt->r.currentOrigin );

  return bolt;
}

//=============================================================================

/*
=================
fire_luciferCannon

=================
*/
gentity_t *fire_luciferCannon( gentity_t *self, vec3_t start, vec3_t dir, int damage, int radius )
{
  gentity_t *bolt;
  int localDamage = (int)( ceil( ( (float)damage /
                                   (float)LCANNON_TOTAL_CHARGE ) * (float)LCANNON_DAMAGE ) );

  VectorNormalize( dir );

  bolt = G_Spawn( );
  bolt->classname = "lcannon";

  if( damage == LCANNON_TOTAL_CHARGE )
    bolt->nextthink = level.time;
  else
    bolt->nextthink = level.time + 10000;

  bolt->think = G_ExplodeMissile;
  bolt->s.eType = ET_MISSILE;
  bolt->r.svFlags = SVF_USE_CURRENT_ORIGIN;
  bolt->s.weapon = WP_LUCIFER_CANNON;
  /*if(self->client->ps.weapon == WP_ALEVEL1)
  {
		bolt->s.weapon = WP_FLAMER;
	}*/
  bolt->s.generic1 = self->s.generic1; //weaponMode
  bolt->r.ownerNum = self->s.number;
  bolt->parent = self;
  bolt->damage = localDamage;
  bolt->splashDamage = localDamage / 2;
  bolt->splashRadius = radius;
  bolt->methodOfDeath = MOD_LCANNON;
  bolt->splashMethodOfDeath = MOD_LCANNON_SPLASH;
  bolt->clipmask = MASK_SHOT;
  bolt->target_ent = NULL;
/*
  bolt->s.pos.trType = TR_LINEAR;
  bolt->s.pos.trTime = level.time - MISSILE_PRESTEP_TIME;   // move a bit on the very first frame
  VectorCopy( start, bolt->s.pos.trBase );
  VectorScale( dir, LCANNON_SPEED, bolt->s.pos.trDelta );
  SnapVector( bolt->s.pos.trDelta );      // save net bandwidth

  VectorCopy( start, bolt->r.currentOrigin );*/
  
   if(self->client->pers.blasterball == 1)
  {
		/*bolt->classname = "hive";
		bolt->s.weapon = WP_HIVE;*/
        self->client->pers.blasterball = 0;
        bolt->nextthink = level.time + 1000000;//meh alot of time, dosnt matter, maybe it matters who knows.
        bolt->damage = 500;
        bolt->splashDamage = 0;
        bolt->s.pos.trType = TR_LINEAR_STOP;
        bolt->s.pos.trTime = level.time - MISSILE_PRESTEP_TIME; 
        VectorCopy( start, bolt->s.pos.trBase );
        VectorScale( dir, 0, bolt->s.pos.trDelta );
        SnapVector( bolt->s.pos.trDelta );      // save net bandwidth

        VectorCopy( start, bolt->r.currentOrigin );
    }   
    else
    {   
        bolt->s.pos.trType = TR_LINEAR;
        bolt->s.pos.trTime = level.time - MISSILE_PRESTEP_TIME;   // move a bit on the very first frame
        VectorCopy( start, bolt->s.pos.trBase );
        VectorScale( dir, LCANNON_SPEED, bolt->s.pos.trDelta );
        SnapVector( bolt->s.pos.trDelta );      // save net bandwidth

        VectorCopy( start, bolt->r.currentOrigin );
    }   


  return bolt;
}

/*
=================
launch_grenade

=================
*/
gentity_t *launch_grenade( gentity_t *self, vec3_t start, vec3_t dir )
{
  gentity_t *bolt;
  gentity_t *cliente;

  vec3_t  forward, right, up;

  VectorNormalize( dir );

  bolt = G_Spawn( );
  if(self->client)
  {
    bolt->r.ownerNum = self->s.number;
    bolt->childnade = 0;
    bolt->nextthink = level.time + 500000;
    bolt->parent = self;
     bolt->damage = GRENADE_DAMAGE/2;
    bolt->splashDamage = GRENADE_DAMAGE/2;
  }
  else
  {
    cliente = &g_entities[ self->r.ownerNum ];
    bolt->childnade = 1;
    bolt->r.ownerNum = cliente->s.number;
    bolt->nextthink = level.time + 3000;
    bolt->parent = cliente;
    bolt->damage = GRENADE_DAMAGE/6;
    bolt->splashDamage = GRENADE_DAMAGE/6;
  }
  
  if(self->newbiebonus == 1 && self->health <= 0)
  {
		bolt->nextthink = level.time + 300;
		bolt->childnade = 1;
		bolt->damage = GRENADE_DAMAGE*4;
    bolt->splashDamage = GRENADE_DAMAGE*4;
	}
  
  bolt->classname = "grenade";
  
  bolt->think = G_ExplodeMissile;
  bolt->s.eType = ET_MISSILE;
  bolt->r.svFlags = SVF_USE_CURRENT_ORIGIN;
  bolt->s.weapon = WP_GRENADE;
  bolt->s.eFlags = EF_BOUNCE_HALF;
  bolt->s.generic1 = WPM_PRIMARY; //weaponMode

  bolt->splashRadius = GRENADE_RANGE;
  bolt->methodOfDeath = MOD_GRENADE;
  bolt->splashMethodOfDeath = MOD_GRENADE;
  bolt->clipmask = MASK_SHOT;
  bolt->target_ent = NULL;
  bolt->r.mins[ 0 ] = bolt->r.mins[ 1 ] = bolt->r.mins[ 2 ] = -3.0f;
  bolt->r.maxs[ 0 ] = bolt->r.maxs[ 1 ] = bolt->r.maxs[ 2 ] = 3.0f;
  bolt->s.time = level.time;

  bolt->s.pos.trType = TR_GRAVITY;
  bolt->s.pos.trTime = level.time - MISSILE_PRESTEP_TIME;   // move a bit on the very first frame
  VectorCopy( start, bolt->s.pos.trBase );
  //VectorScale( dir, GRENADE_SPEED, bolt->s.pos.trDelta );

  if(!self->client)
  {
      AngleVectors( self->s.angles2, forward, right, up );
      VectorCopy( self->s.pos.trBase, start );
      VectorCopy( start, bolt->s.pos.trBase );
      VectorScale( dir, GRENADE_SPEED, bolt->s.pos.trDelta );
  }
  else{
    VectorCopy( start, bolt->s.pos.trBase );
    VectorScale( dir, GRENADE_SPEED, bolt->s.pos.trDelta );
  }


  SnapVector( bolt->s.pos.trDelta );      // save net bandwidth

  VectorCopy( start, bolt->r.currentOrigin );

  return bolt;
}
//=============================================================================

/*
================
AHive_ReturnToHive

Adjust the trajectory to point towards the hive
================
*/
void AHive_ReturnToHive( gentity_t *self )
{
  vec3_t  dir;
  trace_t tr;

  if( !self->parent )
  {
    G_Printf( S_COLOR_YELLOW "WARNING: AHive_ReturnToHive called with no self->parent\n" );
    return;
  }

  trap_UnlinkEntity( self->parent );
  trap_Trace( &tr, self->r.currentOrigin, self->r.mins, self->r.maxs,
              self->parent->r.currentOrigin, self->r.ownerNum, self->clipmask );
  trap_LinkEntity( self->parent );

  if( tr.fraction < 1.0f )
  {
    //if can't see hive then disperse
    VectorCopy( self->r.currentOrigin, self->s.pos.trBase );
    self->s.pos.trType = TR_STATIONARY;
    self->s.pos.trTime = level.time;

    self->think = G_ExplodeMissile;
    self->nextthink = level.time + 2000;
    self->parent->active = qfalse; //allow the parent to start again
  }
  else
  {
    VectorSubtract( self->parent->r.currentOrigin, self->r.currentOrigin, dir );
    VectorNormalize( dir );

    //change direction towards the hive
    VectorScale( dir, HIVE_SPEED, self->s.pos.trDelta );
    SnapVector( self->s.pos.trDelta );      // save net bandwidth
    VectorCopy( self->r.currentOrigin, self->s.pos.trBase );
    self->s.pos.trTime = level.time;

    self->think = G_ExplodeMissile;
    self->nextthink = level.time + 15000;
  }
}

/*
================
AHive_SearchAndDestroy

Adjust the trajectory to point towards the target
================
*/
void AHive_SearchAndDestroy( gentity_t *self )
{
  vec3_t dir;
  trace_t tr;

  trap_Trace( &tr, self->r.currentOrigin, self->r.mins, self->r.maxs,
              self->target_ent->r.currentOrigin, self->r.ownerNum, self->clipmask );

  //if there is no LOS or the parent hive is too far away or the target is dead or notargeting, return
  if( tr.entityNum == ENTITYNUM_WORLD ||
      Distance( self->r.currentOrigin, self->parent->r.currentOrigin ) > ( HIVE_RANGE * 5 ) ||
      self->target_ent->health <= 0 || self->target_ent->flags & FL_NOTARGET )
  {
    self->r.ownerNum = ENTITYNUM_WORLD;

    self->think = AHive_ReturnToHive;
    self->nextthink = level.time + FRAMETIME;
  }
  else
  {
    VectorSubtract( self->target_ent->r.currentOrigin, self->r.currentOrigin, dir );
    VectorNormalize( dir );

    //change direction towards the player
    VectorScale( dir, HIVE_SPEED, self->s.pos.trDelta );
    SnapVector( self->s.pos.trDelta );      // save net bandwidth
    VectorCopy( self->r.currentOrigin, self->s.pos.trBase );
    self->s.pos.trTime = level.time;

    self->nextthink = level.time + HIVE_DIR_CHANGE_PERIOD;
  }
}

/*
=================
fire_hive
=================
*/
gentity_t *fire_hive( gentity_t *self, vec3_t start, vec3_t dir )
{
  gentity_t *bolt;

  VectorNormalize ( dir );

  bolt = G_Spawn( );
  bolt->classname = "hive";
  bolt->nextthink = level.time + HIVE_DIR_CHANGE_PERIOD;
  bolt->think = AHive_SearchAndDestroy;
  bolt->s.eType = ET_MISSILE;
  bolt->s.eFlags |= EF_BOUNCE|EF_NO_BOUNCE_SOUND;
  bolt->r.svFlags = SVF_USE_CURRENT_ORIGIN;
  bolt->s.weapon = WP_HIVE;
  bolt->s.generic1 = WPM_PRIMARY; //weaponMode
  bolt->r.ownerNum = self->s.number;
  bolt->parent = self;
  bolt->damage = HIVE_DMG;
  bolt->splashDamage = 0;
  bolt->splashRadius = 0;
  bolt->methodOfDeath = MOD_SWARM;
  bolt->clipmask = MASK_SHOT;
  bolt->target_ent = self->target_ent;

  bolt->s.pos.trType = TR_LINEAR;
  bolt->s.pos.trTime = level.time - MISSILE_PRESTEP_TIME;   // move a bit on the very first frame
  VectorCopy( start, bolt->s.pos.trBase );
  VectorScale( dir, HIVE_SPEED, bolt->s.pos.trDelta );
  SnapVector( bolt->s.pos.trDelta );      // save net bandwidth
  VectorCopy( start, bolt->r.currentOrigin );

  return bolt;
}

//=============================================================================

/*
=================
fire_lockblob
=================
*/
gentity_t *fire_lockblob( gentity_t *self, vec3_t start, vec3_t dir )
{
  gentity_t *bolt;

  VectorNormalize ( dir );

  bolt = G_Spawn( );
  bolt->classname = "lockblob";
  bolt->nextthink = level.time + 15000;
  bolt->think = G_ExplodeMissile;
  bolt->s.eType = ET_MISSILE;
  bolt->r.svFlags = SVF_USE_CURRENT_ORIGIN;
  bolt->s.weapon = WP_LOCKBLOB_LAUNCHER;
  bolt->s.generic1 = WPM_PRIMARY; //weaponMode
  bolt->r.ownerNum = self->s.number;
  bolt->parent = self;
  bolt->damage = 0;
  bolt->splashDamage = 0;
  bolt->splashRadius = 0;
  bolt->methodOfDeath = MOD_UNKNOWN; //doesn't do damage so will never kill
  bolt->clipmask = MASK_SHOT;
  bolt->target_ent = NULL;

  bolt->s.pos.trType = TR_LINEAR;
  bolt->s.pos.trTime = level.time - MISSILE_PRESTEP_TIME;   // move a bit on the very first frame
  VectorCopy( start, bolt->s.pos.trBase );
  VectorScale( dir, 500, bolt->s.pos.trDelta );
  SnapVector( bolt->s.pos.trDelta );      // save net bandwidth
  VectorCopy( start, bolt->r.currentOrigin );

  return bolt;
}

/*
=================
fire_slowBlob
=================
*/
gentity_t *fire_slowBlob( gentity_t *self, vec3_t start, vec3_t dir )
{
  gentity_t *bolt;
  vec3_t    dir2, down;
  int min=0,sec=0;

  /*if(self->client)
  {
     if(self->client->ps.persistant[ PERS_CREDIT ] < 2){
		 trap_SendServerCommand( self-g_entities, "print \"^5Overmind: ^7You need 2 evos to drop a nuke\n\"" );
		 return NULL;
		 }
     else{self->client->ps.persistant[ PERS_CREDIT ]-=2;}
  }*/
  if(self->client->pers.droppedbombs >= 1)
  {
	  self->client->pers.droppedbombs = 0;
  }
  
  if(self->client->pers.timerbombs > level.time)
  {
	  //You cant drop nades
		min = ((self->client->pers.timerbombs - level.time)/1000) / 60 ;
		sec = ((self->client->pers.timerbombs - level.time)/1000) % 60 ;
		if(sec < 10)
		{
			trap_SendServerCommand( self-g_entities, va(
        "print \"^11 Bomb every 1 minuts wait %d:0%d\n^2!info granger\n\"",min,sec ) );
		}
		else
		{
			trap_SendServerCommand( self-g_entities, va(
        "print \"^11 Bomb every 1 minuts wait %d:%d\n^2!info granger\n\"",min,sec ) );
		}
	  return NULL;
  }
  self->client->pers.droppedbombs++;
  self->client->pers.timerbombs = level.time + (60000 * 1);
  
  VectorSet(down,0.0f,0.0f,ABUILDER_BLOB_SPEED*(-1));//I hope bomb don't go up LOOOOOOOL,LOL it go

  VectorNormalize ( dir );

  bolt = G_Spawn( );
  bolt->classname = "slowblob";
  bolt->nextthink = level.time + 30000;
  bolt->think = G_ExplodeMissile;
  bolt->s.eType = ET_MISSILE;
  bolt->r.svFlags = SVF_USE_CURRENT_ORIGIN;
  bolt->s.weapon = WP_ABUILD2;
  bolt->s.generic1 = self->s.generic1; //weaponMode
  bolt->r.ownerNum = self->s.number;
  bolt->parent = self;
  bolt->damage = ABUILDER_BLOB_DMG;
  bolt->splashDamage = ABUILDER_BLOB_DMG;
  bolt->splashRadius = ABUILDER_BLOB_RADIUS;
  bolt->methodOfDeath = MOD_SLOWBLOB;
  bolt->splashMethodOfDeath = MOD_SLOWBLOB;
  bolt->clipmask = MASK_SHOT;
  bolt->target_ent = NULL;

  bolt->s.pos.trType = TR_LINEAR;
  dir2[0] = dir2[1] = 0;
  dir2[2] = -10;
  G_AddPredictableEvent( self, EV_ALIEN_ACIDTUBE, DirToByte( dir2 )  );
  
  bolt->s.pos.trTime = level.time - MISSILE_PRESTEP_TIME;   // move a bit on the very first frame
  VectorCopy( start, bolt->s.pos.trBase );
  VectorCopy( down, bolt->s.pos.trDelta );
  SnapVector( bolt->s.pos.trDelta );      // save net bandwidth
  VectorCopy( start, bolt->r.currentOrigin );

  return bolt;
  
 /* VectorNormalize ( dir );

  bolt = G_Spawn( );
  bolt->classname = "slowblob";
  bolt->nextthink = level.time + 30000;
  bolt->think = G_ExplodeMissile;
  bolt->s.eType = ET_MISSILE;
  bolt->r.svFlags = SVF_USE_CURRENT_ORIGIN;
  bolt->s.weapon = WP_ABUILD2;
  bolt->s.generic1 = self->s.generic1; //weaponMode
  bolt->r.ownerNum = self->s.number;
  bolt->parent = self;
  bolt->damage = ABUILDER_BLOB_DMG;
  bolt->splashDamage = ABUILDER_BLOB_DMG;
  bolt->splashRadius = ABUILDER_BLOB_RADIUS;
  bolt->methodOfDeath = MOD_SLOWBLOB;
  bolt->splashMethodOfDeath = MOD_SLOWBLOB;
  bolt->clipmask = MASK_SHOT;
  bolt->target_ent = NULL;

  bolt->s.pos.trType = TR_GRAVITY;
  dir2[0] = dir2[1] = 0;
  dir2[2] = -10;
  G_AddPredictableEvent( self, EV_ALIEN_ACIDTUBE, DirToByte( dir2 )  );
  bolt->s.pos.trTime = level.time - MISSILE_PRESTEP_TIME;   // move a bit on the very first frame
  VectorCopy( start, bolt->s.pos.trBase );
  VectorCopy( dir, bolt->s.pos.trDelta );
  SnapVector( bolt->s.pos.trDelta );      // save net bandwidth
  VectorCopy( start, bolt->r.currentOrigin );

  return bolt;*/
}

/*gentity_t *fire_slowBlob( gentity_t *self, vec3_t start, vec3_t dir )
{
  gentity_t *bolt;

  VectorNormalize ( dir );

  bolt = G_Spawn( );
  bolt->classname = "slowblob";
  bolt->nextthink = level.time + 15000;
  bolt->think = G_ExplodeMissile;
  bolt->s.eType = ET_MISSILE;
  bolt->r.svFlags = SVF_USE_CURRENT_ORIGIN;
  bolt->s.weapon = WP_ABUILD2;
  bolt->s.generic1 = self->s.generic1; //weaponMode
  bolt->r.ownerNum = self->s.number;
  bolt->parent = self;
  bolt->damage = ABUILDER_BLOB_DMG;
  bolt->splashDamage = 0;
  bolt->splashRadius = 0;
  bolt->methodOfDeath = MOD_SLOWBLOB;
  bolt->splashMethodOfDeath = MOD_SLOWBLOB;
  bolt->clipmask = MASK_SHOT;
  bolt->target_ent = NULL;

  bolt->s.pos.trType = TR_GRAVITY;
  bolt->s.pos.trTime = level.time - MISSILE_PRESTEP_TIME;   // move a bit on the very first frame
  VectorCopy( start, bolt->s.pos.trBase );
  VectorScale( dir, ABUILDER_BLOB_SPEED, bolt->s.pos.trDelta );
  SnapVector( bolt->s.pos.trDelta );      // save net bandwidth
  VectorCopy( start, bolt->r.currentOrigin );

  return bolt;
}*/

/*
=================
fire_paraLockBlob
=================
*/
gentity_t *fire_paraLockBlob( gentity_t *self, vec3_t start, vec3_t dir )
{
  gentity_t *bolt;

  VectorNormalize ( dir );

  bolt = G_Spawn( );
  bolt->classname = "lockblob";
  bolt->nextthink = level.time + 15000;
  bolt->think = G_ExplodeMissile;
  bolt->s.eType = ET_MISSILE;
  bolt->r.svFlags = SVF_USE_CURRENT_ORIGIN;
  bolt->s.weapon = WP_LOCKBLOB_LAUNCHER;
  bolt->s.generic1 = self->s.generic1; //weaponMode
  bolt->r.ownerNum = self->s.number;
  bolt->parent = self;
  bolt->damage = 0;
  bolt->splashDamage = 0;
  bolt->splashRadius = 0;
  bolt->clipmask = MASK_SHOT;
  bolt->target_ent = NULL;

  bolt->s.pos.trType = TR_GRAVITY;
  bolt->s.pos.trTime = level.time - MISSILE_PRESTEP_TIME;   // move a bit on the very first frame
  VectorCopy( start, bolt->s.pos.trBase );
  VectorScale( dir, LOCKBLOB_SPEED, bolt->s.pos.trDelta );
  SnapVector( bolt->s.pos.trDelta );      // save net bandwidth
  VectorCopy( start, bolt->r.currentOrigin );

  return bolt;
}

/*
=================
fire_bounceBall
=================
*/
gentity_t *fire_bounceBall( gentity_t *self, vec3_t start, vec3_t dir )
{
  gentity_t *bolt;

  VectorNormalize ( dir );

  bolt = G_Spawn( );
  bolt->classname = "bounceball";
  bolt->nextthink = level.time + 3000;
  bolt->think = G_ExplodeMissile;
  bolt->s.eType = ET_MISSILE;
  bolt->r.svFlags = SVF_USE_CURRENT_ORIGIN;
  bolt->s.weapon = WP_ALEVEL3_UPG;
  bolt->s.generic1 = self->s.generic1; //weaponMode
  bolt->r.ownerNum = self->s.number;
  bolt->parent = self;
  bolt->damage = LEVEL3_BOUNCEBALL_DMG;
  bolt->splashDamage = 0;
  bolt->splashRadius = 0;
  bolt->methodOfDeath = MOD_LEVEL3_BOUNCEBALL;
  bolt->splashMethodOfDeath = MOD_LEVEL3_BOUNCEBALL;
  bolt->clipmask = MASK_SHOT;
  bolt->target_ent = NULL;

  bolt->s.pos.trType = TR_GRAVITY;
  bolt->s.pos.trTime = level.time - MISSILE_PRESTEP_TIME;   // move a bit on the very first frame
  VectorCopy( start, bolt->s.pos.trBase );
  VectorScale( dir, LEVEL3_BOUNCEBALL_SPEED, bolt->s.pos.trDelta );
  SnapVector( bolt->s.pos.trDelta );      // save net bandwidth
  VectorCopy( start, bolt->r.currentOrigin );
  /*bolt->s.eFlags |= EF_BOUNCE;*/

  return bolt;
}
/*
=================
fire_fern
=================
*/

#define FERN_SINK_SPEED 100
#define FERN_SINK_TIME 3200
#define FERN_SINK_INCREMENT 0.50f
#define FERN_GROW_SPEED 100
#define FERN_GROW_INCREMENT 0.325f
#define FERN_GROW_HEIGHT 16.0f
#define FERN_LIFE_SPAN 3600000
#define FERN_CLEAN_DELAY 1000

static void fern_die( gentity_t *self )
{
  if( !self->active )
  {
    self->active = qtrue;
    self->timestamp = level.time;
  }

  if( level.time - self->timestamp > FERN_SINK_TIME )
  {
    G_FreeEntity( self );
    return;
  }

  self->nextthink = level.time + FERN_SINK_SPEED;
  self->s.pos.trBase[ 2 ] -= FERN_SINK_INCREMENT;
}


void fernTouch( gentity_t *ent, gentity_t *other, trace_t *trace )
{
	if(G_isPlayerConnected(other))
	{
		G_rtd(other);
		G_FreeEntity( ent );
	}
	else
	{
		G_FreeEntity( ent );
	}
}

static void fern_cleanup( gentity_t *self )
{
  gentity_t *ent;
  int count = 0;
  int i;

  //G_Printf("fern_cleanup\n");
  self->think = fern_die;
  self->nextthink = self->suicideTime;

  self->r.contents = CONTENTS_TRIGGER;
  self->touch = fernTouch;

  for( i = MAX_CLIENTS, ent = g_entities + i; i < level.num_entities; i++, ent++ )
  {
    if( ent->s.eType != ET_GENERAL || !ent->suicideTime ||
        strcmp( self->classname, "fern" ) || ent->active )
      continue;

    if( ent->think == fern_cleanup )
    {
      ent->think = fern_die;
      ent->nextthink = ent->suicideTime;
    }
    count++;
  }

  while( count && count > 100 )
  {
    gentity_t *oldest = NULL;

    for( i = MAX_CLIENTS, ent = g_entities + i; i < level.num_entities; i++, ent++ )
    {
      if( ent->s.eType != ET_GENERAL || !ent->suicideTime ||
          strcmp( self->classname, "fern" ) || ent->active )
        continue;

      if( !oldest ||
          ent->suicideTime < oldest->suicideTime )
        oldest = ent;
    }

    if( oldest )
    {
      oldest->nextthink = level.time + FERN_CLEAN_DELAY;
      oldest->suicideTime = 0;
    }

    count--;
  }
}

static void fern_grow( gentity_t *self )
{
  self->s.pos.trBase[ 2 ] += FERN_GROW_INCREMENT;

  if( self->s.pos.trBase[ 2 ] >= self->pos1[ 2 ] )
  {
    self->think = fern_cleanup;
    self->nextthink = level.time + FERN_CLEAN_DELAY;
    self->r.contents = CONTENTS_TRIGGER;
    self->touch = fernTouch;
  }
  else if((self->fernSpawnTime+8000) < level.time)
  {
	  self->r.contents = CONTENTS_TRIGGER;
	  self->touch = fernTouch;
	  self->think = fern_cleanup;
	  self->nextthink = level.time + FERN_CLEAN_DELAY;
  }
  else
  {
    self->nextthink = level.time + FERN_GROW_SPEED;
  }
}

gentity_t *fire_fern( vec3_t origin, vec3_t angles, int lifespan )
{
  gentity_t *self;
  vec3_t normal, end;
  trace_t tr;
  vec3_t mins,maxs;

  VectorSet(mins, 0, 0, 0);
  VectorSet(maxs,30,30,30);

  if( lifespan < FERN_CLEAN_DELAY )
    lifespan = FERN_LIFE_SPAN;

  VectorSet( normal, 0.0, 0.0, -1.0 );
  VectorMA( origin, 256.0, normal, end );
  trap_Trace( &tr, origin, NULL, NULL, end, -1, MASK_SOLID );
  if( !( tr.fraction < 1.0f ) )
    return NULL;

  self = G_Spawn( );
  self->classname = "fern";
  self->s.eType = ET_GENERAL;
  self->s.modelindex = G_ModelIndex( "models/mapobjects/plant_life/ferns.md3" );

  self->s.time = level.time;
  self->s.pos.trType = TR_STATIONARY;
  self->s.pos.trTime = level.time;

  self->think = fern_grow;
  self->nextthink = level.time + FERN_GROW_SPEED;
  self->fernSpawnTime = level.time;

  self->suicideTime = level.time + lifespan;

  VectorCopy(mins, self->r.mins);
  VectorCopy(mins, self->r.maxs);
  VectorCopy( origin, self->s.origin );
  VectorCopy( origin, self->r.currentOrigin );

  VectorCopy( tr.endpos, self->pos1 );
  VectorCopy( self->pos1, self->s.pos.trBase );
  self->s.pos.trBase[ 2 ] -= FERN_GROW_HEIGHT;

  self->s.apos.trBase[ YAW ] = angles[ YAW ];

  trap_LinkEntity( self );

  return self;
}


