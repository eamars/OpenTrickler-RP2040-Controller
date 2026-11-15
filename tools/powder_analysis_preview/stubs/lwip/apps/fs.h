// Host stub of the lwIP fs.h: just what the REST handlers of the powder analysis touch
#pragma once
#include <stddef.h>
#define FS_FILE_FLAGS_HEADER_INCLUDED 1
#define FS_FILE_FLAGS_HEADER_PERSISTENT 2
struct fs_file { const char *data; int len; int index; int flags; };
