#ifndef TEST_PSPGU_H
#define TEST_PSPGU_H
int sceGuStart(int context, void *list);
void sceGuCopyImage(int format, int sx, int sy, int width, int height,
                   int source_width, void *source, int dx, int dy,
                   int destination_width, void *destination);
int sceGuFinish(void);
#endif
