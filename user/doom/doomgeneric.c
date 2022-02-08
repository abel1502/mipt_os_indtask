#include "doomgeneric.h"

uint32_t* DG_ScreenBuffer = 0;
int LAUNCH_TIME = 0;


void dg_Create()
{
	DG_ScreenBuffer = libc_malloc(DOOMGENERIC_RESX * DOOMGENERIC_RESY * 4);

	DG_Init();
}

extern int main(int argc, char **argv);

void umain(int argc, char **argv) {
	main(argc, argv);
}


void DG_Init() {
	
}


void DG_DrawFrame() {

}


void DG_SleepMs(uint32_t ms) {

}


uint32_t DG_GetTicksMs() {
	return 0;
}


int DG_GetKey(int* pressed, unsigned char* key) {
	return 0;
}


void DG_SetWindowTitle(const char * title) {

}
