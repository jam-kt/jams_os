#include <stdint-gcc.h>

#include <userlib.h>

#define THREAD_COUNT 3
#define THREAD_STACK_QWORDS 512
#define THREAD_ITERS 100000
#define RACE_YIELD_EVERY 8

struct thread_arg {
    int index;
    int status;
};

static uint64_t thread_stacks[THREAD_COUNT][THREAD_STACK_QWORDS];
static struct thread_arg thread_args[THREAD_COUNT];
static volatile uint64_t shared_counter = 0;
static volatile uint64_t locked_counter = 0;
static mutex_t counter_mutex;

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

static void counter_thread(void *arg)
{
    struct thread_arg *targ = (struct thread_arg *)arg;

    puts("thread ");
    putnum(gettid());
    puts(" in pid ");
    putnum(getpid());
    puts(" starting\n");

    for (int i = 0; i < THREAD_ITERS; i++) {
        uint64_t snapshot = shared_counter;
        uint64_t locked_snapshot = shared_counter;

        if ((i % RACE_YIELD_EVERY) == 0) {
            yield();
        }
        
        shared_counter = snapshot + 1;

        if (mutex_lock(&counter_mutex) < 0) {
            thread_exit(90);
        }

        locked_snapshot = locked_counter;
        locked_counter = locked_snapshot + 1;

        if (mutex_unlock(&counter_mutex) < 0) {
            thread_exit(91);
        }
    }

    thread_exit(targ->status);
}

static void test_threads(void)
{
    uint64_t tids[THREAD_COUNT];
    uint64_t joined;
    int status;

    puts("\ninit: pid ");
    putnum(getpid());
    puts(" tid ");
    putnum(gettid());
    putc('\n');

    shared_counter = 0;
    locked_counter = 0;
    if (mutex_init(&counter_mutex) < 0) {
        puts("init: mutex_init failed\n");
        return;
    }

    for (int i = 0; i < THREAD_COUNT; i++) {
        thread_args[i].index = i;
        thread_args[i].status = 20 + i;
        tids[i] = thread_create(thread_stacks[i],
                                sizeof(thread_stacks[i]),
                                counter_thread,
                                &thread_args[i]);
        puts("init: clone thread returned ");
        putnum(tids[i]);
        putc('\n');
    }

    for (int i = 0; i < THREAD_COUNT; i++) {
        status = -1;
        joined = thread_join(tids[i], &status);
        puts("init: joined tid ");
        putnum(joined);
        puts(" status ");
        putnum((uint64_t)status);
        putc('\n');
    }

    puts("init: shared counter final value: ");
    putnum(shared_counter);
    puts(" with an expected value of ");
    putnum(THREAD_COUNT * THREAD_ITERS);
    puts("\n\n");

    puts("init: locked counter final value: ");
    putnum(locked_counter);
    puts(" with an expected value of ");
    putnum(THREAD_COUNT * THREAD_ITERS);
    puts("\n\n");

    if (mutex_destroy(&counter_mutex) < 0) {
        puts("init: mutex_destroy failed\n");
    }
}

int main(void) 
{
    puts("init.elf ready\n");
    puts("press 1 to test fork, exec, and wait\n");
    puts("press 2 to test thread creation and management\n\n");

    while (1) {
        char c = getc();

        if (c == '1') {
            test_fork_exec_wait();
            continue;
        } else if (c == '2') {
            test_threads();
            continue;
        }

        putc(c);
    }

    return 0;
}
