#include <stdio.h>
#include "raylib.h"

int main(void)
{
  const int screenWidth = 800;
  const int screenHeight = 450;

  InitWindow(screenWidth, screenHeight, "Title goes here!");

  SetTargetFPS(60);

  int counter = 0;
  while (!WindowShouldClose())    // Detect window close button or ESC key
  {
    counter++;
    // Update
    if (counter % 60 == 0) {
      printf ("FPS: %i\n", GetFPS());
    }

    // Draw
    BeginDrawing();
      ClearBackground(RAYWHITE);
      DrawText("Hello there!", 190, 200, 20, LIGHTGRAY);
    EndDrawing();
  }

  CloseWindow();        // Close window and OpenGL context

  return 0;
}