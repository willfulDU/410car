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
} finally { Pop-Location }
