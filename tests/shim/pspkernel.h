#ifndef TEST_PSPKERNEL_H
#define TEST_PSPKERNEL_H
#include "pspsdk.h"
typedef int SceUID;
typedef unsigned int SceSize;
typedef struct { char modname[32]; } SceModule;
#define PSP_MODULE_INFO(name, flags, major, minor)
#define PSP_O_WRONLY 1
#define PSP_O_CREAT 2
#define PSP_SEEK_END 2
int sceIoOpen(const char *path, int flags, int permissions);
int sceIoLseek(int fd, int offset, int origin);
int sceIoWrite(int fd, const void *data, unsigned int size);
int sceIoClose(int fd);
void sceKernelDcacheWritebackInvalidateAll(void);
void sceKernelIcacheClearAll(void);
int sceKernelDelayThread(unsigned int duration);
unsigned int sceKernelGetSystemTimeLow(void);
#endif
