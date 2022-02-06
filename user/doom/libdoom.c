#include <inc/libdoom.h>
#include <inc/lib.h>


int libc_abs(int x) {
    return (x >= 0) ? x : -x;
}

int libc_atoi(const char *s) {
    int rc = 0;

    char sign = '+';

    /* TODO: In other than "C" locale, additional patterns may be defined     */
    while ( *s == ' ' || *s == '\t' || *s == '\n' || *s == '\v' || *s == '\r' )
    {
        ++s;
    }

    if ( *s == '+' )
    {
        ++s;
    }
    else if ( *s == '-' )
    {
        sign = *( s++ );
    }

    /* TODO: Earlier version was missing tolower() but was not caught by tests */
    while ( *s >= '0' && *s <= '9' )
    {
        rc = rc * 10 + (*s - '0');
        s++;
    }

    return ( sign == '+' ) ? rc : -rc;
}


char heap[2 * 1024 * 1024]; // 2MB heap

void *HEAP_HEAD = heap; // current free position in heap

void libc_free(void *ptr) {
    return;
}

void *libc_malloc(int cnt) {
    void *ret = HEAP_HEAD;

    HEAP_HEAD += cnt;

    return ret;
}

void *libc_realloc(void *ptr, size_t newsize) {
    void *new_ptr = libc_malloc(newsize);
    if (!new_ptr) {
        return NULL;
    }

    // memcpy(new_ptr, ptr, ???);
    libc_free(ptr);
    
    return new_ptr;
}

void *libc_calloc(size_t num, size_t size) {
    return libc_malloc(num * size);
}


char *libc_strdup(const char *str) {
    size_t length = strlen(str);
    char* new_str = (char*)libc_malloc(length + 1);
    memcpy(new_str, str, length + 1);
    return new_str;
}

char *libc_strchr(const char *str, int ch) {
    do {
        if(*str == ch) return (char*)str;
    } while(*(++str) != '\0');

    return NULL;
}

char *libc_strrchr(const char *str, int ch) {
    const char *found, *ptr;

    if (ch == '\0') {
        return strchr(str, '\0');
    }

    found = NULL;
    while ((ptr = libc_strchr(str, ch)) != NULL) {
        found = ptr;
        str = ptr + 1;
    }

    return (char *) found;
}

char *libc_strstr(const char *str1, const char *str2) {
    const char *a, *b;

    b = str2;
    if (*b == 0) return (char*)str1;

    for ( ; *str1 != 0; str1++) {
        if (*str1 != *b) continue;

        a = str1;
        while (1) {
            if (*b == 0) return (char*)str1;
            if (*a++ != *b++) break;
        }
        b = str2;
    }
    return NULL;
}

int libc_putchar(char c) {
    if(write(1, &c, 1) == 1) return c;
    return -1;
}

int libc_puts(const char *s) {
    if(write(1, s, strlen(s)) > 0) return '\n';
    return -1;
}

int libc_printf(const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int result = vcprintf(format, ap);
    va_end(ap);
    return result;
}

int libc_fprintf(libc_FILE *fp, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
    int result = fprintf(*fp, format, ap);
#pragma GCC diagnostic pop
    va_end(ap);
    return result;
}

int libc_snprintf(char * s, size_t n, const char * format, ...) {
    va_list ap;
    va_start(ap, format);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
    int result = snprintf(s, n, format, ap);
#pragma GCC diagnostic pop
    va_end(ap);
    return result;
}

int libc_vfprintf(libc_FILE *stream, const char *format, va_list arg_ptr) {

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
    int result = vfprintf(*((int*)stream), format, arg_ptr);
#pragma GCC diagnostic pop

    return result;
}

int libc_vsnprintf(char * s, size_t n, const char *format, va_list arg) {

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
    int result = vsnprintf(s, n, format, arg);
#pragma GCC diagnostic pop

    return result;
}

void libc_exit() {
    exit();
}

libc_FILE *libc_fopen(const char *filename, const char *mode) {
    int r = -1;

    if(strcmp(mode, "r") == 0) r = open(filename, O_RDONLY);
    else if(strcmp(mode, "w") == 0) r = open(filename, O_WRONLY);
    else if(strcmp(mode, "rb") == 0) r = open(filename, O_RDONLY);
    else if(strcmp(mode, "wb") == 0) r = open(filename, O_WRONLY);

    if(r < 0) return NULL;

    libc_FILE* buffer = (libc_FILE*)libc_malloc(sizeof(libc_FILE));
    *((int*)buffer) = r;
    return buffer;
}

void libc_fclose(libc_FILE *file) {
    close(*((int*)file));
}

long libc_ftell(libc_FILE *file) {
    return tell(*((int*)file));
}

int libc_fseek (libc_FILE *file, long offset, int origin) {
    struct Stat stat;

    int fd = *((int*)file);
    fstat(fd, &stat);

    if(origin == SEEK_END) {
        if(origin > 0) return -1;
        if(origin < -stat.st_size) return -1;
        seek(fd, stat.st_size + origin);
    } else if(origin == SEEK_SET) {
        if(origin < 0) return -1;
        if(origin >= stat.st_size) return -1;
        seek(fd, origin);
    } else if(origin == SEEK_SET) {
        int cur = tell(fd);
        if(cur < 0) return -1;
        origin += cur;
        if(origin < 0) return -1;
        if(origin >= stat.st_size) return -1;
        seek(fd, origin);
    } else return -1;

    return 0;
}

int libc_fflush(libc_FILE *stream) {
    // We're working with file descriptors directly. fflush is unnecessary
    return 0;
}

size_t libc_fread(void *buf, size_t size, size_t count, libc_FILE *stream) {
    return read(*((int*)stream), buf, size * count);
}

size_t libc_fwrite(const void *buf, size_t size, size_t count, libc_FILE *stream) {
    return write(*((int*)stream), buf, size * count);
}

int libc_mkdir(const char *path, int chmod) {
    int fd = open(path, O_MKDIR);
    if(fd < 0) return -1;
    close(fd);
    return 0;
}

//int libc_remove(const char *fname);
//int libc_sscanf(const char *buf, const char *format, ...);
//int libc_rename(const char *oldfilename, const char *newfilename);
