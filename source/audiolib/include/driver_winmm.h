/*
 Copyright (C) 2009 Jonathon Fowler <jf@jonof.id.au>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

 See the GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

 */

#include "osd.h"
#include "midifuncs.h"
#include "windows_inc.h"

#define WINMM_NOTE_OFF         0x80
#define WINMM_NOTE_ON          0x90
#define WINMM_POLY_AFTER_TCH   0xA0
#define WINMM_CONTROL_CHANGE   0xB0
#define WINMM_PROGRAM_CHANGE   0xC0
#define WINMM_AFTER_TOUCH      0xD0
#define WINMM_PITCH_BEND       0xE0

enum
{
    WinMMErr_Error = -1,
    WinMMErr_Ok    = 0,
    WinMMErr_MIDIStreamOpen,
    WinMMErr_MIDIStreamRestart,
    WinMMErr_MIDICreateEvent,
    WinMMErr_MIDIPlayThread,
    WinMMErr_MIDICreateMutex
};

extern UINT WinMM_DeviceID;

int WinMMDrv_GetError(void);
const char *WinMMDrv_ErrorString( int ErrorNumber );

int  WinMMDrv_MIDI_Init(midifuncs *);
void WinMMDrv_MIDI_Shutdown(void);
int  WinMMDrv_MIDI_StartPlayback(void);
void WinMMDrv_MIDI_HaltPlayback(void);
void WinMMDrv_MIDI_SetTempo(int tempo, int division);
void WinMMDrv_MIDI_Lock(void);
void WinMMDrv_MIDI_Unlock(void);
void WinMMDrv_MIDI_Service(void);

void WinMMDrv_MIDI_PrintDevices(void);
int WinMMDrv_MIDI_GetNumDevices(void);

int WinMMDrv_MIDI_PrintBufferInfo(osdcmdptr_t);
