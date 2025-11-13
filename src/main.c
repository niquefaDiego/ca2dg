#include <stdio.h>
#include <limits.h>
#include <stdint.h>
#include <math.h>
#include <stdlib.h>
#include <assert.h>
#include "raylib.h"
#include "raymath.h"

// ----- Utilities -----

FILE *DebugFile = NULL;
#define Debug(...)                       \
    {                                    \
        fprintf(DebugFile, __VA_ARGS__); \
        fputs("\n", DebugFile);          \
        fflush(DebugFile);               \
    }

inline int Compare(double a, double b)
{
    static const double epsilon = 1e-9;
    return a + epsilon < b ? -1 : (b + epsilon < a ? 1 : 0);
}

// Dynamic array macros
#define ListSize(da)               (da?((size_t*)da)[-1]:0)
#define ListCapacity(da)           (da?((size_t*)da)[-2]:0)
#define ListPop(da)                da[--(((size_t*)da)[-1])]

#define ListFree(da)                                                           \
        do {                                                                   \
            if(da) free((size_t*)da - 2);                                      \
            da = NULL;                                                         \
        } while(0)

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

/// ----- Geometry -----

typedef struct
{
    double x;
    double y;
} Point;

typedef struct
{
    Point c;
    double r;
} Circle;

inline Point Add(const Point a, const Point b)
{
    return (Point){.x = a.x + b.x, .y = a.y + b.y};
}

inline Point Subtract(const Point a, const Point b)
{
    return (Point){.x = a.x - b.x, .y = a.y - b.y};
}

inline Point Scale(const Point a, double s)
{
    return (Point){.x = a.x * s, .y = a.y * s};
}

inline double Distance(const Point a, const Point b)
{
    return hypot(a.x-b.x, a.y-b.y);
}

inline double Cross(const Point a, const Point b)
{
    return a.x * b.y - a.y * b.x;
}

inline double Cross3(const Point fr, const Point toA, const Point toB)
{
    return Cross(Subtract(toA, fr), Subtract(toB, fr));
}

inline int Sign(const double v)
{
    return (v > 0) - (v < 0);
}

// Return true if the line ab intersects segment cd in a point different
// than c or d.
bool LineCutsSegment(Point a, Point b, Point c, Point d)
{
    int signC = Sign(Cross3(a, b, c));
    int signD = Sign(Cross3(a, b, d));
    return signC + signD == 0 && signC != signD;
}

// Returns true if segments ab and cd intersect in one point different from
// their endpoints. If true, *outputPoint will be assigned the intersection.
bool TryFindSegmentInnerIntersection(
    Point a, Point b, Point c, Point d, Point* outPoint)
{
    if (!LineCutsSegment(a, b, c, d)) return false;
    if (!LineCutsSegment(c, d, a, b)) return false;

    Point da = Subtract(b, a);
    Point dc = Subtract(d, c);
    // a + da*t = c + dc*s // t and s are some scalars
    // (a + da*t) x dc = (c + dc*s) x dc // x is cross product
    // a x dc + (da x dc) * t = c x dc // distribute and (dc x dc) = 0
    // t = (c x dc - a x dc) / (da x dc) // isolate t
    // t = ((c - a) x dc) / (da x dc) // cross product is distributive
    double t = Cross(Subtract(c, a), dc) / Cross(da, dc);
    *outPoint = Add(a, Scale(da, t));
    return true;
}

// ----- Core state -----

typedef struct
{
    Point a;
    Point b;
} LineSegment;

struct
{
    Point* points;
    LineSegment* segments;
    Circle* circles;
} CoreState = {0};

size_t FindPoint(Point p)
{
    size_t nPoints = ListSize(CoreState.points);
    for (size_t i = 0; i < nPoints; i++) {
        if (Compare(CoreState.points[i].x, p.x) == 0 &&
            Compare(CoreState.points[i].y, p.y) == 0) {
            return (int)i;
        }
    }
    return SIZE_MAX;
}

size_t AddPoint(Point p)
{
    size_t index = FindPoint(p);
    if (index != SIZE_MAX) return (size_t)index;
    ListPush(CoreState.points, p);
    return ListSize(CoreState.points) - 1;
}

void AddSegment(LineSegment s)
{
    // TODO: Validate the segment don't already exists
    #define segmentList CoreState.segments
    for (size_t i = 0; i < ListSize(segmentList); i++) {
        Point p;
        bool foundIntersection = TryFindSegmentInnerIntersection(
            s.a, s.b, segmentList[i].a, segmentList[i].b, &p
        );
        if (foundIntersection) {
            Debug("Found segment intersection at (%f, %f)", p.x, p.y);
            AddPoint(p);
        }
    }
    ListPush(segmentList, s);
    #undef segmentList
    AddPoint(s.a);
    AddPoint(s.b);
}

void AddCircle(Circle c)
{
    // TODO: Validate the segment don't already exists
    AddPoint(c.c);
    Debug("Adding circle c=(%f,%f), r=%f\n", c.c.x, c.c.y, c.r);
    #define circleList CoreState.circles
    // TODO: Find intersections with existing figures
    ListPush(CoreState.circles, c);
    #undef circleList
}

void InitializeCoreState()
{
    AddSegment((LineSegment){
        (Point){.x= -5.0, .y=-8.0},
        (Point){.x= 8.0,  .y=13.0}
    });
    AddSegment((LineSegment){
        (Point){.x= 5.0,  .y=5.0 },
        (Point){.x= 5.0,  .y=10.0}
    });
    AddSegment((LineSegment){
        (Point){.x= -1.5,  .y=5.3},
        (Point){.x= 6.5,  .y=-4.7}
    });
    AddSegment((LineSegment){
        (Point){.x= -1,  .y=-1 },
        (Point){.x= -2,  .y=-2}
    });
    AddSegment((LineSegment){
        (Point){.x= -1,  .y=-2 },
        (Point){.x= -2,  .y=-1}
    });
}

void FreeCoreState()
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

inline float RectWidth(Rect r)
{
    return r.to.x - r.fr.x;
}

inline float RectHeight(Rect r)
{
    return r.to.y - r.fr.y;
}

struct
{
    int windowWidth;
    int windowHeight;
    Rect canvasRect;
} WindowState = {0};

typedef struct {
    int type;
    Point first;
    Point second;
    bool hasFirst;
} SegmentInProgress;

typedef struct {
    int type;
    Point center;
    Point boundaryPoint;
    bool hasCenter;
} CircleInProgress;

#define DRAWING_SEGMENT 0
#define DRAWING_CIRCLE 1

typedef union {
    int type;
    SegmentInProgress segment;
    CircleInProgress circle;
} DrawingInProgress;

struct
{
    // Rectangle of the cartesian plane being rendered
    double viewWidth;
    Point viewFr;
    // The focus point is the point close to the mouse to be used
    // when drawing shapes
    bool hasFocusedPoint;
    Point focusedPoint;
    DrawingInProgress inProgress;
} ViewState = {0};

inline double GetViewRectHeight() {
    const Rect canvas = WindowState.canvasRect;
    return ViewState.viewWidth / RectWidth(canvas) * RectHeight(canvas);
}

inline Point GetViewTo() {
    return Add(ViewState.viewFr,
            (Point){ViewState.viewWidth, GetViewRectHeight()});
}

struct
{
    int minFps;
    int maxFps;
    double avgFps;
    int64_t frameCount;
} Metrics = {0};

// ----- Mapping functions ------

// Get screen position given cartesian plane position
Vector2 ToVector2(Point p)
{
    const Point viewFr = ViewState.viewFr;
    const Point viewTo = GetViewTo();
    const Rect canvas = WindowState.canvasRect;
    Vector2 r = {
        .x = Normalize(p.x, viewFr.x, viewTo.x),
        .y = Normalize(p.y, viewTo.y, viewFr.y)
    };
    r.x = Lerp(canvas.fr.x, canvas.to.x, r.x);
    r.y = Lerp(canvas.fr.y, canvas.to.y, r.y);
    return r;
}

// Get cartesian plane position given screen position
Point ToPoint(Vector2 p)
{
    const Point viewFr = ViewState.viewFr;
    const Point viewTo = GetViewTo();
    const Rect canvas = WindowState.canvasRect;
    Point r = {
        .x = Normalize(p.x, canvas.fr.x, canvas.to.x),
        .y = Normalize(p.y, canvas.fr.y, canvas.to.y)
    };
    r.x = Lerp(viewFr.x, viewTo.x, r.x);
    r.y = Lerp(viewTo.y, viewFr.y, r.y);
    return r;
}

float ScreenToPlaneRatio()
{
    return RectWidth(WindowState.canvasRect) / ViewState.viewWidth;
}

// ----- Update state functions ------

void SetScreenSize(int width, int height)
{
    WindowState.windowWidth = width;
    WindowState.windowHeight = height;

    Rect* canvas = &WindowState.canvasRect;
    canvas->fr = (Vector2){0.0, 0.0};
    canvas->to = (Vector2){(float)width, (float)height};
}

void HandleSegmentDrawingInProgress(Point p) {
    Debug("Handling segment drawing!");
    SegmentInProgress* inProgress = &ViewState.inProgress.segment;
    if (!inProgress->hasFirst) {
        inProgress->first = p;
        inProgress->hasFirst = true;
    } else {
        inProgress->second = p;
        inProgress->hasFirst = false;
        AddSegment((LineSegment){
            inProgress->first,
            inProgress->second
        });
    }
}

void HandleCircleDrawingInProgress(Point p) {
    Debug("Handling segment drawing!");
    CircleInProgress* inProgress = &ViewState.inProgress.circle;
    if (!inProgress->hasCenter) {
        inProgress->center = p;
        inProgress->hasCenter = true;
    } else {
        inProgress->boundaryPoint = p;
        inProgress->hasCenter = false;
        Debug("Center = (%f,%f), BoundaryPoint = (%f,%f)",
            inProgress->center.x,
            inProgress->center.y,
            p.x,
            p.y);
        AddCircle((Circle){
            .c = inProgress->center,
            .r = Distance(inProgress->center, inProgress->boundaryPoint)
        });
    }
}

void HandleDrawingInProgress(Point p) {
    switch (ViewState.inProgress.type) {
        case DRAWING_SEGMENT:
            HandleSegmentDrawingInProgress(p);
            break;
        case DRAWING_CIRCLE:
            HandleCircleDrawingInProgress(p);
            break;
        default:
            assert(false);
    }
}

bool TryGetFocusedPoint(Point* outPoint)
{
    Point mpos = ToPoint(GetMousePosition());
    Point closestPoint;
    double closestDistance = INFINITY;

    double dx, dy, d;

    // check lattice point nearest to the mouse position
    Point latticePoint;
    latticePoint.x = round(mpos.x);
    latticePoint.y = round(mpos.y);

    dx = fabs((latticePoint.x - mpos.x) * ScreenToPlaneRatio())
            / WindowState.windowWidth;
    dy = fabs((latticePoint.y - mpos.y) * ScreenToPlaneRatio())
            / WindowState.windowHeight;
    d = hypotf(dx, dy);
    closestDistance = d;
    closestPoint = latticePoint;
    
    // check all points in core state
    size_t nPoints = ListSize(CoreState.points);
    for (size_t i = 0; i < nPoints; i++) {
        Point p = CoreState.points[i];
        dx = fabs((p.x - mpos.x) * ScreenToPlaneRatio())
                / WindowState.windowWidth;
        dy = fabs((p.y - mpos.y) * ScreenToPlaneRatio())
                / WindowState.windowHeight;
        d = hypot(dx, dy);
        if (d < closestDistance) {
            closestPoint = p;
            closestDistance = d;
        }
    }

    if (closestDistance > 0.008) {
        return false;
    }
    *outPoint = closestPoint;
    return true;
}


// ----- View render funcitions -----

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
    float spacing = 0;
    Point viewFr = ViewState.viewFr;
    Point viewTo = GetViewTo();
    bool isXAxisVisible = viewFr.y <= 0.0 && viewTo.y >= 0.0;
    bool isYAxisVisible = viewFr.x <= 0.0 && viewTo.x >= 0.0;
    if (isXAxisVisible) {
        spacing = FindAxisTickSpacing(ViewState.viewWidth);
    }
    if (isYAxisVisible) {
        float spacingY = FindAxisTickSpacing(GetViewRectHeight());
        if (spacing < spacingY) spacing = spacingY;
    }

    // none of the axis are visible
    if (Compare(spacing,0.0) == 0) return;

    float tickSize = spacing * ScreenToPlaneRatio() * TICK_SIZE_FACTOR;
    int textHeight = (int)tickSize * 2;

    if (isXAxisVisible) { // Draw X axis
        Vector2 a = ToVector2((Point){viewFr.x, 0.0});
        Vector2 b = ToVector2((Point){viewTo.x, 0.0});
        DrawLineV(a, b, axisColor);
        float firstTick = ceilf(viewFr.x / spacing) * spacing;
        for (float tick = firstTick; tick <= viewTo.x; tick += spacing) {
            if (Compare(tick, 0.0f) == 0) continue;
            // Draw the tick
            Vector2 tickPos = ToVector2((Point){tick, 0.0});
            DrawLine(
                tickPos.x, tickPos.y - tickSize,
                tickPos.x, tickPos.y + tickSize,
                axisColor);
            // Draw the tick number
            snprintf(buffer, sizeof(buffer), "%.0f", tick);
            int textWidth = MeasureText(buffer, textHeight);
            DrawText(
                buffer,
                tickPos.x - 0.5f * textWidth,
                tickPos.y - textHeight - tickSize - 1,
                textHeight,
                axisNumberColor);
        }
    }

    if (isYAxisVisible) { // Draw Y axis
        Vector2 a = ToVector2((Point){0.0, viewFr.y});
        Vector2 b = ToVector2((Point){0.0, viewTo.y});
        DrawLineV(a, b, axisColor);
        float firstTick = ceilf(viewFr.y / spacing) * spacing;
        for (float tick = firstTick; tick <= viewTo.y; tick += spacing) {
            if (Compare(tick, 0.0f) == 0) continue;
            // Draw the tick
            Vector2 tickPos = ToVector2((Point){0.0, tick});
            DrawLine(
                tickPos.x - tickSize, tickPos.y,
                tickPos.x + tickSize, tickPos.y,
                axisColor);
            // Draw the tick number
            snprintf(buffer, sizeof(buffer), "%.0f", tick);
            DrawText(
                buffer,
                tickPos.x + tickSize + 1,
                tickPos.y - textHeight / 2.0f,
                textHeight,
                axisNumberColor);
        }
    }
}

void RenderFocusedPoint()
{
    if (ViewState.hasFocusedPoint)
    {
        Vector2 screenPos = ToVector2(ViewState.focusedPoint);
        DrawCircleV(screenPos, 5.0f, BLUE);
    }
}

void RenderSegmentInProgress()
{
    const SegmentInProgress inProgress = ViewState.inProgress.segment;
    if (inProgress.hasFirst) {
        DrawLineV(ToVector2(inProgress.first), GetMousePosition(), BLUE);
    }
}

void RenderCircleInProgress() {
    const CircleInProgress inProgress = ViewState.inProgress.circle;
    if (inProgress.hasCenter) {
        Vector2 c = ToVector2(inProgress.center);
        double r = Vector2Distance(c, GetMousePosition());
        DrawCircleLinesV(c, r, BLUE);
    }
}

void RenderDrawingInProgress()
{
    switch (ViewState.inProgress.type) {
        case DRAWING_SEGMENT:
            RenderSegmentInProgress();
            break;
        case DRAWING_CIRCLE:
            RenderCircleInProgress();
            break;
        default:
            assert(false);
    }
    RenderFocusedPoint();
}

void RenderCoreState()
{
    size_t nSegments = ListSize(CoreState.segments);
    for (size_t i = 0; i < nSegments; i++)
    {
        LineSegment seg = CoreState.segments[i];
        DrawLineV(ToVector2(seg.a), ToVector2(seg.b), RED);
    }
    size_t nCircles = ListSize(CoreState.circles);
    for (size_t i = 0; i < nCircles; i++)
    {
        Circle o = CoreState.circles[i];
        // point in boundary of the circle
        Point bPoint = (Point){.x = o.c.x, .y = o.c.y+o.r};
        // center point
        Vector2 c = ToVector2(o.c);
        // radius in pixels
        float r = Vector2Distance(c, ToVector2(bPoint));
        DrawCircleLinesV(c, r, RED);
    }
}

// ----- Main functions -----

void Initialize()
{
    DebugFile = fopen("Debug.log", "w");
    Debug("Initializing..");

    ViewState.viewFr = (Point){-10.0, -10.0};
    ViewState.viewWidth = 30;
    SetScreenSize(800, 800);

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    const char* title = "Geometry fun!";
    InitWindow(WindowState.windowWidth, WindowState.windowHeight, title);
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
    FreeCoreState();
    Debug("Done cleaning up..");
    fclose(DebugFile);
}

void Update()
{
    SetScreenSize(GetScreenWidth(), GetScreenHeight());

    ViewState.hasFocusedPoint = TryGetFocusedPoint(&ViewState.focusedPoint);

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && ViewState.hasFocusedPoint) {
        HandleDrawingInProgress(ViewState.focusedPoint);
    }
    // if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {}
    if (IsKeyPressed(KEY_S)) {
        Debug("Go to segment drawing!");
        ViewState.inProgress.type = DRAWING_SEGMENT;
        ViewState.inProgress.segment.hasFirst = false;
    } else if (IsKeyPressed(KEY_C)) {
        Debug("Go to circle drawing!");
        ViewState.inProgress.type = DRAWING_CIRCLE;
        ViewState.inProgress.circle.hasCenter = false;
    }
}

void Draw()
{
    BeginDrawing();
    ClearBackground(RAYWHITE);
    RenderCartesianPlaneAxes();
    RenderCoreState();
    RenderDrawingInProgress();
    DrawFPS(5, 5);
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

// ----- Main -----

int main(void)
{
    Initialize();

    while (!WindowShouldClose()) {
        Draw();
        Update();
        GatherMetrics();
    }

    CleanUp();
    return 0;
}