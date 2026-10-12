#ifndef TEST_SYSTEMCTRL_H
#define TEST_SYSTEMCTRL_H
#include <stdint.h>
#include "pspkernel.h"
typedef int (*STMOD_HANDLER)(SceModule *module);
uintptr_t sctrlHENFindFunction(const char *module, const char *library, u32 nid);
#define FindProc sctrlHENFindFunction
void sctrlHENPatchSyscall(void *original, void *replacement);
STMOD_HANDLER sctrlHENSetStartModuleHandler(STMOD_HANDLER handler);
#endif
