#ifndef __SERIAL_OUT_H__
#define __SERIAL_OUT_H__

int serial_init();
void serial_display_char(char c);
void serial_display_string(const char *s);

#endif