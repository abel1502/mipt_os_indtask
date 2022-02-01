#include <inc/libdoom.h>


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

