#ifndef TEST_PSPCTRL_H
#define TEST_PSPCTRL_H
typedef struct { unsigned int Buttons; } SceCtrlData;
#define PSP_CTRL_LTRIGGER 0x100
int sceCtrlPeekBufferPositive(SceCtrlData *pad, int count);
#endif
