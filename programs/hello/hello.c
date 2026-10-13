/* ProotOS user program — работает в ring 3, общается через int 0x80. */

static int syscall_write(const char* buf, unsigned int len) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(1), "b"(1), "c"(buf), "d"(len)
        : "memory"
    );
    return ret;
}

static void syscall_exit(int code) {
    __asm__ volatile (
        "int $0x80"
        : : "a"(0), "b"(code)
        : "memory"
    );
    for (;;) { }
}

static unsigned int my_strlen(const char* s) {
    unsigned int n = 0;
    while (s[n]) n++;
    return n;
}

void _start(void) {
    const char* m1 = "Hello from ProotOS ring 3!\n";
    const char* m2 = "I can only talk via syscalls now.\n";
    const char* m3 = "Bye!\n";

    syscall_write(m1, my_strlen(m1));
    syscall_write(m2, my_strlen(m2));
    syscall_write(m3, my_strlen(m3));

    syscall_exit(0);
}
