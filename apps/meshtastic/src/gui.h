#pragma once
#include "mesh_app.h"

void draw_header(Canvas* canvas, ZeroMeshApp* app, const char* title);
void draw_list_scrollbar(Canvas* canvas, uint8_t selected, uint8_t count, uint8_t visible);
void render_page(Canvas* canvas, ZeroMeshApp* app);   /* renders app->ui_mode */
void input_dispatch(InputEvent* e, ZeroMeshApp* app); /* handles a button event */
