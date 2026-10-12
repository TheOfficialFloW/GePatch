#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <sys/mman.h>
#include "../main.c"

static int copies, displayed_native, display_result, conversion_result;
static unsigned int clock_us = 1000;
static STMOD_HANDLER module_handler;
static int (*conversion_hook)(void *, void *, int, void *);
static unsigned int controller_buttons;
static int syscall_patches;

int pspSdkSetK1(int value) { return 0; }
int sceIoOpen(const char *path, int flags, int permissions) { return -1; }
int sceIoLseek(int fd, int offset, int origin) { return -1; }
int sceIoWrite(int fd, const void *data, unsigned int size) { return -1; }
int sceIoClose(int fd) { return 0; }
void sceKernelDcacheWritebackInvalidateAll(void) {}
void sceKernelIcacheClearAll(void) {}
int sceKernelDelayThread(unsigned int duration) { return 0; }
unsigned int sceKernelGetSystemTimeLow(void) { return clock_us; }
int sceCtrlPeekBufferPositive(SceCtrlData *pad, int count) { pad->Buttons = controller_buttons; return count; }
int sceGuStart(int context, void *list) { copies++; return 0; }
void sceGuCopyImage(int format, int sx, int sy, int width, int height,
                   int source_width, void *source, int dx, int dy,
                   int destination_width, void *destination) {}
int sceGuFinish(void) { return 0; }
static int enqueue(const void *list, void *stall, int cbid, PspGeListArgs *args) { return 1; }
static int display(void *address, int width, int format, int sync) {
  displayed_native = *(u32 *)DRAW_NATIVE;
  return display_result;
}
static int convert(void *rgb, void *rgb2, int width, void *yuv) { return conversion_result; }
uintptr_t sctrlHENFindFunction(const char *module, const char *library, u32 nid) {
  if (nid == 0x91929A21) return (uintptr_t)convert;
  if (nid == 0xCE8EB837 || nid == 0x304882E1) return 0;
  if (nid == 0x289D82FE) return (uintptr_t)display;
  return (uintptr_t)enqueue;
}
void sctrlHENPatchSyscall(void *original, void *replacement) {
  syscall_patches++;
  if (original == (void *)convert) conversion_hook = replacement;
}
STMOD_HANDLER sctrlHENSetStartModuleHandler(STMOD_HANDLER handler) {
  STMOD_HANDLER previous = module_handler;
  module_handler = handler;
  return previous;
}

static void present(void) {
  assert(sceDisplaySetFrameBufPatched((void *)0x0A000000, 512, 3, 1) == display_result);
}
static void geometry(void) {
  u32 *list = (void *)0x08000000;
  list[0] = (GE_CMD_FRAMEBUFPTR << 24);
  list[1] = (GE_CMD_FRAMEBUFWIDTH << 24) | 0x0A0000 | 512;
  list[2] = (GE_CMD_VERTEXTYPE << 24) | 0x180;
  list[3] = (GE_CMD_VADDR << 24) | 0x001000;
  list[4] = (GE_CMD_BASE << 24) | 0x080000;
  list[5] = (GE_CMD_PRIM << 24) | (3 << 16) | 3;
  list[6] = (GE_CMD_FINISH << 24);
  list[7] = (GE_CMD_END << 24);
  sceGeListEnQueuePatched(list, NULL, -1, NULL);
}

int main(void) {
  assert(mmap((void *)0xABCDE000, 4096, PROT_READ | PROT_WRITE,
              MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0) != MAP_FAILED);
  assert(mmap((void *)0x08000000, 65536, PROT_READ | PROT_WRITE,
              MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0) != MAP_FAILED);
  controller_buttons = PSP_CTRL_LTRIGGER;
  assert(module_start(0, NULL) == 0);
  assert(syscall_patches == 0 && !conversion_hook && !module_handler);
  puts("PASS: upstream L-trigger bypass does not install any hooks");
  controller_buttons = 0;
  module_start(0, NULL);
  present();
  assert(displayed_native == 0 && copies == 0);
  puts("PASS: direct framebuffer before native geometry");

  geometry();
  present();
  assert(displayed_native == 1 && copies == 1);
  puts("PASS: game geometry enables native presentation");
  present();
  assert(displayed_native == 1);
  puts("PASS: unchanged native frame remains native");

  assert(conversion_hook && module_handler);
  SceModule mpeg_module = { "sceMpegbase_Driver" };
  module_handler(&mpeg_module);
  void *frame = (void *)0x0A000000;
  conversion_result = -1;
  assert(conversion_hook(frame, NULL, 512, NULL) == -1);
  present();
  assert(displayed_native == 1);
  puts("PASS: failed conversion preserves native rendering");

  conversion_result = 0;
  assert(conversion_hook(frame, NULL, 512, NULL) == 0);
  copies = 0;
  present();
  assert(displayed_native == 0 && copies == 0);
  geometry();
  assert(*(u32 *)DRAW_NATIVE == 0 && copies == 0);
  puts("PASS: converted video is not hidden by display or cached-list copying");

  u32 movie_list[] = { (GE_CMD_VIEWPORTXSCALE << 24) | 0x437000 };
  u32 before = movie_list[0];
  patchGeList(movie_list, movie_list + 1);
  assert(movie_list[0] == before);
  puts("PASS: movie list is not rewritten to native dimensions");

  clock_us += 1000001;
  present();
  assert(displayed_native == 0);
  geometry();
  present();
  assert(displayed_native == 1);
  puts("PASS: native output returns after video idle and fresh geometry");

  conversion_hook(frame, NULL, 512, NULL);
  clock_us += 1000001;
  geometry();
  present();
  assert(displayed_native == 1);
  puts("PASS: idle decoder does not keep game in video mode");
  return 0;
}
