#pragma once
#include "compat.h"

bool vksurface_initialize(vec2_t res);
void vksurface_destroy();
void* vksurface_getBuffer();
void vksurface_setPalette(void* pal);
void vksurface_blitBuffer();
