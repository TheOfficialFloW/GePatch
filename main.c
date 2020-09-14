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

void GetIndexBounds(const void *inds, int count, u32 vertType, u16 *indexLowerBound, u16 *indexUpperBound) {
  int i;
  int lowerBound = 0x7FFFFFFF;
  int upperBound = 0;
  u32 idx = vertType & GE_VTYPE_IDX_MASK;
  if (idx == GE_VTYPE_IDX_8BIT) {
    const u8 *ind8 = (const u8 *)inds;
    for (i = 0; i < count; i++) {
      u8 value = ind8[i];
      if (value > upperBound)
        upperBound = value;
      if (value < lowerBound)
        lowerBound = value;
    }
  } else if (idx == GE_VTYPE_IDX_16BIT) {
    const u16 *ind16 = (const u16 *)inds;
    for (i = 0; i < count; i++) {
      u16 value = ind16[i];
      if (value > upperBound)
        upperBound = value;
      if (value < lowerBound)
        lowerBound = value;
    }
  } else if (idx == GE_VTYPE_IDX_32BIT) {
    const u32 *ind32 = (const u32 *)inds;
    for (i = 0; i < count; i++) {
      u16 value = (u16)ind32[i];
      if (value > upperBound)
        upperBound = value;
      if (value < lowerBound)
        lowerBound = value;
    }
  } else {
    lowerBound = 0;
    upperBound = count - 1;
  }
  *indexLowerBound = (u16)lowerBound;
  *indexUpperBound = (u16)upperBound;
}

void AdvanceVerts(u32 *index_addr, u32 *vertex_addr, u32 vertex_type, int count, int vertex_size) {
  if ((vertex_type & GE_VTYPE_IDX_MASK) != GE_VTYPE_IDX_NONE) {
    int index_shift = ((vertex_type & GE_VTYPE_IDX_MASK) >> GE_VTYPE_IDX_SHIFT) - 1;
    (*index_addr) += count << index_shift;
  } else {
    (*vertex_addr) += count * vertex_size;
  }
}

typedef struct {
  u32 list;
  u32 offset;
} StackEntry;

static int rendered_in_sync = 0;
static int framebuf_set = 0;

static u32 base = 0;
static u32 offset = 0;
static u32 address = 0;
static u32 index_addr = 0;
static u32 vertex_addr = 0;
static u32 vertex_type = 0;

static StackEntry stack[64];
static u32 curr_stack = 0;

static u32 finished = 0;

static int push(StackEntry data) {
  if (curr_stack < (sizeof(stack) / sizeof(StackEntry))) {
    stack[curr_stack++] = data;
    return 0;
  }
  return -1;
}

static StackEntry pop() {
  if (curr_stack > 0)
    return stack[--curr_stack];
  StackEntry stack_entry;
  stack_entry.list = -1;
  stack_entry.offset = -1;
  return stack_entry;
}

void resetGeGlobals() {
  base = 0;
  offset = 0;
  address = 0;
  index_addr = 0;
  vertex_addr = 0;
  vertex_type = 0;
  curr_stack = 0;
  finished = 0;
}

void patchGeList(u32 *list, u32 *stall) {
  union {
    float f;
    unsigned int i;
  } t;

  StackEntry stack_entry;

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

      // TODO: need to save other states, too?
      case GE_CMD_CALL:
        address = ((base | data) + offset) & 0x0ffffffc;
        stack_entry.list = (u32)list;
        stack_entry.offset = offset;
        if (push(stack_entry) == 0)
          list = (u32 *)(address - 4);
        break;

      // TODO: is it okay to always take the branch?
      case GE_CMD_BJUMP:
        address = ((base | data) + offset) & 0x0ffffffc;
        list = (u32 *)(address - 4);
        break;

      case GE_CMD_JUMP:
        address = ((base | data) + offset) & 0x0ffffffc;
        list = (u32 *)(address - 4);
        break;

      case GE_CMD_RET:
      {
        // Ignore returns when the stack is empty
        stack_entry = pop();
        if (stack_entry.list != -1) {
          list = (u32 *)stack_entry.list;
          offset = stack_entry.offset;
        }
        break;
      }

      case GE_CMD_END:
      {
        u32 prev = *(list-1);
        switch (prev >> 24) {
          // TODO: understand how signals are handled
          case GE_CMD_SIGNAL:
          {
            // u8 behaviour = (prev >> 16) & 0xff;
            // u16 signal = prev & 0xffff;
            // u16 enddata = data & 0xffff;
            break;
          }
          case GE_CMD_FINISH:
            resetGeGlobals();
            finished = 1;
            return;
          default:
            break;
        }
        break;
      }

      case GE_CMD_IADDR:
        index_addr = ((base | data) + offset) & 0x0fffffff;
        break;

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

        u32 count = num_points_u * num_points_v;

        u8 vertex_size = 0, pos_off = 0;
        getVertexSizeAndPositionOffset(vertex_type, &vertex_size, &pos_off);

        AdvanceVerts(&index_addr, &vertex_addr, vertex_type, count, vertex_size);

        break;
      }

      case GE_CMD_BOUNDINGBOX:
      {
        u32 count = data;

        u8 vertex_size = 0, pos_off = 0;
        getVertexSizeAndPositionOffset(vertex_type, &vertex_size, &pos_off);

        AdvanceVerts(&index_addr, &vertex_addr, vertex_type, count, vertex_size);

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
          u16 lower = 0;
          u16 upper = count;
          if ((vertex_type & GE_VTYPE_IDX_MASK) != GE_VTYPE_IDX_NONE) {
            lower = 0;
            upper = 0;
            GetIndexBounds((void *)index_addr, count, vertex_type, &lower, &upper);
          }

          int i;
          for (i = 0; i < count; i++) {
            int j;
            for (j = 0; j < 2; j++) {
              u32 addr = vertex_addr + i * vertex_size + pos_off + j * pos_size;
              switch (pos_size) {
                case 2:
                  // TODO: figure out if that range makes sense.
                  if (*(short *)addr > -2048 && *(short *)addr < 2048)
                    *(short *)addr *= 2;
                  break;
                case 4:
                  // TODO: figure out if that range makes sense.
                  t.i = *(u32 *)addr;
                  if (t.f > -2048 && t.f < 2048) {
                    t.f *= 2;
                    *(u32 *)addr = t.i;
                  }
                  break;
              }
            }
          }
        }

        AdvanceVerts(&index_addr, &vertex_addr, vertex_type, count, vertex_size);

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

void *(* _sceGeEdramGetAddr)(void);
int (* _sceGeGetList)(int qid, void *list, int *flag);
int (* _sceGeListUpdateStallAddr)(int qid, void *stall);
int (* _sceGeListEnQueue)(const void *list, void *stall, int cbid, PspGeListArgs *arg);
int (* _sceGeListEnQueueHead)(const void *list, void *stall, int cbid, PspGeListArgs *arg);
int (* _sceGeListSync)(int qid, int syncType);
int (* _sceGeDrawSync)(int syncType);

int (* _sceDisplaySetFrameBuf)(void *topaddr, int bufferwidth, int pixelformat, int sync);

void *sceGeEdramGetAddrPatched(void) {
  return (void *)FAKE_VRAM;
}

int sceGeListUpdateStallAddrPatched(int qid, void *stall) {
  int k1 = pspSdkSetK1(0);
  char info[64];
  if (_sceGeGetList(qid, info, NULL) == 0) {
    u16 state = *(u16 *)(info + 0x08);
    if (state != 3 && !finished) { // completed
      void *list = *(void **)(info + 0x18); // previous stall
      if (!list)
        list = *(void **)(info + 0x14); // list
      patchGeList((u32 *)((u32)list & 0x0fffffff), (u32 *)((u32)stall & 0x0fffffff));
      sceKernelDcacheWritebackInvalidateAll();
    }
  }
  pspSdkSetK1(k1);
  return _sceGeListUpdateStallAddr(qid, stall);
}

int sceGeListEnQueuePatched(const void *list, void *stall, int cbid, PspGeListArgs *arg) {
  int k1 = pspSdkSetK1(0);
  resetGeGlobals();
  patchGeList((u32 *)((u32)list & 0x0fffffff), (u32 *)((u32)stall & 0x0fffffff));
  sceKernelDcacheWritebackInvalidateAll();
  pspSdkSetK1(k1);
  return _sceGeListEnQueue(list, stall, cbid, arg);
}

int sceGeListEnQueueHeadPatched(const void *list, void *stall, int cbid, PspGeListArgs *arg) {
  int k1 = pspSdkSetK1(0);
  resetGeGlobals();
  patchGeList((u32 *)((u32)list & 0x0fffffff), (u32 *)((u32)stall & 0x0fffffff));
  sceKernelDcacheWritebackInvalidateAll();
  pspSdkSetK1(k1);
  return _sceGeListEnQueueHead(list, stall, cbid, arg);
}

int sceGeListSyncPatched(int qid, int syncType) {
  return _sceGeListSync(qid, syncType);
}

void copyFrameBuffer() {
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
}

int sceGeDrawSyncPatched(int syncType) {
  if (!framebuf_set) {
    // Framebuffer was not set previously (maybe it does never change)
    if (syncType == PSP_GE_LIST_DONE || syncType == PSP_GE_LIST_DRAWING_DONE) {
      copyFrameBuffer();
      rendered_in_sync = 1;
    }
  }

  framebuf_set = 0;

  return _sceGeDrawSync(syncType);
}

int sceDisplaySetFrameBufPatched(void *topaddr, int bufferwidth, int pixelformat, int sync) {
  if (!rendered_in_sync)
    copyFrameBuffer();
  rendered_in_sync = 0;
  framebuf_set = 1;
  return _sceDisplaySetFrameBuf(topaddr, bufferwidth, pixelformat, sync);
}

int module_start(SceSize args, void *argp) {
  _sceGeEdramGetAddr = (void *)FindProc("sceGE_Manager", "sceGe_driver", 0xE47E40E4);
  _sceGeGetList = (void *)FindProc("sceGE_Manager", "sceGe_driver", 0x67B01D8E);
  _sceGeListUpdateStallAddr = (void *)FindProc("sceGE_Manager", "sceGe_driver", 0xE0D68148);
  _sceGeListEnQueue = (void *)FindProc("sceGE_Manager", "sceGe_driver", 0xAB49E76A);
  _sceGeListEnQueueHead = (void *)FindProc("sceGE_Manager", "sceGe_driver", 0x1C0D95A6);
  _sceGeListSync = (void *)FindProc("sceGE_Manager", "sceGe_driver", 0x03444EB4);
  _sceGeDrawSync = (void *)FindProc("sceGE_Manager", "sceGe_driver", 0xB287BD61);

  sctrlHENPatchSyscall((u32)_sceGeEdramGetAddr, sceGeEdramGetAddrPatched);
  sctrlHENPatchSyscall((u32)_sceGeListUpdateStallAddr, sceGeListUpdateStallAddrPatched);
  sctrlHENPatchSyscall((u32)_sceGeListEnQueue, sceGeListEnQueuePatched);
  sctrlHENPatchSyscall((u32)_sceGeListEnQueueHead, sceGeListEnQueueHeadPatched);
  // sctrlHENPatchSyscall((u32)_sceGeListSync, sceGeListSyncPatched);
  sctrlHENPatchSyscall((u32)_sceGeDrawSync, sceGeDrawSyncPatched);

  _sceDisplaySetFrameBuf = (void *)FindProc("sceDisplay_Service", "sceDisplay_driver", 0x289D82FE);
  sctrlHENPatchSyscall((u32)_sceDisplaySetFrameBuf, sceDisplaySetFrameBufPatched);

  sceKernelDcacheWritebackInvalidateAll();
  sceKernelIcacheClearAll();

  return 0;
}
