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


// ================================================================================================ mem

/* block header */
struct header {
    /* next block */
    struct header *next;
    /* prev block */
    struct header *prev;
    /* size of this block */
    size_t size;
} __attribute__((packed, aligned(_Alignof(max_align_t))));

typedef struct header Header;


#define SPACE_SIZE 5 * 0x1000

static uint8_t space[SPACE_SIZE];

/* empty list to get started */
static Header base = {.next = (Header *)space, .prev = (Header *)space};
/* start of free list */
static Header *freep = NULL;

static void
check_list(void) {
    asm volatile("cli");
    Header *prevp = freep, *p = prevp->next;
    for (; p != freep; p = p->next) {
        if (prevp != p->prev) panic("Corrupted list.\n");
        prevp = p;
    }
    asm volatile("sti");
}

/* malloc: general-purpose storage allocator */
void *
test_alloc(size_t nbytes) {

    /* Make allocator thread-safe with the help of spin_lock/spin_unlock. */
    // LAB 5: Your code here DONE?
    // lock_kernel();

    size_t nunits = (nbytes + sizeof(Header) - 1) / sizeof(Header) + 1;

    /* no free list yet */
    if (!freep) {
        Header *hd = (Header *)&space;

        hd->next = (Header *)&base;
        hd->prev = (Header *)&base;
        hd->size = (SPACE_SIZE - sizeof(Header)) / sizeof(Header);

        freep = &base;
    }

    // cprintf("frep %p\n", freep);

    // check_list();

    for (Header *p = freep->next;; p = p->next) {
        /* big enough */
        if (p->size >= nunits) {
            freep = p->prev;
            /* exactly */
            if (p->size == nunits) {
                p->prev->next = p->next;
                p->next->prev = p->prev;
            } else { /* allocate tail end */
                p->size -= nunits;
                p += p->size;
                p->size = nunits;
            }
            // unlock_kernel();
            return (void *)(p + 1);
        }

        /* wrapped around free list */
        if (p == freep) {
            break;
        }
    }

    // unlock_kernel();
    return NULL;
}

/* free: put block ap in free list */
void
test_free(void *ap) {

    /* point to block header */
    Header *bp = (Header *)ap - 1;

    /* Make allocator thread-safe with the help of spin_lock/spin_unlock. */
    // LAB 5: Your code here DONE?
    // lock_kernel();

    /* freed block at start or end of arena */
    Header *p = freep;
    for (; !(bp > p && bp < p->next); p = p->next)
        if (p >= p->next && (bp > p || bp < p->next)) break;

    if (bp + bp->size == p->next && p + p->size == bp) /* join to both */ {
        p->size += bp->size + p->next->size;
        p->next->next->prev = p;
        p->next = p->next->next;
    } else if (bp + bp->size == p->next) /* join to upper nbr */ {
        bp->size += p->next->size;
        bp->next = p->next->next;
        bp->prev = p->next->prev;
        p->next->next->prev = bp;
        p->next = bp;
    } else if (p + p->size == bp) /* join to lower nbr */ {
        p->size += bp->size;
    } else {
        bp->next = p->next;
        bp->prev = p;
        p->next->prev = bp;
        p->next = bp;
    }
    freep = p;

    // check_list();

    // unlock_kernel();

}


Header*
get_header(void *ap) {

    /* point to block header */
    Header *bp = (Header *)ap - 1;

    /* Make allocator thread-safe with the help of spin_lock/spin_unlock. */
    // LAB 5: Your code here DONE?
    // lock_kernel();

    /* freed block at start or end of arena */
    Header *p = freep;
    for (; !(bp > p && bp < p->next); p = p->next)
        if (p >= p->next && (bp > p || bp < p->next)) break;

    return bp;
}

// ================================================================================================

void libc_free(void *ptr) {
    test_free(ptr);
}

void *libc_malloc(int cnt) {
    return test_alloc((size_t) cnt);
}

void *libc_realloc(void *ptr, size_t newsize) {
    void *new_ptr = libc_malloc(newsize);
    if (!new_ptr) {
        return NULL;
    }

    Header *header = get_header(ptr);
    
    memcpy(new_ptr, ptr, header->size);
    libc_free(ptr);
    
    return new_ptr;
}

void *libc_calloc(size_t num, size_t size) {
    void *ptr = libc_malloc(num * size);
    memset(ptr, 0, num * size);
    
    return ptr;
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

// #include <sys/cdefs.h>

#if 0 /* XXX coming soon */
#include <ctype.h>
#else
#endif
// #include <stdarg.h>
// #include <string.h>
// #include <sys/param.h>
// #include <sys/systm.h>

#define	BUF		32 	/* Maximum length of numeric string. */

/*
 * Flags used during conversion.
 */
#define	LONG		0x01	/* l: long or double */
#define	SHORT		0x04	/* h: short */
#define	SUPPRESS	0x08	/* *: suppress assignment */
#define	POINTER		0x10	/* p: void * (as hex) */
#define	NOSKIP		0x20	/* [ or c: do not skip blanks */
#define	LONGLONG	0x400	/* ll: long long (+ deprecated q: quad) */
#define	SHORTSHORT	0x4000	/* hh: char */
#define	UNSIGNED	0x8000	/* %[oupxX] conversions */

/*
 * The following are used in numeric conversions only:
 * SIGNOK, NDIGITS, DPTOK, and EXPOK are for floating point;
 * SIGNOK, NDIGITS, PFXOK, and NZDIGITS are for integral.
 */
#define	SIGNOK		0x40	/* +/- is (still) legal */
#define	NDIGITS		0x80	/* no digits detected */

#define	DPTOK		0x100	/* (float) decimal point is still legal */
#define	EXPOK		0x200	/* (float) exponent (e+3, etc) still legal */

#define	PFXOK		0x100	/* 0x prefix is (still) legal */
#define	NZDIGITS	0x200	/* no zero digits detected */

/*
 * Conversion types.
 */
#define	CT_CHAR		0	/* %c conversion */
#define	CT_CCL		1	/* %[...] conversion */
#define	CT_STRING	2	/* %s conversion */
#define	CT_INT		3	/* %[dioupxX] conversion */

static const uint8_t *__libc_sccl(char *, const uint8_t *);
int libc_vsscanf(const char *inp, char const *fmt0, va_list ap);

int
libc_sscanf(const char *ibuf, const char *fmt, ...)
{
	va_list ap;
	int ret;
	
	va_start(ap, fmt);
	ret = libc_vsscanf(ibuf, fmt, ap);
	va_end(ap);
	return(ret);
}

int
libc_vsscanf(const char *inp, char const *fmt0, va_list ap)
{
	int inr;
	const uint8_t *fmt = (const uint8_t *)fmt0;
	int c;			/* character from format, or conversion */
	size_t width;		/* field width, or 0 */
	char *p;		/* points into all kinds of strings */
	int n;			/* handy integer */
	int flags;		/* flags as defined above */
	char *p0;		/* saves original value of p when necessary */
	int nassigned;		/* number of fields assigned */
	int nconversions;	/* number of conversions */
	int nread;		/* number of characters consumed from fp */
	int base;		/* base argument to conversion function */
	char ccltab[256];	/* character class table for %[...] */
	char buf[BUF];		/* buffer for numeric conversions */

	/* `basefix' is used to avoid `if' tests in the integer scanner */
	static short basefix[17] =
		{ 10, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };

	inr = strlen(inp);
	
	nassigned = 0;
	nconversions = 0;
	nread = 0;
	base = 0;		/* XXX just to keep gcc happy */
	for (;;) {
		c = *fmt++;
		if (c == 0)
			return (nassigned);
		if (isspace(c)) {
			while (inr > 0 && isspace(*inp))
				nread++, inr--, inp++;
			continue;
		}
		if (c != '%')
			goto literal;
		width = 0;
		flags = 0;
		/*
		 * switch on the format.  continue if done;
		 * break once format type is derived.
		 */
again:		c = *fmt++;
		switch (c) {
		case '%':
literal:
			if (inr <= 0)
				goto input_failure;
			if (*inp != c)
				goto match_failure;
			inr--, inp++;
			nread++;
			continue;

		case '*':
			flags |= SUPPRESS;
			goto again;
		case 'l':
			if (flags & LONG) {
				flags &= ~LONG;
				flags |= LONGLONG;
			} else
				flags |= LONG;
			goto again;
		case 'q':
			flags |= LONGLONG;	/* not quite */
			goto again;
		case 'h':
			if (flags & SHORT) {
				flags &= ~SHORT;
				flags |= SHORTSHORT;
			} else
				flags |= SHORT;
			goto again;

		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			width = width * 10 + c - '0';
			goto again;

		/*
		 * Conversions.
		 */
		case 'd':
			c = CT_INT;
			base = 10;
			break;

		case 'i':
			c = CT_INT;
			base = 0;
			break;

		case 'o':
			c = CT_INT;
			flags |= UNSIGNED;
			base = 8;
			break;

		case 'u':
			c = CT_INT;
			flags |= UNSIGNED;
			base = 10;
			break;

		case 'X':
		case 'x':
			flags |= PFXOK;	/* enable 0x prefixing */
			c = CT_INT;
			flags |= UNSIGNED;
			base = 16;
			break;

		case 's':
			c = CT_STRING;
			break;

		case '[':
			fmt = __libc_sccl(ccltab, fmt);
			flags |= NOSKIP;
			c = CT_CCL;
			break;

		case 'c':
			flags |= NOSKIP;
			c = CT_CHAR;
			break;

		case 'p':	/* pointer format is like hex */
			flags |= POINTER | PFXOK;
			c = CT_INT;
			flags |= UNSIGNED;
			base = 16;
			break;

		case 'n':
			nconversions++;
			if (flags & SUPPRESS)	/* ??? */
				continue;
			if (flags & SHORTSHORT)
				*va_arg(ap, char *) = nread;
			else if (flags & SHORT)
				*va_arg(ap, short *) = nread;
			else if (flags & LONG)
				*va_arg(ap, long *) = nread;
			else if (flags & LONGLONG)
				*va_arg(ap, long long *) = nread;
			else
				*va_arg(ap, int *) = nread;
			continue;
		}

		/*
		 * We have a conversion that requires input.
		 */
		if (inr <= 0)
			goto input_failure;

		/*
		 * Consume leading white space, except for formats
		 * that suppress this.
		 */
		if ((flags & NOSKIP) == 0) {
			while (isspace(*inp)) {
				nread++;
				if (--inr > 0)
					inp++;
				else 
					goto input_failure;
			}
			/*
			 * Note that there is at least one character in
			 * the buffer, so conversions that do not set NOSKIP
			 * can no longer result in an input failure.
			 */
		}

		/*
		 * Do the conversion.
		 */
		switch (c) {

		case CT_CHAR:
			/* scan arbitrary characters (sets NOSKIP) */
			if (width == 0)
				width = 1;
			if (flags & SUPPRESS) {
				size_t sum = 0;
				for (;;) {
					if ((n = inr) < (int)width) {
						sum += n;
						width -= n;
						inp += n;
						if (sum == 0)
							goto input_failure;
						break;
					} else {
						sum += width;
						inr -= width;
						inp += width;
						break;
					}
				}
				nread += sum;
			} else {
				// bcopy(inp, va_arg(ap, char *), width);
				// inr -= width;
				// inp += width;
				// nread += width;
				// nassigned++;
			}
			nconversions++;
			break;

		case CT_CCL:
			/* scan a (nonempty) character class (sets NOSKIP) */
			if (width == 0)
				width = (size_t)~0;	/* `infinity' */
			/* take only those things in the class */
			if (flags & SUPPRESS) {
				n = 0;
				while (ccltab[(unsigned char)*inp]) {
					n++, inr--, inp++;
					if (--width == 0)
						break;
					if (inr <= 0) {
						if (n == 0)
							goto input_failure;
						break;
					}
				}
				if (n == 0)
					goto match_failure;
			} else {
				p0 = p = va_arg(ap, char *);
				while (ccltab[(unsigned char)*inp]) {
					inr--;
					*p++ = *inp++;
					if (--width == 0)
						break;
					if (inr <= 0) {
						if (p == p0)
							goto input_failure;
						break;
					}
				}
				n = p - p0;
				if (n == 0)
					goto match_failure;
				*p = 0;
				nassigned++;
			}
			nread += n;
			nconversions++;
			break;

		case CT_STRING:
			/* like CCL, but zero-length string OK, & no NOSKIP */
			if (width == 0)
				width = (size_t)~0;
			if (flags & SUPPRESS) {
				n = 0;
				while (!isspace(*inp)) {
					n++, inr--, inp++;
					if (--width == 0)
						break;
					if (inr <= 0)
						break;
				}
				nread += n;
			} else {
				p0 = p = va_arg(ap, char *);
				while (!isspace(*inp)) {
					inr--;
					*p++ = *inp++;
					if (--width == 0)
						break;
					if (inr <= 0)
						break;
				}
				*p = 0;
				nread += p - p0;
				nassigned++;
			}
			nconversions++;
			continue;

		case CT_INT:
			/* scan an integer as if by the conversion function */
#ifdef hardway
			if (width == 0 || width > sizeof(buf) - 1)
				width = sizeof(buf) - 1;
#else
			/* size_t is unsigned, hence this optimisation */
			if (--width > sizeof(buf) - 2)
				width = sizeof(buf) - 2;
			width++;
#endif
			flags |= SIGNOK | NDIGITS | NZDIGITS;
			for (p = buf; width; width--) {
				c = *inp;
				/*
				 * Switch on the character; `goto ok'
				 * if we accept it as a part of number.
				 */
				switch (c) {

				/*
				 * The digit 0 is always legal, but is
				 * special.  For %i conversions, if no
				 * digits (zero or nonzero) have been
				 * scanned (only signs), we will have
				 * base==0.  In that case, we should set
				 * it to 8 and enable 0x prefixing.
				 * Also, if we have not scanned zero digits
				 * before this, do not turn off prefixing
				 * (someone else will turn it off if we
				 * have scanned any nonzero digits).
				 */
				case '0':
					if (base == 0) {
						base = 8;
						flags |= PFXOK;
					}
					if (flags & NZDIGITS)
					    flags &= ~(SIGNOK|NZDIGITS|NDIGITS);
					else
					    flags &= ~(SIGNOK|PFXOK|NDIGITS);
					goto ok;

				/* 1 through 7 always legal */
				case '1': case '2': case '3':
				case '4': case '5': case '6': case '7':
					base = basefix[base];
					flags &= ~(SIGNOK | PFXOK | NDIGITS);
					goto ok;

				/* digits 8 and 9 ok iff decimal or hex */
				case '8': case '9':
					base = basefix[base];
					if (base <= 8)
						break;	/* not legal here */
					flags &= ~(SIGNOK | PFXOK | NDIGITS);
					goto ok;

				/* letters ok iff hex */
				case 'A': case 'B': case 'C':
				case 'D': case 'E': case 'F':
				case 'a': case 'b': case 'c':
				case 'd': case 'e': case 'f':
					/* no need to fix base here */
					if (base <= 10)
						break;	/* not legal here */
					flags &= ~(SIGNOK | PFXOK | NDIGITS);
					goto ok;

				/* sign ok only as first character */
				case '+': case '-':
					if (flags & SIGNOK) {
						flags &= ~SIGNOK;
						goto ok;
					}
					break;

				/* x ok iff flag still set & 2nd char */
				case 'x': case 'X':
					if (flags & PFXOK && p == buf + 1) {
						base = 16;	/* if %i */
						flags &= ~PFXOK;
						goto ok;
					}
					break;
				}

				/*
				 * If we got here, c is not a legal character
				 * for a number.  Stop accumulating digits.
				 */
				break;
		ok:
				/*
				 * c is legal: store it and look at the next.
				 */
				*p++ = c;
				if (--inr > 0)
					inp++;
				else 
					break;		/* end of input */
			}
			/*
			 * If we had only a sign, it is no good; push
			 * back the sign.  If the number ends in `x',
			 * it was [sign] '0' 'x', so push back the x
			 * and treat it as [sign] '0'.
			 */
			if (flags & NDIGITS) {
				if (p > buf) {
					inp--;
					inr++;
				}
				goto match_failure;
			}
			c = ((uint8_t *)p)[-1];
			if (c == 'x' || c == 'X') {
				--p;
				inp--;
				inr++;
			}
			if ((flags & SUPPRESS) == 0) {
				uint64_t res;

				*p = 0;
				if ((flags & UNSIGNED) == 0)
				    // res = strtoq(buf, (char **)NULL, base); //!!!
                    res = strtol(buf, (char **)NULL, base);
				else
				    // res = strtouq(buf, (char **)NULL, base); //!!!
                    res = strtol(buf, (char **)NULL, base);
				if (flags & POINTER)
					*va_arg(ap, void **) =
						(void *)(uintptr_t)res;
				else if (flags & SHORTSHORT)
					*va_arg(ap, char *) = res;
				else if (flags & SHORT)
					*va_arg(ap, short *) = res;
				else if (flags & LONG)
					*va_arg(ap, long *) = res;
				else if (flags & LONGLONG)
					*va_arg(ap, long long *) = res;
				else
					*va_arg(ap, int *) = res;
				nassigned++;
			}
			nread += p - buf;
			nconversions++;
			break;

		}
	}
input_failure:
	return (nconversions != 0 ? nassigned : -1);
match_failure:
	return (nassigned);
}

/*
 * Fill in the given table from the scanset at the given format
 * (just after `[').  Return a pointer to the character past the
 * closing `]'.  The table has a 1 wherever characters should be
 * considered part of the scanset.
 */
static const uint8_t *
__libc_sccl(char *tab, const uint8_t *fmt)
{
	int c, n, v;

	/* first `clear' the whole table */
	c = *fmt++;		/* first char hat => negated scanset */
	if (c == '^') {
		v = 1;		/* default => accept */
		c = *fmt++;	/* get new first char */
	} else
		v = 0;		/* default => reject */

	/* XXX: Will not work if sizeof(tab*) > sizeof(char) */
	(void) memset(tab, v, 256);

	if (c == 0)
		return (fmt - 1);/* format ended before closing ] */

	/*
	 * Now set the entries corresponding to the actual scanset
	 * to the opposite of the above.
	 *
	 * The first character may be ']' (or '-') without being special;
	 * the last character may be '-'.
	 */
	v = 1 - v;
	for (;;) {
		tab[c] = v;		/* take character c */
doswitch:
		n = *fmt++;		/* and examine the next */
		switch (n) {

		case 0:			/* format ended too soon */
			return (fmt - 1);

		case '-':
			/*
			 * A scanset of the form
			 *	[01+-]
			 * is defined as `the digit 0, the digit 1,
			 * the character +, the character -', but
			 * the effect of a scanset such as
			 *	[a-zA-Z0-9]
			 * is implementation defined.  The V7 Unix
			 * scanf treats `a-z' as `the letters a through
			 * z', but treats `a-a' as `the letter a, the
			 * character -, and the letter a'.
			 *
			 * For compatibility, the `-' is not considerd
			 * to define a range if the character following
			 * it is either a close bracket (required by ANSI)
			 * or is not numerically greater than the character
			 * we just stored in the table (c).
			 */
			n = *fmt;
			if (n == ']' || n < c) {
				c = '-';
				break;	/* resume the for(;;) */
			}
			fmt++;
			/* fill in the range */
			do {
			    tab[++c] = v;
			} while (c < n);
			c = n;
			/*
			 * Alas, the V7 Unix scanf also treats formats
			 * such as [a-c-e] as `the letters a through e'.
			 * This too is permitted by the standard....
			 */
			goto doswitch;
			break;

		case ']':		/* end of scanset */
			return (fmt);

		default:		/* just another character */
			c = n;
			break;
		}
	}
	/* NOTREACHED */
}