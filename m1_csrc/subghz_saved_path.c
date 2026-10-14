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

bool subghz_saved_path_rename_build(char *output,
                                    size_t output_size,
                                    const char *existing_path,
                                    const char *new_filename)
{
    if (existing_path == NULL)
        return false;

    const char *slash = strrchr(existing_path, '/');
    if (slash == NULL || slash == existing_path)
        return false;

    size_t directory_length = (size_t)(slash - existing_path);
    if (directory_length >= SUBGHZ_SAVED_PATH_MAX)
        return false;

    char directory[SUBGHZ_SAVED_PATH_MAX];
    memcpy(directory, existing_path, directory_length);
    directory[directory_length] = '\0';
    return subghz_saved_path_build(output, output_size, directory,
                                   new_filename);
}

static bool subghz_path_extension_equal(const char *extension,
                                        const char *expected)
{
    while (*extension != '\0' && *expected != '\0')
    {
        char a = *extension++;
        char b = *expected++;
        if (a >= 'A' && a <= 'Z')
            a = (char)(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z')
            b = (char)(b - 'A' + 'a');
        if (a != b)
            return false;
    }
    return *extension == *expected;
}

bool subghz_saved_path_recovered_build(char *output,
                                       size_t output_size,
                                       const char *source_path)
{
    static const char suffix[] = "_recovered.sub";
    const char *filename;
    const char *dot;
    size_t stem_length;
    size_t suffix_length = sizeof(suffix) - 1U;

    if (output == NULL || output_size == 0U ||
        source_path == NULL || source_path[0] == '\0')
        return false;

    filename = strrchr(source_path, '/');
    filename = filename == NULL ? source_path : filename + 1;
    dot = strrchr(filename, '.');
    stem_length = strlen(source_path);
    if (dot != NULL &&
        (subghz_path_extension_equal(dot, ".sub") ||
         subghz_path_extension_equal(dot, ".sgh")))
        stem_length = (size_t)(dot - source_path);

    if (stem_length >= output_size ||
        suffix_length >= output_size - stem_length)
        return false;
    memcpy(output, source_path, stem_length);
    memcpy(output + stem_length, suffix, suffix_length + 1U);
    return true;
}
