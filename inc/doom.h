#ifndef JOS_INC_DOOM_H
#define JOS_INC_DOOM_H 1


// #include <fcntl.h>
// #include <sys/types.h>

#ifndef FILE
typedef int FILE;
#endif

#ifndef stderr
extern FILE stderr;
#endif

#ifndef stdout
extern FILE stdout;
#endif


void free(void *ptr);

void *malloc(int cnt);
void *realloc(void *ptr, size_t newsize);
void *calloc(size_t num, size_t size);

int abs(int x);

FILE *fopen(const char *filename, const char *mode);
void fclose(FILE *file);

long ftell(FILE *file);

int rename(const char *oldfilename, const char *newfilename);

int atoi(const char *s);
float atof(const char *s);

int fflush(FILE stream);

int putchar(char c);
int puts(const char *s);

int system(const char *command);

int sscanf(const char *buf, const char *format, ...);

size_t fread(void *buf, size_t size, size_t count, FILE *stream);
size_t fwrite(const void *buf, size_t size, size_t count, FILE *stream);

int fseek (FILE *stream, long offset, int origin);


int mkdir(const char *path, int chmod);


#define SEEK_END 1
#define SEEK_SET 2


#endif // JOS_INC_DOOM_H