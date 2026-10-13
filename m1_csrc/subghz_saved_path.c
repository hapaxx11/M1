/* See COPYING.txt for license details. */

#include "subghz_saved_path.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

bool subghz_saved_path_build(char *output,
                             size_t output_size,
                             const char *directory,
                             const char *filename)
{
    if (output == NULL || output_size == 0U ||
        directory == NULL || filename == NULL || filename[0] == '\0' ||
        strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0)
        return false;

    for (const char *ch = filename; *ch != '\0'; ++ch)
    {
        if (*ch == '/' || *ch == '\\')
            return false;
    }

    if (directory[0] == '0' && directory[1] == ':')
        directory += 2;
    size_t directory_length = strlen(directory);
    if (directory_length < 7U || directory_length > INT_MAX ||
        strncmp(directory, "/SUBGHZ", 7U) != 0 ||
        (directory[7] != '\0' && directory[7] != '/'))
        return false;

    const char *segment = directory + 7U;
    while (*segment != '\0')
    {
        while (*segment == '/')
            ++segment;
        const char *end = segment;
        while (*end != '\0' && *end != '/')
            ++end;
        size_t segment_length = (size_t)(end - segment);
        if ((segment_length == 1U && segment[0] == '.') ||
            (segment_length == 2U && segment[0] == '.' &&
             segment[1] == '.'))
            return false;
        segment = end;
    }

    while (directory_length > 7U && directory[directory_length - 1U] == '/')
        --directory_length;

    int length = snprintf(output, output_size, "%.*s/%s",
                          (int)directory_length, directory, filename);
    return length >= 0 && (size_t)length < output_size;
}
