#include <stdio.h>
#include <limits.h>
#include <stdint.h>
#include "raylib.h"
#include "raymath.h"

#define debug(...) { fprintf(debugFile,__VA_ARGS__); fputs("\n",debugFile); fflush(debugFile); }

FILE* debugFile = NULL;

typedef struct
{
  Vector2 fr; // a.x <= b.x and a.y <= b.y
  Vector2 to;
} Rect;

struct
{
  int windowWidgth;
  int windowHeight;
  Rect canvasRect;
} windowState;

struct CoreState {

};

struct {
  // Rectangle of the cartesian plane being rendered
  Rect viewPlaneRect;
} viewState;

struct {
  int minFps;
  int maxFps;
  double avgFps;
  int64_t frameCount;
} metrics;

void initialize()
{
  debugFile = fopen("debug.log", "w");
  debug("Initializing..");

  int windowWidgth = 800;
  int windowHeight = 450;
  windowState.windowWidgth = windowWidgth;
  windowState.windowHeight = windowHeight;
  windowState.canvasRect.fr = (Vector2){0.0, 0.0};
  windowState.canvasRect.to = (Vector2){(float)windowState.windowWidgth, (float)windowState.windowHeight};

  viewState.viewPlaneRect.fr = (Vector2){-100.0, -100.0};
  viewState.viewPlaneRect.to = (Vector2){300.0, 300.0};

  InitWindow(windowWidgth, windowHeight, "Computer Assisted 2D Geometry");

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
  // update your variables here
}

Vector2 getScreenVec2(Vector2 p)
{
  p.x = Normalize(p.x, viewState.viewPlaneRect.fr.x,  viewState.viewPlaneRect.to.x);
  p.y = Normalize(p.y, viewState.viewPlaneRect.to.y,  viewState.viewPlaneRect.fr.y);
  p.x = Lerp(windowState.canvasRect.fr.x, windowState.canvasRect.to.x, p.x);
  p.y = Lerp(windowState.canvasRect.fr.y, windowState.canvasRect.to.y, p.y);
  return p;
}

void drawCartesianAxes()
{
  Color axisColor = DARKGRAY;

  // Draw X axis
  if (viewState.viewPlaneRect.fr.y <= 0.0 && viewState.viewPlaneRect.to.y >= 0.0)
  {
    Vector2 a = getScreenVec2((Vector2){viewState.viewPlaneRect.fr.x, 0.0});
    Vector2 b = getScreenVec2((Vector2){viewState.viewPlaneRect.to.x, 0.0});
    DrawLineV(a, b, axisColor);
  }

  // Draw Y axis
  if (viewState.viewPlaneRect.fr.x <= 0.0 && viewState.viewPlaneRect.to.x >= 0.0)
  {
    Vector2 a = getScreenVec2((Vector2){0.0, viewState.viewPlaneRect.fr.y});
    Vector2 b = getScreenVec2((Vector2){0.0, viewState.viewPlaneRect.to.y});
    DrawLineV(a, b, axisColor);
  }
}

void draw()
{
  Vector2 a = {-50.0, -50.0};
  Vector2 b = {100.0, 250.0};

  a = getScreenVec2(a);
  b = getScreenVec2(b);

  BeginDrawing();
    ClearBackground(RAYWHITE);
    drawCartesianAxes();
    DrawLineV(a, b, RED);
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