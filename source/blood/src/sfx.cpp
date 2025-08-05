//-------------------------------------------------------------------------
/*
Copyright (C) 2010-2019 EDuke32 developers and contributors
Copyright (C) 2019 Nuke.YKT

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
#include <string.h>
#include "build.h"
#include "compat.h"
#include "common_game.h"
#include "fx_man.h"

#include "config.h"
#include "gameutil.h"
#include "player.h"
#include "resource.h"
#include "sfx.h"
#include "sound.h"
#include "trig.h"

#define kEarDist (int)((32<<4) * 0.17) // distance between ears (17cm)

static POINT2D earL, earR, earL0, earR0; // Ear position
static VECTOR2D earVL, earVR; // Ear velocity
static int lPhase, rPhase, lVol, rVol, lPitch, rPitch;

BONKLE Bonkle[256];
BONKLE *BonkleCache[256];

int nBonkles;

void sfxInit(void)
{
    for (int i = 0; i < 256; i++)
        BonkleCache[i] = &Bonkle[i];
    nBonkles = 0;
}

void sfxTerm()
{
}

int Vol3d(int angle, int dist)
{
    return dist - mulscale16(dist, 0x2000 - mulscale30(0x2000, Cos(angle)));
}

void Calc3DValues(BONKLE *pBonkle)
{
    int dx = pBonkle->curPos.x - gMe->pSprite->x;
    int dy = pBonkle->curPos.y - gMe->pSprite->y;
    int dz = pBonkle->curPos.z - gMe->pSprite->z;
    int angle = getangle(dx, dy);
    dx >>= 4;
    dy >>= 4;
    dz >>= 8;
    int distance = ksqrt(dx*dx + dy * dy + dz * dz);
    distance = ClipLow((distance >> 2) + (distance >> 3), 64);
    int v14, v18;
    v14 = v18 = scale(pBonkle->vol, 80, distance);
    int sinVal = Sin(angle);
    int cosVal = Cos(angle);
    int v8 = dmulscale30r(cosVal, pBonkle->curPos.x - pBonkle->oldPos.x, sinVal, pBonkle->curPos.y - pBonkle->oldPos.y);

    int distanceL = approxDist(pBonkle->curPos.x - earL.x, pBonkle->curPos.y - earL.y);
    lVol = Vol3d(angle - (gMe->pSprite->ang - 85), v18);
    int phaseLeft = mulscale16r(distanceL, pBonkle->format == 1 ? 4114 : 8228);
    lPitch = scale(pBonkle->pitch, dmulscale30r(cosVal, earVL.dx, sinVal, earVL.dy) + 5853, v8 + 5853);
    lPitch = ClipRange(lPitch, 1000, 50000);

    int distanceR = approxDist(pBonkle->curPos.x - earR.x, pBonkle->curPos.y - earR.y);
    rVol = Vol3d(angle - (gMe->pSprite->ang + 85), v14);
    int phaseRight = mulscale16r(distanceR, pBonkle->format == 1 ? 4114 : 8228);
    rPitch = scale(pBonkle->pitch, dmulscale30r(cosVal, earVR.dx, sinVal, earVR.dy) + 5853, v8 + 5853);
    rPitch = ClipRange(rPitch, 1000, 50000);

    int phaseMin = ClipHigh(phaseLeft, phaseRight);
    lPhase = phaseRight - phaseMin;
    rPhase = phaseLeft - phaseMin;
}

void sfxPlay3DSound(int x, int y, int z, int soundId, int nSector)
{
    if (!SoundToggle || soundId < 0) return;
    
    DICTNODE *hRes = gSoundRes.Lookup(soundId, "SFX");
    if (!hRes)return;

    SFX *pEffect = (SFX*)gSoundRes.Load(hRes);
    hRes = gSoundRes.Lookup(pEffect->rawName, "RAW");
    if (!hRes) return;

    int v1c, v18;
    v1c = v18 = mulscale16(pEffect->pitch, sndGetRate(pEffect->format));
    if (nBonkles >= 256)
        return;
    BONKLE *pBonkle = BonkleCache[nBonkles++];
    pBonkle->pSndSpr = NULL;
    pBonkle->curPos.x = x;
    pBonkle->curPos.y = y;
    pBonkle->curPos.z = z;
    pBonkle->sectnum = nSector;
    FindSector(x, y, z, &pBonkle->sectnum);
    pBonkle->oldPos = pBonkle->curPos;
    pBonkle->sfxId = soundId;
    pBonkle->hSnd = hRes;
    pBonkle->vol = pEffect->relVol;
    pBonkle->pitch = v18;
    pBonkle->format = pEffect->format;
    int size = hRes->size;
    char *pData = (char*)gSoundRes.Lock(hRes);
    Calc3DValues(pBonkle);
    int priority = 1;
    if (priority < lVol)
        priority = lVol;
    if (priority < rVol)
        priority = rVol;
    if (gStereo)
    {
        // MV_Lock();
        pBonkle->lChan = FX_PlayRaw(pData + lPhase, size - lPhase, lPitch, 0, lVol, lVol, 0, priority, fix16_one, (intptr_t)&pBonkle->lChan);
        pBonkle->rChan = FX_PlayRaw(pData + rPhase, size - rPhase, rPitch, 0, rVol, 0, rVol, priority, fix16_one, (intptr_t)&pBonkle->rChan);
        // MV_Unlock();
    }
    else
    {
        pBonkle->lChan = FX_PlayRaw(pData + lPhase, size - lPhase, v1c, 0, lVol, lVol, rVol, priority, fix16_one, (intptr_t)&pBonkle->lChan);
        pBonkle->rChan = 0;
    }
}

void sfxPlay3DSound(spritetype *pSprite, int soundId, int chanId, int nFlags)
{
    if (!SoundToggle)
        return;
    if (!pSprite)
        return;
    if (soundId < 0)
        return;
    DICTNODE *hRes = gSoundRes.Lookup(soundId, "SFX");
    if (!hRes)
        return;

    SFX *pEffect = (SFX*)gSoundRes.Load(hRes);
    hRes = gSoundRes.Lookup(pEffect->rawName, "RAW");
    if (!hRes)
        return;
    int size = hRes->size;
    if (size <= 0)
        return;
    int v14;
    v14 = mulscale16(pEffect->pitch, sndGetRate(pEffect->format));
    BONKLE *pBonkle = NULL;
    if (chanId >= 0)
    {
        int i;
        for (i = 0; i < nBonkles; i++)
        {
            pBonkle = BonkleCache[i];
            if (pBonkle->chanId == chanId && (pBonkle->pSndSpr == pSprite || (nFlags & 1) != 0))
            {
                if ((nFlags & 4) != 0 && pBonkle->chanId == chanId)
                    return;
                if ((nFlags & 2) != 0 && pBonkle->sfxId == soundId)
                    return;
                if (pBonkle->lChan > 0)
                    FX_StopSound(pBonkle->lChan);
                if (pBonkle->rChan > 0)
                    FX_StopSound(pBonkle->rChan);
                if (pBonkle->hSnd)
                {
                    gSoundRes.Unlock(pBonkle->hSnd);
                    pBonkle->hSnd = NULL;
                }
                break;
            }
        }
        if (i == nBonkles)
        {
            if (nBonkles >= 256)
                return;
            pBonkle = BonkleCache[nBonkles++];
        }
        pBonkle->pSndSpr = !(nFlags & 8) ? pSprite : NULL;
        pBonkle->chanId = chanId;
    }
    else
    {
        if (nBonkles >= 256)
            return;
        pBonkle = BonkleCache[nBonkles++];
        pBonkle->pSndSpr = NULL;
    }
    pBonkle->curPos.x = pSprite->x;
    pBonkle->curPos.y = pSprite->y;
    pBonkle->curPos.z = pSprite->z;
    pBonkle->sectnum = pSprite->sectnum;
    pBonkle->oldPos = pBonkle->curPos;
    pBonkle->sfxId = soundId;
    pBonkle->hSnd = hRes;
    pBonkle->vol = pEffect->relVol;
    pBonkle->pitch = v14;
    Calc3DValues(pBonkle);
    int priority = 1;
    if (priority < lVol)
        priority = lVol;
    if (priority < rVol)
        priority = rVol;
    int loopStart = pEffect->loopStart;
    int loopEnd = ClipLow(size - 1, 0);
    if (chanId < 0)
        loopStart = -1;
    // MV_Lock();
    char *pData = (char*)gSoundRes.Lock(hRes);
    if (loopStart >= 0)
    {
        if (gStereo)
        {
            pBonkle->lChan = FX_PlayLoopedRaw(pData + lPhase, size - lPhase, pData + loopStart, pData + loopEnd, lPitch, 0, lVol, lVol, 0, priority, fix16_one, (intptr_t)&pBonkle->lChan);
            pBonkle->rChan = FX_PlayLoopedRaw(pData + rPhase, size - rPhase, pData + loopStart, pData + loopEnd, rPitch, 0, rVol, 0, rVol, priority, fix16_one, (intptr_t)&pBonkle->rChan);
        }
        else
        {
            pBonkle->lChan = FX_PlayLoopedRaw(pData + lPhase, size - lPhase, pData + loopStart, pData + loopEnd, v14, 0, lVol, lVol, rVol, priority, fix16_one, (intptr_t)&pBonkle->lChan);
            pBonkle->rChan = 0;
        }
    }
    else
    {
        pData = (char*)gSoundRes.Lock(pBonkle->hSnd);
        if (gStereo)
        {
            pBonkle->lChan = FX_PlayRaw(pData + lPhase, size - lPhase, lPitch, 0, lVol, lVol, 0, priority, fix16_one, (intptr_t)&pBonkle->lChan);
            pBonkle->rChan = FX_PlayRaw(pData + rPhase, size - rPhase, rPitch, 0, rVol, 0, rVol, priority, fix16_one, (intptr_t)&pBonkle->rChan);
        }
        else
        {
            pBonkle->lChan = FX_PlayRaw(pData + lPhase, size - lPhase, v14, 0, lVol, lVol, rVol, priority, fix16_one, (intptr_t)&pBonkle->lChan);
            pBonkle->rChan = 0;
        }
    }
    // MV_Unlock();
}

// by NoOne: same as previous, but allows to set custom pitch for sound AND volume.
void sfxPlay3DSoundCP(spritetype* pSprite, int soundId, int chanId, int nFlags, int pitch, int volume)
{
    if (!SoundToggle || !pSprite || soundId < 0) return;
    DICTNODE* hRes = gSoundRes.Lookup(soundId, "SFX");
    if (!hRes) return;

    SFX* pEffect = (SFX*)gSoundRes.Load(hRes);
    hRes = gSoundRes.Lookup(pEffect->rawName, "RAW");
    if (!hRes) return;
    int size = hRes->size;
    if (size <= 0) return;
    
    if (pitch <= 0) pitch = pEffect->pitch;
    else pitch -= QRandom(pEffect->pitchRange);

    int v14;
    v14 = mulscale16(pitch, sndGetRate(pEffect->format));
    
    BONKLE * pBonkle = NULL;
    if (chanId >= 0)
    {
        int i;
        for (i = 0; i < nBonkles; i++)
        {
            pBonkle = BonkleCache[i];
            if (pBonkle->chanId == chanId && (pBonkle->pSndSpr == pSprite || (nFlags & 1) != 0))
            {
                if ((nFlags & 4) != 0 && pBonkle->chanId == chanId)
                    return;
                if ((nFlags & 2) != 0 && pBonkle->sfxId == soundId)
                    return;
                if (pBonkle->lChan > 0)
                    FX_StopSound(pBonkle->lChan);
                if (pBonkle->rChan > 0)
                    FX_StopSound(pBonkle->rChan);
                if (pBonkle->hSnd)
                {
                    gSoundRes.Unlock(pBonkle->hSnd);
                    pBonkle->hSnd = NULL;
                }
                break;
            }
        }
        if (i == nBonkles)
        {
            if (nBonkles >= 256)
                return;
            pBonkle = BonkleCache[nBonkles++];
        }
        pBonkle->pSndSpr = !(nFlags & 8) ? pSprite : NULL;
        pBonkle->chanId = chanId;
    }
    else
    {
        if (nBonkles >= 256)
            return;
        pBonkle = BonkleCache[nBonkles++];
        pBonkle->pSndSpr = NULL;
    }
    pBonkle->curPos.x = pSprite->x;
    pBonkle->curPos.y = pSprite->y;
    pBonkle->curPos.z = pSprite->z;
    pBonkle->sectnum = pSprite->sectnum;
    pBonkle->oldPos = pBonkle->curPos;
    pBonkle->sfxId = soundId;
    pBonkle->hSnd = hRes;
    switch (volume)
    {
        case 0:
            pBonkle->vol = pEffect->relVol;
            break;
        case -1:
            pBonkle->vol = 0;
            break;
        default:
            pBonkle->vol = volume;
            break;
    }

    pBonkle->pitch = v14;
    Calc3DValues(pBonkle);
    int priority = 1;
    if (priority < lVol)
        priority = lVol;
    if (priority < rVol)
        priority = rVol;
    int loopStart = pEffect->loopStart;
    int loopEnd = ClipLow(size - 1, 0);
    if (chanId < 0)
        loopStart = -1;
    // MV_Lock();
    char* pData = (char*)gSoundRes.Lock(hRes);
    if (loopStart >= 0)
    {
        if (gStereo)
        {
            pBonkle->lChan = FX_PlayLoopedRaw(pData + lPhase, size - lPhase, pData + loopStart, pData + loopEnd, lPitch, 0, lVol, lVol, 0, priority, fix16_one, (intptr_t)& pBonkle->lChan);
            pBonkle->rChan = FX_PlayLoopedRaw(pData + rPhase, size - rPhase, pData + loopStart, pData + loopEnd, rPitch, 0, rVol, 0, rVol, priority, fix16_one, (intptr_t)& pBonkle->rChan);
        }
        else
        {
            pBonkle->lChan = FX_PlayLoopedRaw(pData + lPhase, size - lPhase, pData + loopStart, pData + loopEnd, v14, 0, lVol, lVol, rVol, priority, fix16_one, (intptr_t)& pBonkle->lChan);
            pBonkle->rChan = 0;
        }
    }
    else
    {
        pData = (char*)gSoundRes.Lock(pBonkle->hSnd);
        if (gStereo)
        {
            pBonkle->lChan = FX_PlayRaw(pData + lPhase, size - lPhase, lPitch, 0, lVol, lVol, 0, priority, fix16_one, (intptr_t)& pBonkle->lChan);
            pBonkle->rChan = FX_PlayRaw(pData + rPhase, size - rPhase, rPitch, 0, rVol, 0, rVol, priority, fix16_one, (intptr_t)& pBonkle->rChan);
        }
        else
        {
            pBonkle->lChan = FX_PlayRaw(pData + lPhase, size - lPhase, v14, 0, lVol, lVol, rVol, priority, fix16_one, (intptr_t)& pBonkle->lChan);
            pBonkle->rChan = 0;
        }
    }
    // MV_Unlock();
}

EXTERN_INLINE void sfxKillSoundInternal(int i)
{
    BONKLE *pBonkle = BonkleCache[i];
    if (pBonkle->lChan > 0)
    {
        FX_EndLooping(pBonkle->lChan);
        FX_StopSound(pBonkle->lChan);
    }
    if (pBonkle->rChan > 0)
    {
        FX_EndLooping(pBonkle->rChan);
        FX_StopSound(pBonkle->rChan);
    }
    if (pBonkle->hSnd)
    {
        gSoundRes.Unlock(pBonkle->hSnd);
        pBonkle->hSnd = NULL;
    }
    BonkleCache[i] = BonkleCache[--nBonkles];
    BonkleCache[nBonkles] = pBonkle;
}

void sfxKill3DSound(spritetype *pSprite, int chanId, int soundId)
{
    if (!pSprite)
        return;
    for (int i = nBonkles - 1; i >= 0; i--)
    {
        BONKLE *pBonkle = BonkleCache[i];
        if (pBonkle->pSndSpr == pSprite && (chanId < 0 || chanId == pBonkle->chanId) && (soundId < 0 || soundId == pBonkle->sfxId))
        {
            sfxKillSoundInternal(i);
            break;
        }
    }
}

void sfxKillAllSounds(void)
{
    for (int i = nBonkles - 1; i >= 0; i--)
    {
        sfxKillSoundInternal(i);
    }
}

void sfxKillSpriteSounds(spritetype *pSprite)
{
    if (!pSprite)
        return;
    for (int i = nBonkles - 1; i >= 0; i--)
    {
        BONKLE *pBonkle = BonkleCache[i];
        if (pBonkle->pSndSpr == pSprite)
        {
            sfxKillSoundInternal(i);
        }
    }
}

inline void sfxUpdateListenerPos(void)
{
    const int dx = mulscale30(Cos(gMe->pSprite->ang + kAng90), kEarDist>>1);
    const int dy = mulscale30(Sin(gMe->pSprite->ang + kAng90), kEarDist>>1);
    earL0 = earL;
    earR0 = earR;
    earL = {gMe->pSprite->x - dx, gMe->pSprite->y - dy};
    earR = {gMe->pSprite->x + dx, gMe->pSprite->y + dy};
}

inline void sfxUpdateListenerVel(void)
{
    earVL = {earL.x - earL0.x, earL.y - earL0.y};
    earVR = {earR.x - earR0.x, earR.y - earR0.y};
}

void sfxCorrectListenerPos(void)
{
    sfxUpdateListenerPos();
    earL0 = earL;
    earR0 = earR;
    earL0.x += -earVL.dx;
    earL0.y += -earVL.dy;
    earR0.x += -earVR.dx;
    earR0.y += -earVR.dy;
}

void sfxResetListener(void)
{
    sfxUpdateListenerPos(); // update ear position
    earVL = earVR = {0, 0}; // reset ear velocity
}

void sfxUpdate3DSounds(void)
{
    sfxUpdateListenerPos();
    sfxUpdateListenerVel();
    for (int i = nBonkles - 1; i >= 0; i--)
    {
        BONKLE *pBonkle = BonkleCache[i];
        if (pBonkle->lChan > 0 || pBonkle->rChan > 0)
        {
            if (!pBonkle->hSnd)
                continue;
            if (pBonkle->pSndSpr)
            {
                pBonkle->oldPos = pBonkle->curPos;
                pBonkle->curPos.x = pBonkle->pSndSpr->x;
                pBonkle->curPos.y = pBonkle->pSndSpr->y;
                pBonkle->curPos.z = pBonkle->pSndSpr->z;
                pBonkle->sectnum = pBonkle->pSndSpr->sectnum;
            }
            Calc3DValues(pBonkle);
            // MV_Lock();
            if (pBonkle->lChan > 0)
            {
                if (pBonkle->rChan > 0)
                {
                    FX_SetPan(pBonkle->lChan, lVol, lVol, 0);
                    FX_SetFrequency(pBonkle->lChan, lPitch);
                }
                else
                    FX_SetPan(pBonkle->lChan, lVol, lVol, rVol);
            }
            if (pBonkle->rChan > 0)
            {
                FX_SetPan(pBonkle->rChan, rVol, 0, rVol);
                FX_SetFrequency(pBonkle->rChan, rPitch);
            }
            // MV_Unlock();
        }
        else
        {
            gSoundRes.Unlock(pBonkle->hSnd);
            pBonkle->hSnd = NULL;
            BonkleCache[i] = BonkleCache[--nBonkles];
            BonkleCache[nBonkles] = pBonkle;
        }
    }
}

void sfxSetReverb(bool toggle)
{
    if (toggle)
    {
        FX_SetReverb(128);
        FX_SetReverbDelay(10);
    }
    else
        FX_SetReverb(0);
}

void sfxSetReverb2(bool toggle)
{
    if (toggle)
    {
        FX_SetReverb(128);
        FX_SetReverbDelay(20);
    }
    else
        FX_SetReverb(0);
}
