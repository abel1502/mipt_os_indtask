#include "doomgeneric.h"

uint32_t* DG_ScreenBuffer = 0;


void dg_Create()
{
	DG_ScreenBuffer = libc_malloc(DOOMGENERIC_RESX * DOOMGENERIC_RESY * 4);

	DG_Init();
}

