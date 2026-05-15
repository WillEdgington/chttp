#ifndef CHTTP_FS_H
#define CHTTP_FS_H

#include "clib/arena.h"
#include <stddef.h>

char *chttp_resolve_path(const char *request_path, const char *base_dir,
                         Arena *arena);
char *chttp_read_file(const char *full_path, Arena *arena, size_t *out_size);

#endif