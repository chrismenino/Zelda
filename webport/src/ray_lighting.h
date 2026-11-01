#pragma once
#include "types.h"
void RayLighting_Init(void);
void RayLighting_Toggle(void);
void RayLighting_CycleStyle(void);
const char *RayLighting_Status(void);
void RayLighting_Apply(uint8 *pixels, int pitch, int width, int height, int scale);
