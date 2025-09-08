#ifndef SDL_INC_H_
#define SDL_INC_H_

// include this before SDL does
#ifdef _WIN32
# include "windows_inc.h"
#endif

// Workaround for i686-MinGW-w64.
#if defined __MINGW64_VERSION_MAJOR && !defined __MINGW64__
# define __MINGW64_VERSION_MAJOR_BACKUP __MINGW64_VERSION_MAJOR
# undef __MINGW64_VERSION_MAJOR
#endif

#if defined SDL_USEFOLDER
# if SDL_TARGET == 2
#  include <SDL2/SDL.h>
#  include <SDL2/SDL_syswm.h>
#  include <SDL2/SDL_vulkan.h>
# else
#  include <SDL/SDL.h>
#  include <SDL/SDL_syswm.h>
# endif
#else
# include "SDL.h"
# if !defined __APPLE__
#  include "SDL_syswm.h"
# endif
#endif

#if defined __MINGW64_VERSION_MAJOR_BACKUP && !defined __MINGW64__
# define __MINGW64_VERSION_MAJOR __MINGW64_VERSION_MAJOR_BACKUP
# undef __MINGW64_VERSION_MAJOR_BACKUP
#endif

/* =================================================================
Minimum required SDL versions:
=================================================================== */

#define SDL_MIN_X	1
#define SDL_MIN_Y	2
#define SDL_MIN_Z	10
#define SDL_REQUIREDVERSION	(SDL_VERSIONNUM(SDL_MIN_X,SDL_MIN_Y,SDL_MIN_Z))

#define SDL2_MIN_X  2
#define SDL2_MIN_Y  0
#define SDL2_MIN_Z  5
#define SDL2_REQUIREDVERSION (SDL_VERSIONNUM(SDL2_MIN_X,SDL2_MIN_Y,SDL2_MIN_Z))

#if !(SDL_VERSION_ATLEAST(SDL_MIN_X,SDL_MIN_Y,SDL_MIN_Z))
#error SDL version found is too old
#endif

#endif	/* SDL_INC_H_ */
