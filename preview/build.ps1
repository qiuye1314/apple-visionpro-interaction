param([string]$Zig = '')
$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path -Parent $PSScriptRoot
$taskProject = Join-Path $taskRoot 'app'
if (Test-Path -LiteralPath (Join-Path $taskRoot 'app-draw-clean')) { $taskProject = Join-Path $taskRoot 'app-draw-clean' }
if (Test-Path -LiteralPath (Join-Path $taskRoot 'app-draw-final')) { $taskProject = Join-Path $taskRoot 'app-draw-final' }
if (-not $Zig) {
    $Zig = (Get-Command zig.exe -ErrorAction SilentlyContinue).Source
    if (-not $Zig) { $Zig = 'D:\工作区\israel-spatial-ui\.tools\ziglang\zig.exe' }
}
if (-not (Test-Path -LiteralPath $Zig)) { throw 'Pass -Zig with the full path to zig.exe (tested with 0.13.0).' }
$taskArgs = @('c++','-std=c++17','-O1','-g','-UNDEBUG','-DIMGUI_DISABLE_WIN32_DEFAULT_IME_FUNCTIONS',
    '-I', (Join-Path $taskProject 'jni\include\ImGui'), '-I', (Join-Path $taskProject 'jni\src'),
    (Join-Path $PSScriptRoot 'preview.cpp'),
    (Join-Path $taskProject 'jni\src\ImGui\imgui.cpp'),
    (Join-Path $taskProject 'jni\src\ImGui\imgui_draw.cpp'),
    (Join-Path $taskProject 'jni\src\ImGui\imgui_widgets.cpp'),
    (Join-Path $taskProject 'jni\src\ImGui\imgui_tables.cpp'),
    '-lopengl32','-lgdi32','-luser32','-o', (Join-Path $PSScriptRoot 'SpatialPreview.exe'))
& $Zig @taskArgs
if ($LASTEXITCODE -ne 0) { throw 'UI preview build failed.' }
