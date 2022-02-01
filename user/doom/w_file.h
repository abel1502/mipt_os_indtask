//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
//
// This program is libc_free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the libc_free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//	WAD I/O functions.
//


#ifndef __W_FILE__
#define __W_FILE__

// #include <stdio.h>
#include "doomtype.h"

typedef struct _wad_file_s wad_file_t;

typedef struct
{
    // Open a libc_FILE for reading.

    wad_file_t *(*OpenFile)(char *path);

    // Close the specified libc_FILE.

    void (*CloseFile)(wad_file_t *libc_FILE);

    // Read data from the specified position in the libc_FILE into the 
    // provided buffer.  Returns the number of bytes read.

    size_t (*Read)(wad_file_t *libc_FILE, unsigned int offset,
                   void *buffer, size_t buffer_len);

} wad_file_class_t;

struct _wad_file_s
{
    // Class of this libc_FILE.

    wad_file_class_t *file_class;

    // If this is NULL, the libc_FILE cannot be mapped into memory.  If this
    // is non-NULL, it is a pointer to the mapped libc_FILE.

    byte *mapped;

    // Length of the libc_FILE, in bytes.

    unsigned int length;
};

// Open the specified libc_FILE. Returns a pointer to a new wad_file_t 
// handle for the WAD libc_FILE, or NULL if it could not be opened.

wad_file_t *W_OpenFile(char *path);

// Close the specified WAD libc_FILE.

void W_CloseFile(wad_file_t *wad);

// Read data from the specified libc_FILE into the provided buffer.  The
// data is read from the specified offset from the start of the libc_FILE.
// Returns the number of bytes read.

size_t W_Read(wad_file_t *wad, unsigned int offset,
              void *buffer, size_t buffer_len);

#endif /* #ifndef __W_FILE__ */
