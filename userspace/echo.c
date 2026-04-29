#include <userlib.h>

int main(void)
{
    puts("echo.elf ready, type q to exit\n");

    while (1) {
        char c = getc();

        if (c == 'q') {
            puts("\necho.elf exiting with status 7\n");
            return 7;
        }

        putc(c);
    }
}
