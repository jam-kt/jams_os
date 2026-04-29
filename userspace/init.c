#include <stdint-gcc.h>

#include <userlib.h>

static void test_fork_exec_wait(void)
{
    int status = -1;
    uint64_t pid;
    uint64_t waited;

    puts("\ninit: forking child\n");
    pid = fork();

    if (pid == 0) {
        puts("child: exec echo.elf\n");
        if (exec("echo.elf") == (uint64_t)-1) {
            puts("child: exec failed\n");
            exit(1);
        }
    }

    puts("init: waiting for child ");
    putnum(pid);
    putc('\n');

    waited = wait(&status);
    puts("init: wait returned pid ");
    putnum(waited);
    puts(" status ");
    putnum((uint64_t)status);
    putc('\n');
}

int main(void) 
{
    puts("init.elf ready\n");
    puts("1: fork + exec echo.elf + wait\n");
    puts("echo.elf exits when you type q\n\n");
    
    while (1) {
        char c = getc();

        if (c == '1') {
            test_fork_exec_wait();
        } else {
            putc(c);
        }
    }
    
    return 0;
}
