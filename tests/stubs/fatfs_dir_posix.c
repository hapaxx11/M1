#include "fatfs_dir_posix.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>

void *ff_stub_dir_open(const char *path)
{
    return opendir(path);
}

bool ff_stub_dir_next(void *context, char *name, size_t name_size,
                      bool *is_directory)
{
    if (!context || !name || name_size == 0 || !is_directory)
        return false;

    struct dirent *entry = readdir((DIR *)context);
    if (!entry)
    {
        name[0] = '\0';
        return false;
    }

    snprintf(name, name_size, "%s", entry->d_name);
    *is_directory = entry->d_type == DT_DIR;
    return true;
}

void ff_stub_dir_close(void *context)
{
    if (context)
        closedir((DIR *)context);
}
