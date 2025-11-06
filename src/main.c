#include <stdio.h>
#include <limits.h>
#include <stdint.h>
#include <math.h>
#include <stdlib.h>
#include "raylib.h"
#include "raymath.h"

// ----- Utilities -----

#define Debug(...)                       \
    {                                    \
        fprintf(DebugFile, __VA_ARGS__); \
        fputs("\n", DebugFile);          \
        fflush(DebugFile);               \
    }

inline int Compare(double a, double b)
{
    return a + EPSILON < b ? -1 : (b + EPSILON < a ? 1 : 0);
}

FILE *DebugFile = NULL;

// Dynamic array macros
#define ListSize(da)               (da?((size_t*)da)[-1]:0)
#define ListCapacity(da)           (da?((size_t*)da)[-2]:0)
#define ListFree(da)               {if(da) free((size_t*)da - 2); da = NULL;}
#define ListPop(da)                da[--(((size_t*)da)[-1])]
#define ListPush(da, value) {                                                  \
        if(!da) {                                                              \
            size_t* _ptr = malloc(2 * sizeof(size_t) + 4 * sizeof(*(da)));     \
            da = (void*)(_ptr + 2);                                            \
            _ptr[0] = 4;                                                       \
            _ptr[1] = 0;                                                       \
        } else if (ListSize(da) == ListCapacity(da)) {                         \
            const size_t _newCapacity = ListCapacity(da) * 2 + 4;              \
            const size_t _newAllocSize = sizeof(*da) * _newCapacity            \
                                         + 2 * sizeof(size_t);                 \
            size_t* _ptr = realloc((size_t*)da - 2, _newAllocSize);            \
            da = (void*)(_ptr + 2);                                            \
            _ptr[0] = _newCapacity;                                            \
        }                                                                      \
        da[((size_t*)da)[-1]++] = value;                                       \
    }

// ----- Core state -----

typedef struct
{
    double x;
    double y;
} Point;

typedef struct {
    size_t a;
    size_t b;
} LineSegment;

typedef struct
{
    Point* points;
    LineSegment* segments;
} CoreStateSingleton;

CoreStateSingleton CoreState = {0};

size_t FindPoint(Point p) {
    size_t nPoints = ListSize(CoreState.points);
    for (size_t i = 0; i < nPoints; i++) {
        if (Compare(CoreState.points[i].x, p.x) == 0 &&
            Compare(CoreState.points[i].y, p.y) == 0) {
            return (int)i;
        }
    }
    return SIZE_MAX;
}

size_t AddPoint(Point p) {
    size_t index = FindPoint(p);
    if (index != SIZE_MAX) return (size_t)index;
    ListPush(CoreState.points, p);
    return ListSize(CoreState.points) - 1;
}

void AddSegment(LineSegment s) {
    ListPush(CoreState.segments, s);
}

void InitializeCoreState()
{
    size_t a = AddPoint((Point){.x= -5.0, .y=-8.0});
    size_t b = AddPoint((Point){.x= 8.0,  .y=13.0});
    size_t c = AddPoint((Point){.x= 5.0,  .y=5.0 });
    size_t d = AddPoint((Point){.x= 5.0,  .y=10.0});
    AddSegment((LineSegment){.a=a, .b=b});
    AddSegment((LineSegment){.a=c, .b=d});
}

void FreeCoreStateSingleton()
{
    free(CoreState.points);
    free(CoreState.segments);
}

// ----- View state -----

typedef struct
{
    Vector2 fr; // a.x <= b.x and a.y <= b.y
    Vector2 to;
} Rect;

typedef struct
{
    int windowWidth;
    int windowHeight;
    Rect canvasRect;
} WindowStateSingleton;

typedef struct
{
    // Rectangle of the cartesian plane being rendered
    Rect viewPlaneRect;
} ViewStateSingleton;

WindowStateSingleton WindowState = {0};
ViewStateSingleton ViewState = {0};

typedef struct {
    int minFps;
    int maxFps;
    double avgFps;
    int64_t frameCount;
} MetricsSingleton;

MetricsSingleton Metrics = {0};

void SetScreenSize(int width, int height)
{
    WindowState.windowWidth = width;
    WindowState.windowHeight = height;
    WindowState.canvasRect.fr = (Vector2){0.0, 0.0};
    WindowState.canvasRect.to = (Vector2){(float)width, (float)height};
}

// ----- View render funcitions -----

// Get screen position given cartesian plane position
Vector2 ToVector2(Point p)
{
    Vector2 r = {
        .x = Normalize(p.x, ViewState.viewPlaneRect.fr.x, ViewState.viewPlaneRect.to.x),
        .y = Normalize(p.y, ViewState.viewPlaneRect.to.y, ViewState.viewPlaneRect.fr.y)
    };
    r.x = Lerp(WindowState.canvasRect.fr.x, WindowState.canvasRect.to.x, r.x);
    r.y = Lerp(WindowState.canvasRect.fr.y, WindowState.canvasRect.to.y, r.y);
    return r;
}

// Get cartesian plane position given screen position
Point ToPoint(Vector2 p)
{
    Point r = {
        .x = Normalize(p.x, WindowState.canvasRect.fr.x, WindowState.canvasRect.to.x),
        .y = Normalize(p.y, WindowState.canvasRect.fr.y, WindowState.canvasRect.to.y)
    };
    r.x = Lerp(ViewState.viewPlaneRect.fr.x, ViewState.viewPlaneRect.to.x, r.x);
    r.y = Lerp(ViewState.viewPlaneRect.to.y, ViewState.viewPlaneRect.fr.y, r.y);
    return r;
}

float XScreenToPlaneRatio()
{
    return (WindowState.canvasRect.to.x - WindowState.canvasRect.fr.x)
           / (ViewState.viewPlaneRect.to.x - ViewState.viewPlaneRect.fr.x);
}

float YScreenToPlaneRatio()
{
    return (WindowState.canvasRect.to.y - WindowState.canvasRect.fr.y)
           / (ViewState.viewPlaneRect.to.y - ViewState.viewPlaneRect.fr.y);
}

float FindAxisTickSpacing(float size)
{
    const int maxTicks = 15;
    float step = 1;
    const float factor[] = {2, 2.5, 2};
    for (size_t i = 0; step * factor[i] * maxTicks < size; i = (i + 1) % 3)
    {
        step *= factor[i];
    }
    return step;
}

void RenderCartesianPlaneAxes()
{
    // Config variables
    Color axisColor = DARKGRAY;
    Color axisNumberColor = axisColor;
    const float TICK_SIZE_FACTOR = 0.2f;
    char buffer[32];

    // Calculate tick spacing, size and text height
    float spacingY, tickSizeY, spacingX, tickSizeX;
    int textHeight = 0;
    bool isXAxisVisible = ViewState.viewPlaneRect.fr.y <= 0.0 && ViewState.viewPlaneRect.to.y >= 0.0;
    bool isYAxisVisible = ViewState.viewPlaneRect.fr.x <= 0.0 && ViewState.viewPlaneRect.to.x >= 0.0;
    if (isXAxisVisible) {
        spacingX = FindAxisTickSpacing(ViewState.viewPlaneRect.to.x - ViewState.viewPlaneRect.fr.x);
        tickSizeX = spacingX * XScreenToPlaneRatio() * TICK_SIZE_FACTOR;
        textHeight = (int)tickSizeX * 2;
    }
    if (isYAxisVisible) {
        spacingY = FindAxisTickSpacing(ViewState.viewPlaneRect.to.y - ViewState.viewPlaneRect.fr.y);
        tickSizeY = spacingY * YScreenToPlaneRatio() * TICK_SIZE_FACTOR;
        if (textHeight == 0 || textHeight > (int)tickSizeY * 2)
            textHeight = (int)tickSizeY * 2;
    }

    // Draw X axis
    if (isXAxisVisible)
    {
        Vector2 a = ToVector2((Point){ViewState.viewPlaneRect.fr.x, 0.0});
        Vector2 b = ToVector2((Point){ViewState.viewPlaneRect.to.x, 0.0});
        DrawLineV(a, b, axisColor);
        
        float firstTick = ceilf(ViewState.viewPlaneRect.fr.x / spacingX) * spacingX;
        for (float tick = firstTick; tick <= ViewState.viewPlaneRect.to.x; tick += spacingX)
        {
            if (Compare(tick, 0.0f) == 0)
                continue;

            Vector2 tickPos = ToVector2((Point){tick, 0.0});
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
        Vector2 a = ToVector2((Point){0.0, ViewState.viewPlaneRect.fr.y});
        Vector2 b = ToVector2((Point){0.0, ViewState.viewPlaneRect.to.y});
        DrawLineV(a, b, axisColor);

        float firstTick = ceilf(ViewState.viewPlaneRect.fr.y / spacingY) * spacingY;
        for (float tick = firstTick; tick <= ViewState.viewPlaneRect.to.y; tick += spacingY)
        {
            if (Compare(tick, 0.0f) == 0)
                continue;

            Vector2 tickPos = ToVector2((Point){0.0, tick});
            DrawLine(tickPos.x - tickSizeY, tickPos.y, tickPos.x + tickSizeY, tickPos.y, axisColor); // Draw the tick
            snprintf(buffer, sizeof(buffer), "%.0f", tick);
            // Draw the tick number
            DrawText(buffer, tickPos.x + tickSizeY + 1, tickPos.y - textHeight / 2.0f, textHeight, axisNumberColor);
        }
    }
}

void RenderLatticePointCloseToMouse() {
    Point point = ToPoint(GetMousePosition());

    Point latticePoint;
    latticePoint.x = roundf(point.x);
    latticePoint.y = roundf(point.y);

    double dx = 1000.0 * fabs((latticePoint.x - point.x) * XScreenToPlaneRatio()) / WindowState.windowWidth;
    double dy = 1000.0 * fabs((latticePoint.y - point.y) * YScreenToPlaneRatio()) / WindowState.windowHeight;
    double distance = hypotf(dx, dy); // distance in pixels if the screen was 1000 pixels wide/high
    if (distance < 8.0)
    {
        Vector2 screenPos = ToVector2(latticePoint);
        DrawCircleV(screenPos, 5.0f, BLUE);
    }
}

void RenderCoreState()
{
    size_t nSegments = ListSize(CoreState.segments);
    for (size_t i = 0; i < nSegments; i++)
    {
        LineSegment seg = CoreState.segments[i];
        Point a = CoreState.points[seg.a];
        Point b = CoreState.points[seg.b];
        Debug("Drawing segment from (%f, %f) to (%f, %f)", a.x, a.y, b.x, b.y);
        DrawLineV(ToVector2(a), ToVector2(b), RED);
    }
}

// ----- Main loop functions -----

void Initialize()
{
    DebugFile = fopen("Debug.log", "w");
    Debug("Initializing..");

    SetScreenSize(800, 800);

    ViewState.viewPlaneRect.fr = (Vector2){-10.0, -10.0};
    ViewState.viewPlaneRect.to = (Vector2){20.0, 20.0};

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(WindowState.windowWidth, WindowState.windowHeight, "Computer Assisted 2D Geometry");
    SetWindowMinSize(400, 400);

    SetTargetFPS(60);
    Metrics.minFps = INT_MAX;

    InitializeCoreState();
    Debug("Done initializing..");
}

void CleanUp()
{
    Debug("Cleaning up..");
    Debug("Min FPS: %d", Metrics.minFps);
    Debug("Max FPS: %d", Metrics.maxFps);
    Debug("Avg FPS: %lf", Metrics.avgFps);
    CloseWindow();
    FreeCoreStateSingleton();
    Debug("Done cleaning up..");
    fclose(DebugFile);
}

void Update()
{
    SetScreenSize(GetScreenWidth(), GetScreenHeight());
}

void Draw()
{
    BeginDrawing();
    ClearBackground(RAYWHITE);
    RenderCartesianPlaneAxes();
    RenderCoreState();
    RenderLatticePointCloseToMouse();
    EndDrawing();
}

void GatherMetrics()
{
    int fps = GetFPS();
    Metrics.minFps = fps < Metrics.minFps ? fps : Metrics.minFps;
    Metrics.maxFps = fps > Metrics.maxFps ? fps : Metrics.maxFps;
    Metrics.frameCount++;
    Metrics.avgFps += (fps - Metrics.avgFps) / Metrics.frameCount;
}

int main(void)
{
    Initialize();

    while (!WindowShouldClose())
    {
        Update();
        Draw();
        GatherMetrics();
    }

    CleanUp();
    return 0;
}