/* See COPYING.txt for license details. */

/*
 * subghz_playlist_parser.h
 *
 * Path remapping utility for Sub-GHz playlist files.
 * Extracted from m1_subghz_scene_playlist.c.
 *
 * Converts Flipper-style paths (e.g. "/ext/subghz/foo.sub") to M1
 * convention (e.g. "/SUBGHZ/foo.sub") so playlist files from
 * UberGuidoZ/Flipper collections work without manual editing.
 *
 * This module is hardware-independent and compiles on both ARM and host.
 *
 * M1 Project — Hapax fork
 */

#ifndef SUBGHZ_PLAYLIST_PARSER_H
#define SUBGHZ_PLAYLIST_PARSER_H

#include <stddef.h>

/**
 * Remap a Flipper-style path to M1 convention.
 *
 * Converts "/ext/subghz/..." → "/SUBGHZ/..." so that playlist files
 * from UberGuidoZ/Flipper work without manual editing.
 *
 * If the path does not match the Flipper convention, it is copied
 * unchanged.
 *
 * @param src   Source path (trimmed, null-terminated)
 * @param dst   Destination buffer
 * @param dlen  Size of destination buffer (must be > 0)
 */
void subghz_remap_flipper_path(const char *src, char *dst, size_t dlen);

#endif /* SUBGHZ_PLAYLIST_PARSER_H */
