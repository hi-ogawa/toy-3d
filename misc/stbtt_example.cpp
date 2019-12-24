#include <cstdio>
#include <cstdint>

#include <stb_image.h>
#include <stb_image_write.h>
#include <stb_truetype.h>

uint8_t ttf_buffer[1<<25];

int main(int argc, char **argv) {
  stbtt_fontinfo font;
  unsigned char *bitmap;
  int w,h,i,j,c = (argc > 1 ? atoi(argv[1]) : 'g'), s = (argc > 2 ? atoi(argv[2]) : 20);

  fread(ttf_buffer, 1, 1<<25, fopen(argc > 3 ? argv[3] : "thirdparty/imgui/misc/fonts/ProggyClean.ttf", "rb"));

  stbtt_InitFont(&font, ttf_buffer, stbtt_GetFontOffsetForIndex(ttf_buffer,0));
  bitmap = stbtt_GetCodepointBitmap(&font, 0,stbtt_ScaleForPixelHeight(&font, s), c, &w, &h, 0,0);

  stbi_write_bmp("stbtt_example.bmp", w, h, 1, bitmap);
  stbi_write_png("stbtt_example.png", w, h, 1, bitmap, 0);

  for (j=0; j < h; ++j) {
    for (i=0; i < w; ++i) {
      putchar(" .:ioVM@"[bitmap[j*w+i]>>5]);
    }
    putchar('\n');
  }

  stbtt_FreeBitmap(bitmap, nullptr);
  return 0;
}
