#include "raylib.h"
#include "raymath.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define SCREEN_WIDTH 1200
#define SCREEN_HEIGHT 800
#define SAVE_PROMPT_DURATION 2.0f
#define FONT_PATH "/usr/share/fonts/TTF/DejaVuSans.ttf"
#define STROKE_INITIAL_CAPACITY 64
#define PEN_GREEN (Color){34, 197, 94, 255}

typedef enum {
    BG_WHITE,
    BG_BLACK,
    BG_DARKGRAY
} BackgroundState;

typedef struct {
    Vector2 *points;
    int count;
    int capacity;
    Color color;
} Stroke;

typedef struct {
    Stroke *items;
    int count;
    int capacity;
} StrokeStack;

static const char *background_name(BackgroundState bg) {
    switch (bg) {
        case BG_WHITE: return "white";
        case BG_BLACK: return "black";
        case BG_DARKGRAY: return "darkgray";
        default: return "unknown";
    }
}

static Color background_color(BackgroundState bg) {
    switch (bg) {
        case BG_WHITE: return WHITE;
        case BG_BLACK: return BLACK;
        case BG_DARKGRAY: return (Color){0x18, 0x18, 0x18, 0xFF};
        default: return WHITE;
    }
}

static void next_background(BackgroundState *bg) {
    *bg = (*bg + 1) % 3;
}

static Color auto_pen_color(BackgroundState bg) {
    if (bg == BG_BLACK || bg == BG_DARKGRAY) {
        return WHITE;
    }
    return BLACK;
}

static bool is_white_or_black(Color c) {
    return (c.r == 255 && c.g == 255 && c.b == 255 && c.a == 255) ||
           (c.r == 0 && c.g == 0 && c.b == 0 && c.a == 255);
}

static void generate_timestamped_filename(char *out, size_t out_size) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(out, out_size, "drawing_%Y%m%d_%H%M%S.png", tm_info);
}

static void save_canvas_to_file(RenderTexture2D canvas, BackgroundState bg_state, const char *filename) {
    RenderTexture2D save_texture = LoadRenderTexture(SCREEN_WIDTH, SCREEN_HEIGHT);
    BeginTextureMode(save_texture);
    ClearBackground(background_color(bg_state));
    DrawTextureRec(
        canvas.texture,
        (Rectangle){0.0f, 0.0f, (float)SCREEN_WIDTH, -(float)SCREEN_HEIGHT},
        (Vector2){0.0f, 0.0f},
        WHITE
    );
    EndTextureMode();

    Image img = LoadImageFromTexture(save_texture.texture);
    ImageFlipVertical(&img);
    ExportImage(img, filename);
    UnloadImage(img);
    UnloadRenderTexture(save_texture);
}

static void stroke_init(Stroke *stroke, Color color) {
    stroke->points = (Vector2 *)malloc(STROKE_INITIAL_CAPACITY * sizeof(Vector2));
    stroke->count = 0;
    stroke->capacity = STROKE_INITIAL_CAPACITY;
    stroke->color = color;
}

static void stroke_free(Stroke *stroke) {
    free(stroke->points);
    stroke->points = NULL;
    stroke->count = 0;
    stroke->capacity = 0;
}

static void stroke_add_point(Stroke *stroke, Vector2 point) {
    if (stroke->count >= stroke->capacity) {
        stroke->capacity *= 2;
        stroke->points = (Vector2 *)realloc(stroke->points, stroke->capacity * sizeof(Vector2));
    }
    stroke->points[stroke->count++] = point;
}

static void stroke_draw(const Stroke *stroke) {
    for (int i = 1; i < stroke->count; i++) {
        DrawLineV(stroke->points[i - 1], stroke->points[i], stroke->color);
    }
}

static void stack_init(StrokeStack *stack) {
    stack->items = NULL;
    stack->count = 0;
    stack->capacity = 0;
}

static void stack_free(StrokeStack *stack) {
    for (int i = 0; i < stack->count; i++) {
        stroke_free(&stack->items[i]);
    }
    free(stack->items);
    stack->items = NULL;
    stack->count = 0;
    stack->capacity = 0;
}

static void stack_push(StrokeStack *stack, Stroke stroke) {
    if (stack->count >= stack->capacity) {
        stack->capacity = stack->capacity == 0 ? 16 : stack->capacity * 2;
        stack->items = (Stroke *)realloc(stack->items, stack->capacity * sizeof(Stroke));
    }
    stack->items[stack->count++] = stroke;
}

static Stroke stack_pop(StrokeStack *stack) {
    Stroke empty = {0};
    if (stack->count <= 0) return empty;
    return stack->items[--stack->count];
}

static void stack_clear(StrokeStack *stack) {
    for (int i = 0; i < stack->count; i++) {
        stroke_free(&stack->items[i]);
    }
    stack->count = 0;
}

static void redraw_canvas(RenderTexture2D canvas, const StrokeStack *undo_stack, const Stroke *active_stroke) {
    BeginTextureMode(canvas);
    ClearBackground(BLANK);
    for (int i = 0; i < undo_stack->count; i++) {
        stroke_draw(&undo_stack->items[i]);
    }
    if (active_stroke != NULL && active_stroke->count > 0) {
        stroke_draw(active_stroke);
    }
    EndTextureMode();
}

int main(void) {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Drawing Table");
    SetTargetFPS(60);

    Font font = LoadFontEx(FONT_PATH, 32, NULL, 0);

    RenderTexture2D canvas = LoadRenderTexture(SCREEN_WIDTH, SCREEN_HEIGHT);
    BeginTextureMode(canvas);
    ClearBackground(BLANK);
    EndTextureMode();

    BackgroundState bg_state = BG_WHITE;
    Color pen_color = BLACK;

    StrokeStack undo_stack;
    StrokeStack redo_stack;
    stack_init(&undo_stack);
    stack_init(&redo_stack);

    Stroke active_stroke;
    bool has_active_stroke = false;

    bool show_save_prompt = false;
    float save_prompt_timer = 0.0f;
    char saved_filename[256] = {0};

    bool canvas_dirty = true;

    while (!WindowShouldClose()) {
        Vector2 mouse = GetMousePosition();
        bool ctrl_down = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);

        if (IsKeyPressed(KEY_B)) {
            next_background(&bg_state);
            if (is_white_or_black(pen_color)) {
                pen_color = auto_pen_color(bg_state);
            }
        }

        if (IsKeyPressed(KEY_R)) {
            pen_color = RED;
        }

        if (IsKeyPressed(KEY_G)) {
            pen_color = PEN_GREEN;
        }

        if (IsKeyPressed(KEY_W)) {
            pen_color = auto_pen_color(bg_state);
        }

        if (IsKeyPressed(KEY_Z) && undo_stack.count > 0 && !has_active_stroke) {
            stack_push(&redo_stack, stack_pop(&undo_stack));
            canvas_dirty = true;
        }

        if (IsKeyPressed(KEY_Y) && redo_stack.count > 0 && !has_active_stroke) {
            stack_push(&undo_stack, stack_pop(&redo_stack));
            canvas_dirty = true;
        }

        if (IsKeyPressed(KEY_P)) {
            generate_timestamped_filename(saved_filename, sizeof(saved_filename));
            save_canvas_to_file(canvas, bg_state, saved_filename);
            show_save_prompt = true;
            save_prompt_timer = SAVE_PROMPT_DURATION;
        }

        if (IsKeyPressed(KEY_Q)) {
            time_t now = time(NULL);
            struct tm *tm_info = localtime(&now);
            char backup_path[256];
            strftime(backup_path, sizeof(backup_path), "/tmp/drawing_backup_%Y%m%d_%H%M%S.png", tm_info);
            save_canvas_to_file(canvas, bg_state, backup_path);
            break;
        }

        if (show_save_prompt) {
            save_prompt_timer -= GetFrameTime();
            if (save_prompt_timer <= 0.0f) {
                show_save_prompt = false;
            }
        }

        if (ctrl_down) {
            if (!has_active_stroke) {
                stroke_init(&active_stroke, pen_color);
                has_active_stroke = true;
                stack_clear(&redo_stack);
                active_stroke.points[0] = mouse;
                active_stroke.count = 1;
            }

            if (Vector2Distance(mouse, active_stroke.points[active_stroke.count - 1]) > 0.0f) {
                stroke_add_point(&active_stroke, mouse);
                canvas_dirty = true;
            }
        } else if (has_active_stroke) {
            if (active_stroke.count > 1) {
                stack_push(&undo_stack, active_stroke);
            } else {
                stroke_free(&active_stroke);
            }
            has_active_stroke = false;
            canvas_dirty = true;
        }

        if (canvas_dirty) {
            redraw_canvas(canvas, &undo_stack, has_active_stroke ? &active_stroke : NULL);
            canvas_dirty = false;
        }

        BeginDrawing();
        ClearBackground(background_color(bg_state));
        DrawTextureRec(
            canvas.texture,
            (Rectangle){0.0f, 0.0f, (float)SCREEN_WIDTH, -(float)SCREEN_HEIGHT},
            (Vector2){0.0f, 0.0f},
            WHITE
        );

        DrawCircleV(mouse, 4.0f, pen_color);

        const float menu_font_size = 18.0f;
        const float menu_spacing = 1.0f;
        Color menu_color = (bg_state == BG_WHITE) ? DARKGRAY : LIGHTGRAY;

        DrawTextEx(font, "CTRL: draw | B: bg | R: red | G: green | W: white/black | Z: undo | Y: redo | P: save | Q: quit",
                   (Vector2){10.0f, 10.0f}, menu_font_size, menu_spacing, menu_color);

        const char *status_text = TextFormat("Background: %s | Pen:", background_name(bg_state));
        DrawTextEx(font, status_text, (Vector2){10.0f, 35.0f}, menu_font_size, menu_spacing, menu_color);

        Vector2 status_size = MeasureTextEx(font, status_text, menu_font_size, menu_spacing);
        DrawRectangle(15.0f + status_size.x, 35.0f, 18.0f, 18.0f, pen_color);

        if (show_save_prompt) {
            const float prompt_font_size = 16.0f;
            const float prompt_spacing = 1.0f;
            const char *msg = TextFormat("Saved: %s", saved_filename);

            Vector2 msg_size = MeasureTextEx(font, msg, prompt_font_size, prompt_spacing);
            const float padding = 10.0f;
            float popup_x = SCREEN_WIDTH - msg_size.x - 2.0f * padding;
            float popup_y = SCREEN_HEIGHT - msg_size.y - 2.0f * padding;
            float popup_w = msg_size.x + 2.0f * padding;
            float popup_h = msg_size.y + 2.0f * padding;

            DrawRectangle(popup_x, popup_y, popup_w, popup_h, Fade(BLACK, 0.75f));
            DrawTextEx(font, msg, (Vector2){popup_x + padding, popup_y + padding},
                       prompt_font_size, prompt_spacing, WHITE);
        }

        EndDrawing();
    }

    stack_free(&undo_stack);
    stack_free(&redo_stack);
    if (has_active_stroke) {
        stroke_free(&active_stroke);
    }
    UnloadRenderTexture(canvas);
    UnloadFont(font);
    CloseWindow();

    return 0;
}
