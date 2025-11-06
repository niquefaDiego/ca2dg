$RAYLIB_PATH="C:\raylib\raylib"
$COMPILER_PATH="C:\raylib\w64devkit\bin"

$CC="${COMPILER_PATH}\gcc"
$CFLAGS="${RAYLIB_PATH}\src\raylib.rc.data -s -static -O2 -std=c99 -Wall -Wextra -Wpedantic  -I${RAYLIB_PATH}\src -Iexternal -DPLATFORM_DESKTOP"
$LDFLAGS="-lraylib -lopengl32 -lgdi32 -lwinmm"

New-Item -ItemType Directory -Force -Path ".\build"
$outputBinary = ".\build\main.exe"


if (Test-Path $outputBinary) {
  Remove-Item $outputBinary -Force
}

Invoke-Expression "${CC} -o ${outputBinary} src\main.c ${CFLAGS} ${LDFLAGS}"

if (Test-Path $outputBinary) {
  Start-Process $outputBinary
}
