#include <pspsdk.h>
#include <pspkernel.h>
#include <pspge.h>
#include <pspgu.h>

#include <stdio.h>
#include <string.h>

#include <systemctrl.h>

#include "ge_constants.h"

PSP_MODULE_INFO("GePatch", 0x1007, 1, 0);

#define DRAW_NATIVE 0xABCDEF00

#define PITCH 960
#define WIDTH 960
#define HEIGHT 544
#define PIXELFORMAT 0

#define DISPLAY_BUFFER 0x0A000000
#define FAKE_VRAM 0x0A200000
#define VERTICES_BUFFER 0x0A400000
#define RENDER_LIST 0x0A800000

#define VRAM 0x04000000

#define VRAM_DRAW_BUFFER_OFFSET 0
#define VRAM_DEPTH_BUFFER_OFFSET 0x00100000

#define VRAM_1KB 0x001ff000

#define log(...) \
{ \
  char msg[256]; \
  sprintf(msg,__VA_ARGS__); \
  logmsg(msg); \
}

void logmsg(char *msg) {
  int k1 = pspSdkSetK1(0);

  SceUID fd = sceIoOpen("ms0:/ge_patch.txt", PSP_O_WRONLY | PSP_O_CREAT, 0777);
  if (fd >= 0) {
    sceIoLseek(fd, 0, PSP_SEEK_END);
    sceIoWrite(fd, msg, strlen(msg));
    sceIoClose(fd);
  }

  pspSdkSetK1(k1);
}

static u32 stack[0x10000];
static u32 curr_stack = 0;

static void push(u32 data) {
  if (curr_stack < (sizeof(stack) / sizeof(u32)))
    stack[curr_stack++] = data;
}

static u32 pop() {
  if (curr_stack > 0)
    return stack[--curr_stack];
  return 0;
}

static const u8 tcsize[4] = { 0, 2, 4, 8 }, tcalign[4] = { 0, 1, 2, 4 };
static const u8 colsize[8] = { 0, 0, 0, 0, 2, 2, 2, 4 }, colalign[8] = { 0, 0, 0, 0, 2, 2, 2, 4 };
static const u8 nrmsize[4] = { 0, 3, 6, 12 }, nrmalign[4] = { 0, 1, 2, 4 };
static const u8 possize[4] = { 3, 3, 6, 12 }, posalign[4] = { 1, 1, 2, 4 };
static const u8 wtsize[4] = { 0, 1, 2, 4 }, wtalign[4] = { 0, 1, 2, 4 };

#define ALIGN(x, align) (((x) + ((align) - 1)) & ~((align) - 1))

void getVertexSizeAndPositionOffset(u32 op, u8 *vertex_size, u8 *pos_off) {
  int tc = (op & GE_VTYPE_TC_MASK) >> GE_VTYPE_TC_SHIFT;
  int col = (op & GE_VTYPE_COL_MASK) >> GE_VTYPE_COL_SHIFT;
  int nrm = (op & GE_VTYPE_NRM_MASK) >> GE_VTYPE_NRM_SHIFT;
  int pos = (op & GE_VTYPE_POS_MASK) >> GE_VTYPE_POS_SHIFT;
  int weight = (op & GE_VTYPE_WEIGHT_MASK) >> GE_VTYPE_WEIGHT_SHIFT;
  int weightCount = ((op & GE_VTYPE_WEIGHTCOUNT_MASK) >> GE_VTYPE_WEIGHTCOUNT_SHIFT) + 1;
  int morphCount = ((op & GE_VTYPE_MORPHCOUNT_MASK) >> GE_VTYPE_MORPHCOUNT_SHIFT) + 1;

  u8 biggest = 0;
  u8 size = 0;
  u8 weightoff = 0, tcoff = 0, coloff = 0, nrmoff = 0, posoff = 0;

  if (weight) {
    size = ALIGN(size, wtalign[weight]);
    weightoff = size;
    size += wtsize[weight] * weightCount;
    if (wtalign[weight] > biggest)
      biggest = wtalign[weight];
  }

  if (tc) {
    size = ALIGN(size, tcalign[tc]);
    tcoff = size;
    size += tcsize[tc];
    if (tcalign[tc] > biggest)
      biggest = tcalign[tc];
  }

  if (col) {
    size = ALIGN(size, colalign[col]);
    coloff = size;
    size += colsize[col];
    if (colalign[col] > biggest)
      biggest = colalign[col];
  }

  if (nrm) {
    size = ALIGN(size, nrmalign[nrm]);
    nrmoff = size;
    size += nrmsize[nrm];
    if (nrmalign[nrm] > biggest)
      biggest = nrmalign[nrm];
  }

  if (pos) {
    size = ALIGN(size, posalign[pos]);
    posoff = size;
    size += possize[pos];
    if (posalign[pos] > biggest)
      biggest = posalign[pos];
  }

  size = ALIGN(size, biggest);
  size *= morphCount;

  *vertex_size = size;
  *pos_off = posoff;
}

void patchGeList(u32 *list, u32 *stall) {
  union {
    float f;
    unsigned int i;
  } t;

  u32 base = 0;
  u32 offset = 0;
  u32 address = 0;
  u32 vertex_addr = 0;
  u32 vertex_type = 0;

  while (!stall || (stall && list != stall)) {
    u32 op = *list;
    u32 cmd = op >> 24;
    u32 data = op & 0xffffff;

    switch (cmd) {
      case GE_CMD_BASE:
        base = (data << 8) & 0x0f000000;
        break;
      case GE_CMD_OFFSETADDR:
        offset = data << 8;
        break;
      case GE_CMD_ORIGIN:
        offset = (u32)list;
        break;

      case GE_CMD_CALL:
        address = ((base | data) + offset) & 0x0ffffffc;
        push(address);
        break;

      case GE_CMD_BJUMP:
        address = ((base | data) + offset) & 0x0ffffffc;
        push(address);
        return; // MAYBE break? returns seems to work with MUA

      case GE_CMD_JUMP:
        address = ((base | data) + offset) & 0x0ffffffc;
        push(address);
        return;

      case GE_CMD_RET:
      case GE_CMD_END:
        return;

      case GE_CMD_VADDR:
        vertex_addr = ((base | data) + offset) & 0x0fffffff;
        break;

      case GE_CMD_VERTEXTYPE:
        vertex_type = data;
        break;

      case GE_CMD_BEZIER:
      case GE_CMD_SPLINE:
      {
        u8 num_points_u = data & 0xff;
        u8 num_points_v = (data >> 8) & 0xff;

        u8 vertex_size = 0, pos_off = 0;
        getVertexSizeAndPositionOffset(vertex_type, &vertex_size, &pos_off);

        if ((vertex_type & GE_VTYPE_IDX_MASK) == GE_VTYPE_IDX_NONE) {
          vertex_addr += (num_points_u * num_points_v) * vertex_size;
        }

        break;
      }

      case GE_CMD_BOUNDINGBOX:
      {
        u32 count = data;

        u8 vertex_size = 0, pos_off = 0;
        getVertexSizeAndPositionOffset(vertex_type, &vertex_size, &pos_off);

        if ((vertex_type & GE_VTYPE_IDX_MASK) == GE_VTYPE_IDX_NONE) {
          vertex_addr += count * vertex_size;
        }

        break;
      }

      case GE_CMD_PRIM:
      {
        u16 count = data & 0xffff;
        // u8 type = (data >> 16) & 7;

        u8 vertex_size = 0, pos_off = 0;
        getVertexSizeAndPositionOffset(vertex_type, &vertex_size, &pos_off);

        int through = (vertex_type & GE_VTYPE_THROUGH_MASK) == GE_VTYPE_THROUGH;
        if (through) {
          int pos = (vertex_type & GE_VTYPE_POS_MASK) >> GE_VTYPE_POS_SHIFT;
          int pos_size = possize[pos] / 3;

          // TODO: we may patch the same vertex again and again...
          int i;
          for (i = 0; i < count; i++) {
            int j;
            for (j = 0; j < 2; j++) {
              u32 addr = vertex_addr + i * vertex_size + pos_off + j * pos_size;
              switch (pos_size) {
                case 1:
                  *(u8 *)addr *= 2;
                  break;
                case 2:
                  // This works for Marvel Ultimate Alliance
                  // if (*(u16 *)addr == 480)
                    // *(u16 *)addr = 960;
                  // else if (*(u16 *)addr == 272)
                    // *(u16 *)addr = 544;
                  *(u16 *)addr *= 2;
                  break;
                case 4:
                  t.i = *(u32 *)addr;
                  t.f *= 2;
                  *(u32 *)addr = t.i;
                  break;
              }
            }
          }
        }

        if ((vertex_type & GE_VTYPE_IDX_MASK) == GE_VTYPE_IDX_NONE) {
          vertex_addr += count * vertex_size;
        }

        break;
      }

      case GE_CMD_FRAMEBUFPIXFORMAT:
        *list = (cmd << 24) | GE_FORMAT_565;
        break;

      case GE_CMD_FRAMEBUFPTR:
        *list = (cmd << 24) | (VRAM_DRAW_BUFFER_OFFSET & 0xffffff);
        break;
      case GE_CMD_FRAMEBUFWIDTH:
        if ((data & 0xffff) == 512) {
          *list = (cmd << 24) | ((VRAM_DRAW_BUFFER_OFFSET >> 24) << 16) | PITCH;
        } else {
          // Dummy draw
          if ((*(list+1) >> 24) == GE_CMD_FRAMEBUFPTR) {
            *list = (cmd << 24) | ((VRAM_1KB >> 24) << 16) | 0;
            *(list+1) = (GE_CMD_FRAMEBUFPTR << 24) | (VRAM_1KB & 0xffffff);
            list++;
          } else if ((*(list-1) >> 24) == GE_CMD_FRAMEBUFPTR) {
            *(list-1) = (GE_CMD_FRAMEBUFPTR << 24) | (VRAM_1KB & 0xffffff);
            *list = (cmd << 24) | ((VRAM_1KB >> 24) << 16) | 0;
          }
        }
        break;
      case GE_CMD_ZBUFPTR:
        *list = (cmd << 24) | (VRAM_DEPTH_BUFFER_OFFSET & 0xffffff);
        break;
      case GE_CMD_ZBUFWIDTH:
        *list = (cmd << 24) | ((VRAM_DEPTH_BUFFER_OFFSET >> 24) << 16) | PITCH;
        break;

      case GE_CMD_VIEWPORTXSCALE:
        t.i = data << 8;
        t.f = (t.f < 0) ? -(WIDTH / 2) : (WIDTH / 2);
        *list = (cmd << 24) | (t.i >> 8);
        break;
      case GE_CMD_VIEWPORTYSCALE:
        t.i = data << 8;
        t.f = (t.f < 0) ? -(HEIGHT / 2) : (HEIGHT / 2);
        *list = (cmd << 24) | (t.i >> 8);
        break;
      case GE_CMD_VIEWPORTXCENTER:
        t.f = 2048;
        *list = (cmd << 24) | (t.i >> 8);
        break;
      case GE_CMD_VIEWPORTYCENTER:
        t.f = 2048;
        *list = (cmd << 24) | (t.i >> 8);
        break;
      case GE_CMD_OFFSETX:
        *list = (cmd << 24) | ((2048 - (WIDTH / 2)) << 4);
        break;
      case GE_CMD_OFFSETY:
        *list = (cmd << 24) | ((2048 - (HEIGHT / 2)) << 4);
        break;

      case GE_CMD_REGION2:
      case GE_CMD_SCISSOR2:
        *list = (cmd << 24) | ((HEIGHT - 1) << 10) | (WIDTH - 1);
        break;

      default:
        break;
    }

    list++;
  }
}

void iterateGeList(u32 list, u32 stall) {
  push(list);

  do {
    patchGeList((u32 *)pop(), (u32 *)stall);
  } while (curr_stack > 0);

  sceKernelDcacheWritebackInvalidateAll();
}

int (* _sceGeGetList)(int qid, void *list, int *flag);
int (* _sceGeListSync)(int qid, int syncType);
int (* _sceGeListUpdateStallAddr)(int qid, void *stall);
int (* _sceGeListEnQueue)(const void *list, void *stall, int cbid, PspGeListArgs *arg);
int (* _sceGeListEnQueueHead)(const void *list, void *stall, int cbid, PspGeListArgs *arg);
int (* _sceGeDrawSync)(int syncType);
void *(* _sceGeEdramGetAddr)(void);

int (* _sceDisplaySetFrameBuf)(void *topaddr, int bufferwidth, int pixelformat, int sync);

int sceGeListUpdateStallAddrPatched(int qid, void *stall) {
  int k1 = pspSdkSetK1(0);
  char info[64];
  if (_sceGeGetList(qid, info, NULL) == 0) {
    void *list = *(void **)(info + 0x18); // stall
    if (!list)
      list = *(void **)(info + 0x14); // list
    iterateGeList((u32)list & 0x0fffffff, (u32)stall & 0x0fffffff);
  }
  pspSdkSetK1(k1);
  return _sceGeListUpdateStallAddr(qid, stall);
}

int sceGeListEnQueuePatched(const void *list, void *stall, int cbid, PspGeListArgs *arg) {
  int k1 = pspSdkSetK1(0);
  iterateGeList((u32)list & 0x0fffffff, (u32)stall & 0x0fffffff);
  pspSdkSetK1(k1);
  return _sceGeListEnQueue(list, stall, cbid, arg);
}

int sceGeListEnQueueHeadPatched(const void *list, void *stall, int cbid, PspGeListArgs *arg) {
  int k1 = pspSdkSetK1(0);
  iterateGeList((u32)list & 0x0fffffff, (u32)stall & 0x0fffffff);
  pspSdkSetK1(k1);
  return _sceGeListEnQueueHead(list, stall, cbid, arg);
}

int sceGeDrawSyncPatched(int syncType) {
  return _sceGeDrawSync(syncType);
}

void *sceGeEdramGetAddrPatched(void) {
  return (void *)FAKE_VRAM;
}

int sceDisplaySetFrameBufPatched(void *topaddr, int bufferwidth, int pixelformat, int sync) {
  *(u32 *)DRAW_NATIVE = 1;

  sceGuStart(0, (void *)(RENDER_LIST | 0xA0000000));
  sceGuTexSync();
  sceGuTexImage(0, 0, 0, 0, (void *)(VRAM + VRAM_DRAW_BUFFER_OFFSET));
  sceGuCopyImage(PIXELFORMAT, 0, 0, WIDTH, HEIGHT, PITCH,
                 (void *)(0x40000000 | (VRAM + VRAM_DRAW_BUFFER_OFFSET)),
                 0, 0, PITCH, (void *)(0x40000000 | DISPLAY_BUFFER));
  sceGuTexSync();
  sceGuFinish();
  _sceGeListEnQueue((void *)((u32)RENDER_LIST | 0x40000000), NULL, 0, NULL);

  return _sceDisplaySetFrameBuf(topaddr, bufferwidth, pixelformat, sync);
}

int module_start(SceSize args, void *argp) {
  _sceGeGetList = (void *)sctrlHENFindFunction("sceGE_Manager", "sceGe_driver", 0x67B01D8E);
  _sceGeListSync = (void *)sctrlHENFindFunction("sceGE_Manager", "sceGe_driver", 0x03444EB4);
  _sceGeListUpdateStallAddr = (void *)sctrlHENFindFunction("sceGE_Manager", "sceGe_driver", 0xE0D68148);
  _sceGeListEnQueue = (void *)sctrlHENFindFunction("sceGE_Manager", "sceGe_driver", 0xAB49E76A);
  _sceGeListEnQueueHead = (void *)sctrlHENFindFunction("sceGE_Manager", "sceGe_driver", 0x1C0D95A6);
  _sceGeDrawSync = (void *)sctrlHENFindFunction("sceGE_Manager", "sceGe_driver", 0xB287BD61);
  _sceGeEdramGetAddr = (void *)sctrlHENFindFunction("sceGE_Manager", "sceGe_driver", 0xE47E40E4);

  sctrlHENPatchSyscall((u32)_sceGeListUpdateStallAddr, sceGeListUpdateStallAddrPatched);
  sctrlHENPatchSyscall((u32)_sceGeListEnQueue, sceGeListEnQueuePatched);
  sctrlHENPatchSyscall((u32)_sceGeListEnQueueHead, sceGeListEnQueueHeadPatched);
  // sctrlHENPatchSyscall((u32)_sceGeDrawSync, sceGeDrawSyncPatched);
  sctrlHENPatchSyscall((u32)_sceGeEdramGetAddr, sceGeEdramGetAddrPatched);

  _sceDisplaySetFrameBuf = (void *)sctrlHENFindFunction("sceDisplay_Service", "sceDisplay_driver", 0x289D82FE);
  sctrlHENPatchSyscall((u32)_sceDisplaySetFrameBuf, sceDisplaySetFrameBufPatched);

  sceKernelDcacheWritebackInvalidateAll();
  sceKernelIcacheClearAll();

  return 0;
}
