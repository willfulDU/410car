param([string]$VcVars = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat')
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$build = Join-Path $PSScriptRoot 'build'
$source = Join-Path $repo 'User\distance_display\distance_display.c'
if (!(Test-Path -LiteralPath $source)) { throw 'FAIL: the distance receiver/formatter has not been implemented.' }
if (!(Test-Path -LiteralPath $VcVars)) { throw 'MSVC vcvars64.bat is required; pass its path with -VcVars.' }
$setup = & $env:ComSpec /d /c "call `"$VcVars`" >nul && set"
if ($LASTEXITCODE -ne 0) { throw 'MSVC environment setup failed.' }
$seen = @{}
$setup | ForEach-Object {
    if (($_ -match '^([^=]+)=(.*)$') -and !$seen.ContainsKey($matches[1])) {
        $seen[$matches[1]] = $true
        [Environment]::SetEnvironmentVariable($matches[1], $matches[2], 'Process')
    }
}
New-Item -ItemType Directory -Path $build -Force | Out-Null
Push-Location $build
try {
    # Keep the embedded printf retarget separate from the host C runtime.
    & cl.exe /nologo /W4 /WX /wd4100 /std:c11 /source-charset:.936 /Dfputc=Test_USART_fputc /Dfgetc=Test_USART_fgetc `
        "/I$repo\User\distance_display" "/I$PSScriptRoot\stubs" "/I$repo\User" `
        $source "$repo\User\usart\bsp_usart.c" "$PSScriptRoot\test_distance_display.c" /Fe:distance_display_tests.exe
    if ($LASTEXITCODE -ne 0) { throw 'Distance tests did not compile.' }
    & '.\distance_display_tests.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Distance behavior tests failed.' }
    # Compile the real static main-loop helper in isolation to exercise a busy producer.
    $mainText = Get-Content -LiteralPath "$repo\User\main.c" -Raw -Encoding utf8
    $begin = $mainText.IndexOf('#include "distance_display.h"')
    $end = $mainText.IndexOf('#define WHEEL_SPEED_DISPLAY_PERIOD_MS', $begin)
    if ($begin -lt 0 -or $end -le $begin) { throw 'Main ECU distance helper boundaries not found.' }
    [IO.File]::WriteAllText((Join-Path $build 'main_distance_task.inc'), $mainText.Substring($begin, $end - $begin))
    & cl.exe /nologo /W4 /WX /std:c11 /utf-8 "/I$build" "/I$repo\User\distance_display" `
        $source "$PSScriptRoot\test_main_distance_task.c" /Fe:main_distance_task_tests.exe
    if ($LASTEXITCODE -ne 0) { throw 'Main distance task tests did not compile.' }
    & '.\main_distance_task_tests.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Main distance task budget failed.' }
    & cl.exe /nologo /W4 /WX /std:c11 /utf-8 "/I$PSScriptRoot\stubs" "/I$repo\User" `
        "$repo\User\oled\oled.c" "$PSScriptRoot\test_oled_bus.c" /Fe:oled_bus_tests.exe
    if ($LASTEXITCODE -ne 0) { throw 'OLED bus tests did not compile.' }
    & '.\oled_bus_tests.exe'
    if ($LASTEXITCODE -ne 0) { throw 'OLED bus behavior tests failed.' }
} finally { Pop-Location }
