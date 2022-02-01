#ifndef JOS_INC_DOOM_H
#define JOS_INC_DOOM_H 1


#include <inc/types.h>
#include <inc/stdarg.h>

// #include <fcntl.h>
// #include <sys/types.h>


#ifndef libc_FILE
typedef int libc_FILE;
#endif

#ifndef libc_stderr
extern libc_FILE *libc_stderr;
#endif

#ifndef libc_stdout
extern libc_FILE *libc_stdout;
#endif

char *libc_strdup(const char *str);
char *libc_strrchr(const char *str, int ch);
char *libc_strstr(const char *str1, const char *str2);


int libc_printf(const char *format, ...);
int libc_snprintf(char * s, size_t n, const char * format, ...);

int libc_fprintf(libc_FILE *fp, const char *format, ...);

void libc_exit();

int libc_remove(const char *fname);

void libc_free(void *ptr);

void *libc_malloc(int cnt);
void *libc_realloc(void *ptr, size_t newsize);
void *libc_calloc(size_t num, size_t size);

int libc_abs(int x);

libc_FILE *libc_fopen(const char *filename, const char *mode);
void libc_fclose(libc_FILE *libc_FILE);

long libc_ftell(libc_FILE *libc_FILE);

int libc_rename(const char *oldfilename, const char *newfilename);

int libc_atoi(const char *s);

int libc_fflush(libc_FILE *stream);

int libc_vfprintf(libc_FILE *stream, const char *format, va_list arg_ptr);
int libc_vsnprintf(char * s, size_t n, const char *format, va_list arg);

int libc_putchar(char c);
int libc_puts(const char *s);

int libc_sscanf(const char *buf, const char *format, ...);

size_t libc_fread(void *buf, size_t size, size_t count, libc_FILE *stream);
size_t libc_fwrite(const void *buf, size_t size, size_t count, libc_FILE *stream);

int libc_fseek (libc_FILE *stream, long offset, int origin);

int toupper(int ch);

int libc_mkdir(const char *path, int chmod);


#define SEEK_END 1
#define SEEK_SET 2


#endif // JOS_INC_DOOM_H