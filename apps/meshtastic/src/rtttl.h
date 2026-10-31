#pragma once
#include "mesh_app.h"

/* v1: custom SD-card .rtttl ringtones are deferred; only the built-in tones
 * are available. These keep the settings/GUI call sites working. */
void ringtones_scan(ZeroMeshApp* app);
uint16_t ringtone_total(const ZeroMeshApp* app);
void ringtone_label(const ZeroMeshApp* app, uint16_t index, char* out, size_t cap);
int16_t ringtone_index_of(const ZeroMeshApp* app, const char* filename);
bool rtttl_play_custom(const ZeroMeshApp* app, uint16_t index);
