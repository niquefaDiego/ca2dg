#include <stdio.h>
#include <limits.h>
#include <stdint.h>
#include <stdbool.h>
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

inline bool SamePoint(const Point a, const Point b)
{
    return Compare(a.x, b.x) == 0 && Compare(a.y, b.y) == 0;
}

inline Point Add(const Point a, const Point b)
{
    return (Point){.x = a.x + b.x, .y = a.y + b.y};
}

inline Point Add3(const Point a, const Point b, const Point c)
{
    return (Point){.x = a.x + b.x + c.x, .y = a.y + b.y + c.y};
}

inline Point Subtract(const Point a, const Point b)
{
    return (Point){.x = a.x - b.x, .y = a.y - b.y};
}

inline Point Scale(const Point a, double s)
{
    return (Point){.x = a.x * s, .y = a.y * s};
}

inline double Magnitude(const Point a)
{
    return hypot(a.x, a.y);
}

inline double Distance(const Point a, const Point b)
{
    return hypot(a.x-b.x, a.y-b.y);
}

inline double DistanceSqr(const Point a, const Point b)
{
    return (a.x - b.x)*(a.x - b.x) + (a.y - b.y)*(a.y - b.y);
}

inline Point Unit(const Point a)
{
    return Scale(a, 1.0 / Magnitude(a));
}

inline Point RotCCW90(const Point a) {
    return (Point){.x = -a.y, .y = a.x};
}

inline double Dot(const Point a, const Point b)
{
    return a.x * b.x + a.y * b.y;
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
bool SegmentIntersect(Point a, Point b, Point c, Point d, Point* outPoint)
{
    assert(!SamePoint(a, b) && !SamePoint(c, d));
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

// Returns the number of intersections between the segment ab and the
// circle with center c and radius r.
// a and b don't count as intersectin points.
size_t CircleSegmentIntersect(Point c, double r, Point a, Point b, Point out[2])
{
    assert(!SamePoint(a, b));
    Debug(
        "check circle-segment inteersect c=(%f,%f), r=%f, a=(%f,%f), b=(%f,%f)",
        c.x, c.y, r, a.x, a.y, b.x, b.y);
    const Point ab = Subtract(b, a);
    const Point ac = Subtract(c, a);
    const double acDist = Magnitude(ac);
    const double abDist = Magnitude(ab);
    if (Compare(acDist, r) < 0 && Compare(abDist, r) < 0) {
        Debug("Segment is fully inside the circle");
        return 0; // segment is fully inside the circle
    }
    const Point abUnit = Unit(ab);
    Debug("acDist = %f", acDist);
    const Point p = Add(a, Scale(abUnit, Dot(abUnit,ac)));
    Debug("p = (%f,%f)", p.x, p.y);
    const double cpDistanceSqr = DistanceSqr(c, p);
    const double rSquared = r*r;
    const int cmp = Compare(cpDistanceSqr, rSquared);
    if (cmp > 0) {
        Debug("Line does not touch the circle");
        return 0; // line ab does not touch the circle
    }
    if (cmp == 0) { // line ab touches the circle in one point
        if (Sign(Dot(Subtract(a, p), Subtract(b, p))) == -1) {
            Debug("intersection: (%f,%f)", p.x, p.y);
            out[0] = p;
            return 1;
        }
        Debug("Line touches the circle in one point outside the segment");
        return 0;
    }
    const double d = sqrt(rSquared - cpDistanceSqr);
    const Point p0 = Add(p, Scale(abUnit, d));
    const Point p1 = Add(p, Scale(abUnit, -d));
    Debug("p0 = (%f,%f)\np1=(%f,%f)",p0.x, p0.y, p1.x, p1.y);
    int outLen = 0;
    if (Sign(Dot(Subtract(a, p0), Subtract(b, p0))) == -1) out[outLen++] = p0;
    if (Sign(Dot(Subtract(a, p1), Subtract(b, p1))) == -1) out[outLen++] = p1;
    Debug("Line touches the circle in two points outside the segment");
    return outLen;
}


// If the segments intersects in either exactly 1 or exactly 2 points, then
// this function populates those intersection points in out. Returns the
// number of populated points.
size_t CircleIntersect(Point a, double r1, Point b, double r2, Point out[2])
{
    assert(Compare(r1, 0) > 0 && Compare(r2, 0) > 0);

    if (SamePoint(a, b))
        return 0; // circle are either equal or one completely inside the other

    double d = Distance(a, b);
    if (Compare(d, r1+r2) > 0)
        return 0; // circles too far appart, no intersection
    if (Compare(d, r1+r2) == 0) {
        out[0] = Add(a, Scale(Subtract(b, a), r1 / (r1+r2)));
        return 1; // circles are tangent, touching on exactly 1 point
    }

    // One circle is completely inside the other, not touching at all
    if (Compare(d+r1, r2) < 0) return 0;
    if (Compare(d+r2, r1) < 0) return 0;

    if (Compare(d+r1, r2) == 0) {
        out[0] = Add(a, Scale(Unit(Subtract(a, b)), r1));
        return 1; // circle 1 is inside circle 2, they touch in eactly 1 point
    }
    if (Compare(d+r2, r1) == 0) {
        out[0] = Add(b, Scale(Unit(Subtract(b, a)), r2));
        return 1; // circle 2 is inside circle 1, they touch in eactly 1 point
    }
    
    // circle touch in 2 points
    const double x = (r1 * r1 + d * d - r2 * r2) / 2.0 / d;
    const double y = sqrt(r1*r1 - x*x);
    const Point vx = Unit(Subtract(b, a));
    const Point vy = RotCCW90(vx);
    out[0] = Add3(a, Scale(vx, x), Scale(vy, y));
    out[1] = Add3(a, Scale(vx, x), Scale(vy, -y));
    return 2;
}

// ----- Core state -----

typedef struct
{
    Point a;
    Point b;
} LineSegment;

typedef struct
{
    Point c;
    double r;
} Circle;

struct
{
    Point* points;
    LineSegment* segments;
    Circle* circles;
} Core = {0};

size_t FindPoint(Point p)
{
    size_t nPoints = ListSize(Core.points);
    for (size_t i = 0; i < nPoints; i++)
        if (SamePoint(Core.points[i], p))
            return i;
    return SIZE_MAX;
}

size_t AddPoint(Point p)
{
    size_t index = FindPoint(p);
    if (index != SIZE_MAX) return (size_t)index;
    ListPush(Core.points, p);
    return ListSize(Core.points) - 1;
}

void AddSegment(LineSegment s)
{
    if (SamePoint(s.a, s.b)) {
        Debug("Not adding segment with length 0"); // TODO: show toast
        return;
    }
    AddPoint(s.a);
    AddPoint(s.b);
    #define circs Core.circles
    #define segs Core.segments
    for (size_t i = 0; i < ListSize(segs); i++)
        if (SamePoint(s.a, segs[i].a) && SamePoint(s.b, segs[i].b)) {
            Debug("Not adding duplicated segment"); // TODO: show toast
            return;
        }
    for (size_t i = 0; i < ListSize(segs); i++) {
        Point p;
        bool foundIntersection = SegmentIntersect(
            s.a, s.b, segs[i].a, segs[i].b, &p
        );
        if (foundIntersection) {
            Debug("segment-segment at (%f, %f)", p.x, p.y);
            AddPoint(p);
        }
    }
    for (size_t i = 0; i < ListSize(circs); i++) {
        Point out[2];
        size_t len;
        len = CircleSegmentIntersect(circs[i].c, circs[i].r, s.a, s.b, out);
        if (len == 0) continue;
        for (size_t j = 0; j < len; j++) {
            Debug("circle-segment itersection at: (%f,%f)", out[j].x, out[j].y);
            AddPoint(out[j]);
        }
    }
    ListPush(segs, s);
    #undef segs
    #undef circs
}

void AddCircle(Circle c)
{
    if (Compare(c.r, 0) == 0) {
        Debug("Not circle with radius 0"); // TODO: show toast
        return;
    }
    Debug("Adding circle c=(%f,%f), r=%f", c.c.x, c.c.y, c.r);
    #define circs Core.circles
    #define segs Core.segments
    
    for (size_t i = 0; i < ListSize(circs); i++) {
        if (SamePoint(circs[i].c, c.c) && Compare(circs[i].r, c.r) == 0) {
            Debug("Not adding duplicated circle!"); // TODO: show toast
            Debug("Same as existing circle %zu. c=(%f,%f), r=%f",
                i, circs[i].c.x, circs[i].c.y, circs[i].r);
            return;
        }
    }
    
    AddPoint(c.c);
    Point out[2];
    size_t outLen;
    for (size_t i = 0; i < ListSize(segs); i++) {
        outLen = CircleSegmentIntersect(c.c, c.r, segs[i].a, segs[i].b, out);
        if (!outLen) continue;
        for (size_t j = 0; j < outLen; j++) {
            Debug("circle-segment itersection at: (%f,%f)", out[j].x, out[j].y);
            AddPoint(out[j]);
        }
    }
    for (size_t i = 0; i < ListSize(circs); i++) {
        outLen = CircleIntersect(c.c, c.r, circs[i].c, circs[i].r, out);
        if (!outLen) continue;
        for (size_t j = 0; j < outLen; j++) {
            Debug("circle-circle itersection at: (%f,%f)", out[j].x, out[j].y);
            AddPoint(out[j]);
        }
    }
    ListPush(circs, c);
    #undef segs
    #undef circs
}

void InitializeCore()
{
}

void FreeCore()
{
    free(Core.points);
    free(Core.segments);
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
} Window = {0};

typedef struct {
    int type;
    Point first;
    Point second;
    bool hasFirst;
} DrawSegmentAction;

typedef struct {
    int type;
    Point center;
    Point boundaryPoint;
    bool hasCenter;
} DrawCircleAction;

#define ACTION_DRAW_SEGMENT 0
#define ACTION_DRAW_CIRCLE 1

typedef union {
    int type;
    DrawSegmentAction segment;
    DrawCircleAction circle;
} Action;

struct
{
    // Rectangle of the cartesian plane being rendered
    double viewWidth;
    Point viewFr;
    // The focus point is the point close to the mouse to be used
    // when drawing shapes
    bool hasFocusedPoint;
    Point focusedPoint;
    // Action in progress
    Action action;
} View = {0};

inline double ViewHeight() {
    const Rect canvas = Window.canvasRect;
    return View.viewWidth / RectWidth(canvas) * RectHeight(canvas);
}

inline Point ViewTo() {
    return Add(View.viewFr, (Point){View.viewWidth, ViewHeight()});
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
    const Point viewFr = View.viewFr;
    const Point viewTo = ViewTo();
    const Rect canvas = Window.canvasRect;
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
    const Point viewFr = View.viewFr;
    const Point viewTo = ViewTo();
    const Rect canvas = Window.canvasRect;
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
    return RectWidth(Window.canvasRect) / View.viewWidth;
}

// ----- Update state functions ------

void SetScreenSize(int width, int height)
{
    Window.windowWidth = width;
    Window.windowHeight = height;

    Rect* canvas = &Window.canvasRect;
    canvas->fr = (Vector2){0.0, 0.0};
    canvas->to = (Vector2){(float)width, (float)height};
}

void HandleDrawSegmentAction(Point p) {
    Debug("Handling segment drawing!");
    DrawSegmentAction* action = &View.action.segment;
    if (!action->hasFirst) {
        action->first = p;
        action->hasFirst = true;
    } else {
        action->second = p;
        action->hasFirst = false;
        AddSegment((LineSegment){
            action->first,
            action->second
        });
    }
}

void HandleDrawCircleAction(Point p) {
    Debug("Handling circle drawing!");
    DrawCircleAction* action = &View.action.circle;
    if (!action->hasCenter) {
        action->center = p;
        action->hasCenter = true;
    } else {
        action->boundaryPoint = p;
        action->hasCenter = false;
        Debug("Center = (%f,%f), BoundaryPoint = (%f,%f)",
            action->center.x,
            action->center.y,
            p.x,
            p.y);
        AddCircle((Circle){
            .c = action->center,
            .r = Distance(action->center, action->boundaryPoint)
        });
    }
}

void HandleAction(Point p) {
    switch (View.action.type) {
        case ACTION_DRAW_SEGMENT:
            HandleDrawSegmentAction(p);
            break;
        case ACTION_DRAW_CIRCLE:
            HandleDrawCircleAction(p);
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
            / Window.windowWidth;
    dy = fabs((latticePoint.y - mpos.y) * ScreenToPlaneRatio())
            / Window.windowHeight;
    d = hypotf(dx, dy);
    closestDistance = d;
    closestPoint = latticePoint;

    // check all points in core state
    size_t nPoints = ListSize(Core.points);
    for (size_t i = 0; i < nPoints; i++) {
        Point p = Core.points[i];
        dx = fabs((p.x - mpos.x) * ScreenToPlaneRatio())
                / Window.windowWidth;
        dy = fabs((p.y - mpos.y) * ScreenToPlaneRatio())
                / Window.windowHeight;
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
    Point viewFr = View.viewFr;
    Point viewTo = ViewTo();
    bool isXAxisVisible = viewFr.y <= 0.0 && viewTo.y >= 0.0;
    bool isYAxisVisible = viewFr.x <= 0.0 && viewTo.x >= 0.0;
    if (isXAxisVisible) {
        spacing = FindAxisTickSpacing(View.viewWidth);
    }
    if (isYAxisVisible) {
        float spacingY = FindAxisTickSpacing(ViewHeight());
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
    if (View.hasFocusedPoint)
    {
        Vector2 screenPos = ToVector2(View.focusedPoint);
        DrawCircleV(screenPos, 5.0f, BLUE);
    }
}

void RenderDrawSegmentAction()
{
    const DrawSegmentAction action = View.action.segment;
    if (action.hasFirst) {
        DrawLineV(ToVector2(action.first), GetMousePosition(), BLUE);
    }
}

void RenderDrawCircleAction() {
    const DrawCircleAction action = View.action.circle;
    if (action.hasCenter) {
        Vector2 c = ToVector2(action.center);
        double r = Vector2Distance(c, GetMousePosition());
        DrawCircleLinesV(c, r, BLUE);
    }
}

void RenderAction()
{
    switch (View.action.type) {
        case ACTION_DRAW_SEGMENT:
            RenderDrawSegmentAction();
            break;
        case ACTION_DRAW_CIRCLE:
            RenderDrawCircleAction();
            break;
        default:
            assert(false);
    }
    RenderFocusedPoint();
}

void RenderCore()
{
    size_t nSegments = ListSize(Core.segments);
    for (size_t i = 0; i < nSegments; i++)
    {
        LineSegment seg = Core.segments[i];
        DrawLineV(ToVector2(seg.a), ToVector2(seg.b), RED);
    }
    size_t nCircles = ListSize(Core.circles);
    for (size_t i = 0; i < nCircles; i++)
    {
        Circle o = Core.circles[i];
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
#define LOG_FILE "Debug.log"
    errno_t err = fopen_s(&DebugFile, LOG_FILE, "w");
    Debug("Initializing..");

    if (err || DebugFile == NULL) {
        perror("Failed to open " LOG_FILE " file for writing");
        DebugFile = NULL;
    }
#undef LOG_FILE

    View.viewFr = (Point){-10.0, -10.0};
    View.viewWidth = 30;
    SetScreenSize(800, 800);

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    const char* title = "Geometry fun!";
    InitWindow(Window.windowWidth, Window.windowHeight, title);
    SetWindowMinSize(400, 400);

    SetTargetFPS(60);
    Metrics.minFps = INT_MAX;

    InitializeCore();
    Debug("Done initializing..");
}

void CleanUp()
{
    Debug("Cleaning up..");
    Debug("Min FPS: %d", Metrics.minFps);
    Debug("Max FPS: %d", Metrics.maxFps);
    Debug("Avg FPS: %lf", Metrics.avgFps);
    CloseWindow();
    FreeCore();
    Debug("Done cleaning up..");
    fclose(DebugFile);
}

void HandleViewMovementInputs()
{
    Point viewDisplacement = {0};
    if (IsKeyDown(KEY_W)) viewDisplacement.y += 1.0;
    if (IsKeyDown(KEY_S)) viewDisplacement.y -= 1.0;
    if (IsKeyDown(KEY_A)) viewDisplacement.x -= 1.0;
    if (IsKeyDown(KEY_D)) viewDisplacement.x += 1.0;

    const int MIN_VIEW_X = -1000, MAX_VIEW_X = 1000;
    const int MIN_VIEW_Y = -1000, MAX_VIEW_Y = 1000;

    // Translate view
    if (!SamePoint(viewDisplacement, (Point){.x=0, .y=0})) {
        const double secsToMove1Width = 2.0;
        double speed = View.viewWidth * (GetFrameTime()/secsToMove1Width);
        viewDisplacement = Scale(viewDisplacement, speed);
        View.viewFr = Add(View.viewFr, viewDisplacement);
        if (View.viewFr.x + View.viewWidth > MAX_VIEW_X)
            View.viewFr.x = MAX_VIEW_X - View.viewWidth;
        if (View.viewFr.y + ViewHeight() > MAX_VIEW_Y)
            View.viewFr.y = MAX_VIEW_Y - ViewHeight();
        if (View.viewFr.x < MIN_VIEW_X) View.viewFr.x = MIN_VIEW_X;
        if (View.viewFr.y < MIN_VIEW_Y) View.viewFr.y = MIN_VIEW_Y;
    }

    // Zoom view
    const double zoomPerTick = 0.1;
    if (fabsf(GetMouseWheelMove()) > EPSILON) {
        View.viewWidth *= (1 - GetMouseWheelMove() * zoomPerTick);
        if (View.viewFr.x + View.viewWidth > MAX_VIEW_X)
            View.viewWidth = MAX_VIEW_X - View.viewFr.x;
        if (View.viewFr.y + ViewHeight() > MAX_VIEW_Y) {
            double viewH = MAX_VIEW_Y - View.viewFr.y;
            const Rect canvas = Window.canvasRect;
            View.viewWidth = viewH / RectHeight(canvas) * RectWidth(canvas);
        }
    }
}


void Update()
{
    SetScreenSize(GetScreenWidth(), GetScreenHeight());

    View.hasFocusedPoint = TryGetFocusedPoint(&View.focusedPoint);

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && View.hasFocusedPoint) {
        HandleAction(View.focusedPoint);
    }

    if (IsKeyPressed(KEY_Q)) {
        assert(IsKeyPressed(KEY_Q));
        Debug("Go to segment drawing!");
        View.action.type = ACTION_DRAW_SEGMENT;
        View.action.segment.hasFirst = false;
    } else if (IsKeyPressed(KEY_C)) {
        Debug("Go to circle drawing!");
        View.action.type = ACTION_DRAW_CIRCLE;
        View.action.circle.hasCenter = false;
    }

    HandleViewMovementInputs();
}

void Draw()
{
    BeginDrawing();
    ClearBackground(RAYWHITE);
    RenderCartesianPlaneAxes();
    RenderCore();
    RenderAction();
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
