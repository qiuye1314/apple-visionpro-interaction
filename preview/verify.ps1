param([string]$Zig = '')
$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path -Parent $PSScriptRoot
$taskProject = Join-Path $taskRoot 'app'
if (Test-Path -LiteralPath (Join-Path $taskRoot 'app-draw-clean')) { $taskProject = Join-Path $taskRoot 'app-draw-clean' }
if (Test-Path -LiteralPath (Join-Path $taskRoot 'app-draw-final')) { $taskProject = Join-Path $taskRoot 'app-draw-final' }
$taskSrc = Join-Path $taskProject 'jni\src'
$taskImgui = Join-Path $taskProject 'jni\include\ImGui'
if (-not $Zig) {
    $Zig = (Get-Command zig.exe -ErrorAction SilentlyContinue).Source
    if (-not $Zig) { $Zig = 'D:\工作区\israel-spatial-ui\.tools\ziglang\zig.exe' }
}
if (-not (Test-Path -LiteralPath $Zig)) { throw 'Pass -Zig with the path to Zig 0.13.0.' }

$taskInitialProjectHashes = [ordered]@{}
Get-ChildItem -LiteralPath (Join-Path $taskProject 'jni') -File -Recurse | Sort-Object FullName | ForEach-Object {
    $taskKey = $_.FullName.Substring($taskProject.Length + 1).Replace('\','/')
    $taskInitialProjectHashes[$taskKey] = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
}
$taskInitialPreviewHashes = [ordered]@{}
Get-ChildItem -LiteralPath $PSScriptRoot -File | Where-Object {
    $_.Extension -in @('.cpp','.h','.hpp','.inl','.ps1') -or $_.Name -in @('prepare_draw_only.py','export_pngs.py')
} | Sort-Object Name | ForEach-Object {
    $taskInitialPreviewHashes[$_.Name] = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
}

Push-Location $PSScriptRoot
try {
    & (Join-Path $PSScriptRoot 'build.ps1') -Zig $Zig
    foreach ($taskTest in @('motion','quotes','draw_only_visuals','backdrop','backdrop_images','liquid','bone_reader','projection','driver','console','mortar','mortar_visuals','memory_bridge','actor_read','frame_stability','physx_concurrency','physx_throttle','touch_recovery','scene_bounds')) {
        $taskExe = switch ($taskTest) {
            'motion' { 'VerifyMotion.exe' }
            'quotes' { 'VerifyQuotes.exe' }
            'backdrop' { 'VerifyBackdrop.exe' }
            'backdrop_images' { 'VerifyBackdropImages.exe' }
            'liquid' { 'VerifyLiquid.exe' }
            'bone_reader' { 'VerifyBoneReader.exe' }
            'projection' { 'VerifyProjection.exe' }
            'driver' { 'VerifyDriver.exe' }
            'console' { 'VerifyConsole.exe' }
            'mortar' { 'VerifyMortar.exe' }
            'mortar_visuals' { 'VerifyMortarVisuals.exe' }
            'memory_bridge' { 'VerifyMemoryBridge.exe' }
            'actor_read' { 'VerifyActorRead.exe' }
            'frame_stability' { 'VerifyFrameStability.exe' }
            'physx_concurrency' { 'VerifyPhysXConcurrency.exe' }
            'physx_throttle' { 'VerifyPhysXThrottle.exe' }
            'touch_recovery' { 'VerifyTouchRecovery.exe' }
            'scene_bounds' { 'VerifySceneBounds.exe' }
            default { 'VerifyDrawOnlyVisuals.exe' }
        }
        $taskArgs = @('c++','-std=c++17','-O2','-ffast-math','-UNDEBUG','-I',$taskSrc,'-I',$taskImgui,
            (Join-Path $PSScriptRoot ('verify_' + $taskTest + '.cpp')))
        if ($taskTest -eq 'physx_concurrency') { $taskArgs += @('-I',(Join-Path $taskProject 'jni\include')) }
        if ($taskTest -in @('draw_only_visuals','backdrop','liquid','mortar_visuals')) {
            foreach ($taskFile in @('imgui.cpp','imgui_draw.cpp','imgui_tables.cpp','imgui_widgets.cpp')) {
                $taskArgs += Join-Path $taskSrc ('ImGui\' + $taskFile)
            }
        }
        $taskArgs += @('-o',(Join-Path $PSScriptRoot $taskExe))
        & $Zig @taskArgs
        if ($LASTEXITCODE -ne 0) { throw ('Failed to compile ' + $taskTest) }
    }
    $taskChecks = @(
        @{ Name='ui'; Exe='SpatialPreview.exe'; Args=@('--test') },
        @{ Name='gyro-ui'; Exe='SpatialPreview.exe'; Args=@('--gyro-test') },
        @{ Name='quotes-ui'; Exe='SpatialPreview.exe'; Args=@('--quotes-test') },
        @{ Name='scene'; Exe='SpatialPreview.exe'; Args=@('--scene') },
        @{ Name='visuals'; Exe='VerifyDrawOnlyVisuals.exe'; Args=@() },
        @{ Name='bone-reader'; Exe='VerifyBoneReader.exe'; Args=@() },
        @{ Name='projection'; Exe='VerifyProjection.exe'; Args=@() },
        @{ Name='driver'; Exe='VerifyDriver.exe'; Args=@() },
        @{ Name='console'; Exe='VerifyConsole.exe'; Args=@() },
        @{ Name='mortar'; Exe='VerifyMortar.exe'; Args=@() },
        @{ Name='mortar-visuals'; Exe='VerifyMortarVisuals.exe'; Args=@() },
        @{ Name='memory-bridge'; Exe='VerifyMemoryBridge.exe'; Args=@() },
        @{ Name='actor-read'; Exe='VerifyActorRead.exe'; Args=@() },
        @{ Name='frame-stability'; Exe='VerifyFrameStability.exe'; Args=@() },
        @{ Name='physx-concurrency'; Exe='VerifyPhysXConcurrency.exe'; Args=@() },
        @{ Name='physx-throttle'; Exe='VerifyPhysXThrottle.exe'; Args=@() },
        @{ Name='touch-recovery'; Exe='VerifyTouchRecovery.exe'; Args=@() },
        @{ Name='scene-bounds'; Exe='VerifySceneBounds.exe'; Args=@() },
        @{ Name='motion'; Exe='VerifyMotion.exe'; Args=@() },
        @{ Name='quotes-unit'; Exe='VerifyQuotes.exe'; Args=@() },
        @{ Name='background'; Exe='VerifyBackdrop.exe'; Args=@() },
        @{ Name='background-images'; Exe='VerifyBackdropImages.exe'; Args=@() },
        @{ Name='background-ui'; Exe='SpatialPreview.exe'; Args=@('--background-test') },
        @{ Name='liquid'; Exe='SpatialPreview.exe'; Args=@('--liquid-test') },
        @{ Name='liquid-unit'; Exe='VerifyLiquid.exe'; Args=@() },
        @{ Name='responsive'; Exe='SpatialPreview.exe'; Args=@('--responsive-test') }
    )
    $taskReport = @()
    foreach ($taskCheck in $taskChecks) {
        $taskLog = Join-Path $PSScriptRoot ('validation-draw-only-' + $taskCheck.Name + '.txt')
        $taskProgram = Join-Path $PSScriptRoot $taskCheck.Exe
        $taskArguments = $taskCheck.Args
        & $taskProgram @taskArguments *> $taskLog
        $taskExit = $LASTEXITCODE
        $taskPass = @(Select-String -LiteralPath $taskLog -Pattern '^PASS ').Count
        if ($taskExit -ne 0 -or $taskPass -eq 0) {
            Get-Content -LiteralPath $taskLog
            throw ('Validation failed: ' + $taskCheck.Name)
        }
        $taskReport += @{ name=$taskCheck.Name; exit_code=$taskExit; passed=$taskPass; log=(Split-Path -Leaf $taskLog) }
        Write-Output ('PASS ' + $taskCheck.Name + ' (' + $taskPass + ' checks)')
    }
    $taskProjectHashes = [ordered]@{}
    Get-ChildItem -LiteralPath (Join-Path $taskProject 'jni') -File -Recurse | Sort-Object FullName | ForEach-Object {
        $taskKey = $_.FullName.Substring($taskProject.Length + 1).Replace('\','/')
        $taskProjectHashes[$taskKey] = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    $taskPreviewHashes = [ordered]@{}
    Get-ChildItem -LiteralPath $PSScriptRoot -File | Where-Object {
        $_.Extension -in @('.cpp','.h','.hpp','.inl','.ps1') -or $_.Name -in @('prepare_draw_only.py','export_pngs.py')
    } | Sort-Object Name | ForEach-Object {
        $taskPreviewHashes[$_.Name] = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    $taskExecutableHashes = [ordered]@{}
    if (($taskInitialProjectHashes | ConvertTo-Json -Compress) -cne ($taskProjectHashes | ConvertTo-Json -Compress) -or
        ($taskInitialPreviewHashes | ConvertTo-Json -Compress) -cne ($taskPreviewHashes | ConvertTo-Json -Compress)) {
        throw 'Source changed during desktop compilation/verification; rerun against a stable source snapshot.'
    }
    Get-ChildItem -LiteralPath $PSScriptRoot -File | Where-Object {
        $_.Name -eq 'SpatialPreview.exe' -or ($_.Name -like 'Verify*.exe')
    } | Sort-Object Name | ForEach-Object {
        $taskExecutableHashes[$_.Name] = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    @{
        generated_at=[DateTime]::UtcNow.ToString('o')
        mode='host UI and simulated rendering; no game or driver access'
        cases=$taskReport
        project_source_sha256=$taskProjectHashes
        preview_sources_sha256=$taskPreviewHashes
        preview_executables_sha256=$taskExecutableHashes
        total=(($taskReport | ForEach-Object { [int]$_.passed } | Measure-Object -Sum).Sum)
    } | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $PSScriptRoot 'validation-summary.json') -Encoding utf8
} finally { Pop-Location }
