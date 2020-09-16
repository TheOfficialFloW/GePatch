/*
 * PSP Software Development Kit - http://www.pspdev.org
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPSDK root for details.
 *
 * Copyright (c) 2005 Jesper Svennevid
 */

#include <pspgu.h>

static int tbpcmd_tbl[8] = { 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7 };    // 0x30A18
static int tbwcmd_tbl[8] = { 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf };    // 0x30A38
static int tsizecmd_tbl[8] = { 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf };  // 0x30A58

unsigned int *current = NULL;

void sendCommandi(int cmd, int argument)
{
  *(current++) = (cmd << 24) | (argument & 0xffffff);
}

int getExp(int val)
{
  unsigned int i;
  asm("clz %0, %1\n":"=r"(i):"r"(val&0x3FF));
  return 31-i;
}

void sceGuTexFlush(void)
{
  sendCommandi(203,0);
}

void sceGuTexSync()
{
  sendCommandi(204,0);
}

void sceGuTexImage(int mipmap, int width, int height, int tbw, const void* tbp)
{
  sendCommandi(tbpcmd_tbl[mipmap],((unsigned int)tbp) & 0xffffff);
  sendCommandi(tbwcmd_tbl[mipmap],((((unsigned int)tbp) >> 8) & 0x0f0000)|tbw);
  sendCommandi(tsizecmd_tbl[mipmap],(getExp(height) << 8)|(getExp(width)));
  sceGuTexFlush();
}

void sceGuCopyImage(int psm, int sx, int sy, int width, int height, int srcw, void* src, int dx, int dy, int destw, void* dest)
{
  sendCommandi(178,((unsigned int)src) & 0xffffff);
  sendCommandi(179,((((unsigned int)src) & 0xff000000) >> 8)|srcw);
  sendCommandi(235,(sy << 10)|sx);
  sendCommandi(180,((unsigned int)dest) & 0xffffff);
  sendCommandi(181,((((unsigned int)dest) & 0xff000000) >> 8)|destw);
  sendCommandi(236,(dy << 10)|dx);
  sendCommandi(238,((height-1) << 10)|(width-1));
  sendCommandi(234,(psm ^ 0x03) ? 0 : 1);
}

void sceGuDrawBuffer(int psm, void* fbp, int frame_width)
{
  sendCommandi(210,psm);
  sendCommandi(156,((unsigned int)fbp) & 0xffffff);
  sendCommandi(157,((((unsigned int)fbp) & 0xff000000) >> 8)|frame_width);
}

void sceGuEnable(int state)
{
  switch(state)
  {
    case GU_ALPHA_TEST:           sendCommandi(34,1); break;
    case GU_DEPTH_TEST:           sendCommandi(35,1); break;
    case GU_STENCIL_TEST:         sendCommandi(36,1); break;
    case GU_BLEND:                sendCommandi(33,1); break;
    case GU_CULL_FACE:            sendCommandi(29,1); break;
    case GU_DITHER:               sendCommandi(32,1); break;
    case GU_FOG:                  sendCommandi(31,1); break;
    case GU_CLIP_PLANES:          sendCommandi(28,1); break;
    case GU_TEXTURE_2D:           sendCommandi(30,1); break;
    case GU_LIGHTING:             sendCommandi(23,1); break;
    case GU_LIGHT0:               sendCommandi(24,1); break;
    case GU_LIGHT1:               sendCommandi(25,1); break;
    case GU_LIGHT2:               sendCommandi(26,1); break;
    case GU_LIGHT3:               sendCommandi(27,1); break;
    case GU_LINE_SMOOTH:          sendCommandi(37,1); break;
    case GU_PATCH_CULL_FACE:      sendCommandi(38,1); break;
    case GU_COLOR_TEST:           sendCommandi(39,1); break;
    case GU_COLOR_LOGIC_OP:       sendCommandi(40,1); break;
    case GU_FACE_NORMAL_REVERSE:  sendCommandi(81,1); break;
    case GU_PATCH_FACE:           sendCommandi(56,1); break;
    break;
  }
}

void sceGuDisable(int state)
{
  switch(state)
  {
    case GU_ALPHA_TEST:           sendCommandi(34,0); break;
    case GU_DEPTH_TEST:           sendCommandi(35,0); break;
    case GU_STENCIL_TEST:         sendCommandi(36,0); break;
    case GU_BLEND:                sendCommandi(33,0); break;
    case GU_CULL_FACE:            sendCommandi(29,0); break;
    case GU_DITHER:               sendCommandi(32,0); break;
    case GU_FOG:                  sendCommandi(31,0); break;
    case GU_CLIP_PLANES:          sendCommandi(28,0); break;
    case GU_TEXTURE_2D:           sendCommandi(30,0); break;
    case GU_LIGHTING:             sendCommandi(23,0); break;
    case GU_LIGHT0:               sendCommandi(24,0); break;
    case GU_LIGHT1:               sendCommandi(25,0); break;
    case GU_LIGHT2:               sendCommandi(26,0); break;
    case GU_LIGHT3:               sendCommandi(27,0); break;
    case GU_LINE_SMOOTH:          sendCommandi(37,0); break;
    case GU_PATCH_CULL_FACE:      sendCommandi(38,0); break;
    case GU_COLOR_TEST:           sendCommandi(39,0); break;
    case GU_COLOR_LOGIC_OP:       sendCommandi(40,0); break;
    case GU_FACE_NORMAL_REVERSE:  sendCommandi(81,0); break;
    case GU_PATCH_FACE:           sendCommandi(56,0); break;
    break;
  }
}

int sceGuFinish(void)
{
  sendCommandi(15,0);
  sendCommandi(12,0);
  return 0;
}

void sceGuStart(int cid, void* list)
{
  current = (unsigned int*)((unsigned int)list);
}
