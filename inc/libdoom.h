#ifndef JOS_INC_DOOM_H
#define JOS_INC_DOOM_H 1


#include <inc/types.h>
#include <inc/stdarg.h>


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
char *libc_strchr(const char *str, int ch);
char *libc_strrchr(const char *str, int ch);
char *libc_strstr(const char *str1, const char *str2);

int libc_putchar(char c);
int libc_puts(const char *s);

int libc_printf(const char *format, ...);
int libc_fprintf(libc_FILE *fp, const char *format, ...);

int libc_snprintf(char * s, size_t n, const char * format, ...);

void libc_exit();

// void *libc_init_heap();
void *libc_malloc(int cnt);
void *libc_realloc(void *ptr, size_t newsize);
void *libc_calloc(size_t num, size_t size);

void libc_free(void *ptr);

int libc_abs(int x);
int libc_atoi(const char *s);
int libc_toupper(int ch);

libc_FILE *libc_fopen(const char *filename, const char *mode);
void libc_fclose(libc_FILE *libc_FILE);

long libc_ftell(libc_FILE *libc_FILE);

int libc_rename(const char *oldfilename, const char *newfilename);
int libc_remove(const char *fname);
int libc_mkdir(const char *path, int chmod);

size_t libc_fread(void *buf, size_t size, size_t count, libc_FILE *stream);
size_t libc_fwrite(const void *buf, size_t size, size_t count, libc_FILE *stream);

int libc_fflush(libc_FILE *stream);

int libc_vfprintf(libc_FILE *stream, const char *format, va_list arg_ptr);
int libc_vsnprintf(char * s, size_t n, const char *format, va_list arg);

int libc_sscanf(const char *buf, const char *format, ...);

int libc_fseek (libc_FILE *stream, long offset, int origin);


#define SEEK_END 1
#define SEEK_SET 2


#ifdef LIBC_REMOVE_FPREFIX
#define strdup(...) libc_strdup( __VA_ARGS__ )
#define strchr(...) libc_strchr( __VA_ARGS__ )
#define strrchr(...) slibc_strrchr( __VA_ARGS__ )
#define strstr(...) libc_strstr( __VA_ARGS__ )

#define putchar(...) libc_putchar( __VA_ARGS__ )
#define puts(...) libc_puts( __VA_ARGS__ )

#define printf(...) libc_printf( __VA_ARGS__ )
#define fprintf(...) libc_fprintf( __VA_ARGS__ )

#define snprintf(...) libc_snprintf( __VA_ARGS__ )

#define exit(...) libc_exit( __VA_ARGS__ )

#define malloc(...) libc_malloc( __VA_ARGS__ )
#define realloc(...) libc_realloc( __VA_ARGS__ )
#define calloc(...) libc_calloc( __VA_ARGS__ )

#define free(...) libc_free( __VA_ARGS__ )

#define abs(...) libc_abs( __VA_ARGS__ )
#define atoi(...) libc_atoi( __VA_ARGS__ )
#define toupper(...) libc_toupper( __VA_ARGS__ )

#define fopen(...) libc_fopen( __VA_ARGS__ )
#define  fclose(...) libc_fclose( __VA_ARGS__ )

#define  ftell(...) libc_ftell( __VA_ARGS__ )

#define  rename(...) libc_rename( __VA_ARGS__ )
#define  remove(...) libc_remove( __VA_ARGS__ )
#define  mkdir(...) libc_mkdir( __VA_ARGS__ )

#define  fread(...) libc_fread( __VA_ARGS__ )
#define  fwrite(...) libc_fwrite( __VA_ARGS__ )

#define  fflush(...) libc_fflush( __VA_ARGS__ )

#define  vfprintf(...) libc_vfprintf( __VA_ARGS__ )
#define  vsnprintf(...) libc_vsnprintf( __VA_ARGS__ )

#define  sscanf(...) libc_sscanf( __VA_ARGS__ )

#define  fseek(...) libc_fseek( __VA_ARGS__ )
#endif


#endif // JOS_INC_DOOM_H