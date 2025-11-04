#include <stdio.h>
#include <limits.h>
#include <stdint.h>
#include <math.h>
#include "raylib.h"
#include "raymath.h"

// ----- Utilities -----

#define debug(...)                       \
    {                                    \
        fprintf(debugFile, __VA_ARGS__); \
        fputs("\n", debugFile);          \
        fflush(debugFile);               \
    }

inline int cmpf(float a, float b)
{
    return a + EPSILON < b ? -1 : (b + EPSILON < a ? 1 : 0);
}

FILE *debugFile = NULL;

typedef struct
{
    Vector2 fr; // a.x <= b.x and a.y <= b.y
    Vector2 to;
} Rect;

// ----- Core state -----

struct CoreState
{
};

// ----- View state -----

struct
{
    int windowWidth;
    int windowHeight;
    Rect canvasRect;
} windowState;

struct
{
    // Rectangle of the cartesian plane being rendered
    Rect viewPlaneRect;
} viewState;

struct
{
    int minFps;
    int maxFps;
    double avgFps;
    int64_t frameCount;
} metrics;

void setScreenSize(int width, int height)
{
    windowState.windowWidth = width;
    windowState.windowHeight = height;
    windowState.canvasRect.fr = (Vector2){0.0, 0.0};
    windowState.canvasRect.to = (Vector2){(float)width, (float)height};
}

// ----- View render funcitions -----

// Get screen position given cartesian plane position
Vector2 getScreenPosition(Vector2 p)
{
    p.x = Normalize(p.x, viewState.viewPlaneRect.fr.x, viewState.viewPlaneRect.to.x);
    p.y = Normalize(p.y, viewState.viewPlaneRect.to.y, viewState.viewPlaneRect.fr.y);
    p.x = Lerp(windowState.canvasRect.fr.x, windowState.canvasRect.to.x, p.x);
    p.y = Lerp(windowState.canvasRect.fr.y, windowState.canvasRect.to.y, p.y);
    return p;
}

// Get cartesian plane position given screen position
Vector2 getPlanePosition(Vector2 p)
{
    p.x = Normalize(p.x, windowState.canvasRect.fr.x, windowState.canvasRect.to.x);
    p.y = Normalize(p.y, windowState.canvasRect.fr.y, windowState.canvasRect.to.y);
    p.x = Lerp(viewState.viewPlaneRect.fr.x, viewState.viewPlaneRect.to.x, p.x);
    p.y = Lerp(viewState.viewPlaneRect.to.y, viewState.viewPlaneRect.fr.y, p.y);
    return p;
}

float xScreenToPlaneRatio()
{
    return (windowState.canvasRect.to.x - windowState.canvasRect.fr.x)
           / (viewState.viewPlaneRect.to.x - viewState.viewPlaneRect.fr.x);
}

float yScreenToPlaneRatio()
{
    return (windowState.canvasRect.to.y - windowState.canvasRect.fr.y)
           / (viewState.viewPlaneRect.to.y - viewState.viewPlaneRect.fr.y);
}

float findAxisTickSpacing(float size)
{
    int MAX_TICKS = 15;
    float step = 1;
    const float factor[] = {2, 2.5, 2};
    for (size_t i = 0; step * factor[i] * MAX_TICKS < size; i = (i + 1) % 3)
    {
        step *= factor[i];
    }
    return step;
}

void drawCartesianPlaneAxes()
{
    // Config variables
    Color axisColor = DARKGRAY;
    Color axisNumberColor = axisColor;
    const float TICK_SIZE_FACTOR = 0.2f;
    char buffer[32];

    // Calculate tick spacing, size and text height
    float spacingY, tickSizeY, spacingX, tickSizeX;
    int textHeight = 0;
    bool isXAxisVisible = viewState.viewPlaneRect.fr.y <= 0.0 && viewState.viewPlaneRect.to.y >= 0.0;
    bool isYAxisVisible = viewState.viewPlaneRect.fr.x <= 0.0 && viewState.viewPlaneRect.to.x >= 0.0;
    if (isXAxisVisible) {
        spacingX = findAxisTickSpacing(viewState.viewPlaneRect.to.x - viewState.viewPlaneRect.fr.x);
        tickSizeX = spacingX * xScreenToPlaneRatio() * TICK_SIZE_FACTOR;
        textHeight = (int)tickSizeX * 2;
    }
    if (isYAxisVisible) {
        spacingY = findAxisTickSpacing(viewState.viewPlaneRect.to.y - viewState.viewPlaneRect.fr.y);
        tickSizeY = spacingY * yScreenToPlaneRatio() * TICK_SIZE_FACTOR;
        if (textHeight == 0 || textHeight > (int)tickSizeY * 2)
            textHeight = (int)tickSizeY * 2;
    }

    // Draw X axis
    if (isXAxisVisible)
    {
        Vector2 a = getScreenPosition((Vector2){viewState.viewPlaneRect.fr.x, 0.0});
        Vector2 b = getScreenPosition((Vector2){viewState.viewPlaneRect.to.x, 0.0});
        DrawLineV(a, b, axisColor);
        
        float firstTick = ceilf(viewState.viewPlaneRect.fr.x / spacingX) * spacingX;
        for (float tick = firstTick; tick <= viewState.viewPlaneRect.to.x; tick += spacingX)
        {
            if (cmpf(tick, 0.0f) == 0)
                continue;

            Vector2 tickPos = getScreenPosition((Vector2){tick, 0.0});
            DrawLine(tickPos.x, tickPos.y - tickSizeX, tickPos.x, tickPos.y + tickSizeX, axisColor); // Draw the tick
            snprintf(buffer, sizeof(buffer), "%.0f", tick);
            int textWidth = MeasureText(buffer, textHeight);
            // Draw the tick number
            DrawText(buffer, tickPos.x - 0.5f * textWidth, tickPos.y - textHeight - tickSizeX - 1, textHeight, axisNumberColor);
        }
    }

    // Draw Y axis
    if (isYAxisVisible)
    {
        Vector2 a = getScreenPosition((Vector2){0.0, viewState.viewPlaneRect.fr.y});
        Vector2 b = getScreenPosition((Vector2){0.0, viewState.viewPlaneRect.to.y});
        DrawLineV(a, b, axisColor);

        float firstTick = ceilf(viewState.viewPlaneRect.fr.y / spacingY) * spacingY;
        for (float tick = firstTick; tick <= viewState.viewPlaneRect.to.y; tick += spacingY)
        {
            if (cmpf(tick, 0.0f) == 0)
                continue;

            Vector2 tickPos = getScreenPosition((Vector2){0.0, tick});
            DrawLine(tickPos.x - tickSizeY, tickPos.y, tickPos.x + tickSizeY, tickPos.y, axisColor); // Draw the tick
            snprintf(buffer, sizeof(buffer), "%.0f", tick);
            // Draw the tick number
            DrawText(buffer, tickPos.x + tickSizeY + 1, tickPos.y - textHeight / 2.0f, textHeight, axisNumberColor);
        }
    }
}


void drawLatticePointCloseToMouse() {
    Vector2 mousePos = GetMousePosition();
    Vector2 planePos = getPlanePosition(mousePos);

    Vector2 latticePoint;
    latticePoint.x = roundf(planePos.x);
    latticePoint.y = roundf(planePos.y);

    float dx = 1000.0 * fabsf((latticePoint.x - planePos.x) * xScreenToPlaneRatio()) / windowState.windowWidth;
    float dy = 1000.0 * fabsf((latticePoint.y - planePos.y) * yScreenToPlaneRatio()) / windowState.windowHeight;
    float distance = hypotf(dx, dy); // distance in pixels if the screen was 1000 pixels wide/high
    if (distance < 8.0)
    {
        Vector2 screenPos = getScreenPosition(latticePoint);
        DrawCircleV(screenPos, 5.0f, BLUE);
    }
}


// ----- Main loop functions -----

void initialize()
{
    debugFile = fopen("debug.log", "w");
    debug("Initializing..");

    setScreenSize(800, 800);

    viewState.viewPlaneRect.fr = (Vector2){-10.0, -10.0};
    viewState.viewPlaneRect.to = (Vector2){20.0, 20.0};

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(windowState.windowWidth, windowState.windowHeight, "Computer Assisted 2D Geometry");
    SetWindowMinSize(400, 400);

    SetTargetFPS(60);

    metrics.minFps = INT_MAX;
    debug("Done initializing..");
}

void cleanUp()
{
    debug("Cleaning up..");
    debug("Min FPS: %d", metrics.minFps);
    debug("Max FPS: %d", metrics.maxFps);
    debug("Avg FPS: %lf", metrics.avgFps);
    CloseWindow();
    debug("Done cleaning up..");
    fclose(debugFile);
}

void update()
{
    setScreenSize(GetScreenWidth(), GetScreenHeight());
}

void draw()
{
    Vector2 a = {-5.0, -8.0};
    Vector2 b = {3.0, 13.0};

    a = getScreenPosition(a);
    b = getScreenPosition(b);

    BeginDrawing();
    ClearBackground(RAYWHITE);
    drawCartesianPlaneAxes();
    DrawLineV(a, b, RED);
    drawLatticePointCloseToMouse();
    EndDrawing();
}

void gatherMetrics()
{
    int fps = GetFPS();
    metrics.minFps = fps < metrics.minFps ? fps : metrics.minFps;
    metrics.maxFps = fps > metrics.maxFps ? fps : metrics.maxFps;
    metrics.frameCount++;
    metrics.avgFps += (fps - metrics.avgFps) / metrics.frameCount;
}

int main(void)
{
    initialize();

    while (!WindowShouldClose())
    {
        update();
        draw();
        gatherMetrics();
    }

    cleanUp();
    return 0;
}