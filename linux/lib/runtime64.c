#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <time.h>

// I/O y consola para x86_64
void fundamental_printf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

void fundamental_puts(const char* s) {
    puts(s ? s : "(null)");
}

void fundamental_putchar(int c) {
    putchar(c);
}

int fundamental_getchar(void) {
    return getchar();
}

void fundamental_print_str(const char* s) {
    if (s) fputs(s, stdout);
    fflush(stdout);
}

void fundamental_println_str(const char* s) {
    if (s) puts(s);
    else puts("(null)");
    fflush(stdout);
}

void fundamental_println_empty(void) {
    putchar('\n');
    fflush(stdout);
}

void fundamental_print_int(long long n) {
    printf("%lld", n);
    fflush(stdout);
}

void fundamental_println_int(long long n) {
    printf("%lld\n", n);
    fflush(stdout);
}

void fundamental_print_float(double d) {
    printf("%f", d);
    fflush(stdout);
}

void fundamental_println_float(double d) {
    printf("%f\n", d);
    fflush(stdout);
}

void fundamental_read_str(char* buf, int max_len) {
    if (buf && max_len > 0) {
        if (fgets(buf, max_len, stdin)) {
            size_t l = strlen(buf);
            if (l > 0 && (buf[l-1] == '\n' || buf[l-1] == '\r')) buf[l-1] = '\0';
        }
    }
}

long long fundamental_read_int(void) {
    long long v = 0;
    if (scanf("%lld", &v) != 1) return 0;
    return v;
}

// Memoria dinamica
void* fundamental_malloc(size_t sz) {
    return malloc(sz);
}

void fundamental_free(void* ptr) {
    free(ptr);
}

void* fundamental_calloc(size_t num, size_t sz) {
    return calloc(num, sz);
}

void* fundamental_realloc(void* ptr, size_t sz) {
    return realloc(ptr, sz);
}

void* fundamental_memset(void* s, int c, size_t n) {
    return memset(s, c, n);
}

void* fundamental_memcpy(void* dest, const void* src, size_t n) {
    return memcpy(dest, src, n);
}

void* fundamental_memmove(void* dest, const void* src, size_t n) {
    return memmove(dest, src, n);
}

int fundamental_memcmp(const void* s1, const void* s2, size_t n) {
    return memcmp(s1, s2, n);
}

// Manipulacion de cadenas
size_t fundamental_strlen(const char* s) {
    return s ? strlen(s) : 0;
}

char* fundamental_strcpy(char* dest, const char* src) {
    return strcpy(dest, src);
}

char* fundamental_strncpy(char* dest, const char* src, size_t n) {
    return strncpy(dest, src, n);
}

int fundamental_strcmp(const char* s1, const char* s2) {
    return strcmp(s1, s2);
}

char* fundamental_strcat(char* dest, const char* src) {
    return strcat(dest, src);
}

char* fundamental_strchr(const char* s, int c) {
    return strchr(s, c);
}

char* fundamental_strstr(const char* haystack, const char* needle) {
    return strstr(haystack, needle);
}

int fundamental_atoi(const char* s) {
    return s ? atoi(s) : 0;
}

char* fundamental_itoa(int val, char* buf, int base) {
    if (!buf) return NULL;
    if (base == 10) sprintf(buf, "%d", val);
    else if (base == 16) sprintf(buf, "%x", val);
    else if (base == 8) sprintf(buf, "%o", val);
    else sprintf(buf, "%d", val);
    return buf;
}

// Matematicas y utilidades
int fundamental_abs(int x) {
    return abs(x);
}

int fundamental_max(int a, int b) {
    return a > b ? a : b;
}

int fundamental_min(int a, int b) {
    return a < b ? a : b;
}

int fundamental_rand(void) {
    return rand();
}

void fundamental_srand(unsigned int seed) {
    srand(seed);
}

void fundamental_exit(int code) {
    exit(code);
}

int fundamental_system(const char* cmd) {
    return system(cmd);
}

clock_t fundamental_clock(void) {
    return clock();
}

time_t fundamental_time(time_t* t) {
    return time(t);
}

double fundamental_sqrt(double x) {
    return sqrt(x);
}

double fundamental_pow(double b, double e) {
    return pow(b, e);
}

double fundamental_sin(double x) {
    return sin(x);
}

double fundamental_cos(double x) {
    return cos(x);
}

double fundamental_tan(double x) {
    return tan(x);
}

// Operaciones de archivos
FILE* fundamental_file_open(const char* path, const char* mode) {
    return fopen(path, mode);
}

int fundamental_file_close(FILE* f) {
    return f ? fclose(f) : -1;
}

size_t fundamental_file_read(void* ptr, size_t sz, size_t count, FILE* f) {
    return f ? fread(ptr, sz, count, f) : 0;
}

size_t fundamental_file_write(const void* ptr, size_t sz, size_t count, FILE* f) {
    return f ? fwrite(ptr, sz, count, f) : 0;
}

int fundamental_file_remove(const char* path) {
    return remove(path);
}

// ========================================================
// Manejo de Saltos No Locales (setjmp / longjmp) para Itsuki
// ========================================================
#if defined(__x86_64__) || defined(_M_X64)
__asm__(
".globl itsuki_setjmp\n"
".globl _itsuki_setjmp\n"
".globl setjmp\n"
".globl _setjmp\n"
"itsuki_setjmp:\n"
"_itsuki_setjmp:\n"
"setjmp:\n"
"_setjmp:\n"
"    movq 8(%rsp), %rdx\n"
"    movq %rbx, 0(%rdx)\n"
"    movq %rbp, 8(%rdx)\n"
"    movq %r12, 16(%rdx)\n"
"    movq %r13, 24(%rdx)\n"
"    movq %r14, 32(%rdx)\n"
"    movq %r15, 40(%rdx)\n"
"    leaq 8(%rsp), %rcx\n"
"    movq %rcx, 48(%rdx)\n"
"    movq 0(%rsp), %rcx\n"
"    movq %rcx, 56(%rdx)\n"
"    xorq %rax, %rax\n"
"    ret\n"
"\n"
".globl itsuki_longjmp\n"
".globl _itsuki_longjmp\n"
".globl longjmp\n"
".globl _longjmp\n"
"itsuki_longjmp:\n"
"_itsuki_longjmp:\n"
"longjmp:\n"
"_longjmp:\n"
"    movq 8(%rsp), %rdx\n"
"    movq 16(%rsp), %rax\n"
"    testq %rax, %rax\n"
"    jnz 1f\n"
"    movq $1, %rax\n"
"1:\n"
"    movq 0(%rdx), %rbx\n"
"    movq 8(%rdx), %rbp\n"
"    movq 16(%rdx), %r12\n"
"    movq 24(%rdx), %r13\n"
"    movq 32(%rdx), %r14\n"
"    movq 40(%rdx), %r15\n"
"    movq 48(%rdx), %rsp\n"
"    movq 56(%rdx), %rcx\n"
"    jmp *%rcx\n"
);
#else
__asm__(
".globl itsuki_setjmp\n"
".globl _itsuki_setjmp\n"
".globl setjmp\n"
".globl _setjmp\n"
"itsuki_setjmp:\n"
"_itsuki_setjmp:\n"
"setjmp:\n"
"_setjmp:\n"
"    movl 4(%esp), %edx\n"
"    movl %ebx, 0(%edx)\n"
"    movl %esi, 4(%edx)\n"
"    movl %edi, 8(%edx)\n"
"    movl %ebp, 12(%edx)\n"
"    leal 4(%esp), %ecx\n"
"    movl %ecx, 16(%edx)\n"
"    movl 0(%esp), %ecx\n"
"    movl %ecx, 20(%edx)\n"
"    xorl %eax, %eax\n"
"    ret\n"
"\n"
".globl itsuki_longjmp\n"
".globl _itsuki_longjmp\n"
".globl longjmp\n"
".globl _longjmp\n"
"itsuki_longjmp:\n"
"_itsuki_longjmp:\n"
"longjmp:\n"
"_longjmp:\n"
"    movl 4(%esp), %edx\n"
"    movl 8(%esp), %eax\n"
"    testl %eax, %eax\n"
"    jnz 1f\n"
"    movl $1, %eax\n"
"1:\n"
"    movl 0(%edx), %ebx\n"
"    movl 4(%edx), %esi\n"
"    movl 8(%edx), %edi\n"
"    movl 12(%edx), %ebp\n"
"    movl 16(%edx), %esp\n"
"    movl 20(%edx), %ecx\n"
"    jmp *%ecx\n"
);
#endif

