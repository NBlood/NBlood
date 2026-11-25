//-------------------------------------------------------------------------
/*
Copyright (C) 2010-2019 EDuke32 developers and contributors
Copyright (C) 2019 Nuke.YKT
Copyright (C) NoOne

*********************************************************************
NoOne: This file provides dude patrol functionality in modern maps.
For full documentation visit: http://cruo.bloodgame.ru/xxsystem
*********************************************************************

This file is part of NBlood.

NBlood is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License version 2
as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/
//-------------------------------------------------------------------------

#ifdef NOONE_EXTENSIONS
#ifndef __AI_PATROL_H
#define __AI_PATROL_H
#include "common_game.h"

#define kMaxPatrolSpotValue 512

//  -------------------------------------------------------------------------   //
char aiPatrolSetMarker(spritetype* pSpr, XSPRITE* pXSpr);
void aiPatrolStop(spritetype* pSpr, int nTarg, char alarm = 0);
void aiPatrolState(spritetype* pSpr, XSPRITE* pXSpr, int nState, unsigned int nTime = 0);
int aiPatrolMarkerBusy(int nExcept, int nMark);
char aiPatrolMarkerReached(spritetype* pSpr, XSPRITE* pXSpr);
void aiPatrolFlagsMgr(spritetype* pSrc, XSPRITE* pXSrc, spritetype* pDest, XSPRITE* pXDest, char copy, char init);

FORCE_INLINE char aiPatrolWaiting(AISTATE* pState)  { return pState && pState->stateType == kAiStatePatrolIdle; }
FORCE_INLINE char aiPatrolMoving(AISTATE* pState)   { return pState && pState->stateType == kAiStatePatrolMove; }
FORCE_INLINE char aiPatrolTurning(AISTATE* pState)  { return pState && pState->stateType == kAiStatePatrolTurn; }
FORCE_INLINE char aiPatrolGuarding(AISTATE* pState) { return pState && pState->stateType == kAiStatePatrolGuard; }
FORCE_INLINE char aiInPatrolState(int nState)       { return rngok(nState, kAiStatePatrolBase, kAiStatePatrolMax); }
FORCE_INLINE char aiInPatrolState(AISTATE* pState)  { return pState && aiInPatrolState(pState->stateType); }
//  -------------------------------------------------------------------------   //

#endif
#endif