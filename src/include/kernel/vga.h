#ifndef __x86_64_VGA_H__
#define __x86_64_VGA_H__

#include <stdint-gcc.h>


#define VGA_BLACK           0x00
#define VGA_BLUE            0x01
#define VGA_GREEN           0x02
#define VGA_CYAN            0x03
#define VGA_RED             0x04
#define VGA_PURPLE          0x05
#define VGA_ORANGE          0x06
#define VGA_LIGHT_GREY      0x07
#define VGA_DARK_GREY       0x08
#define VGA_BRIGHT_BLUE     0x09
#define VGA_BRIGHT_GREEN    0x0A
#define VGA_BRIGHT_CYAN     0x0B
#define VGA_MAGENTA         0x0C
#define VGA_BRIGHT_PURPLE   0x0D
#define VGA_YELLOW          0x0E
#define VGA_WHITE           0x0F

#define BG(color) ((color & 0x0F) << 4)
#define FG(color) ((color & 0x0F) << 0)

void vga_setcolor(uint8_t fg, uint8_t bg);
void vga_display_char(char c);
void vga_display_str(const char *s);
void vga_clear();

#endif