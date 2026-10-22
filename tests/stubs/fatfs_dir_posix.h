#ifndef FATFS_DIR_POSIX_H_
#define FATFS_DIR_POSIX_H_

#include <stdbool.h>
#include <stddef.h>

void *ff_stub_dir_open(const char *path);
bool ff_stub_dir_next(void *context, char *name, size_t name_size,
                      bool *is_directory);
void ff_stub_dir_close(void *context);

#endif /* FATFS_DIR_POSIX_H_ */
