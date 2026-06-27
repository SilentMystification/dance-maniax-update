# Build Dev, Debug, and Production in parallel (same outputs as Configuration=All).
# Usage: ./scripts/build-all.ps1 [-ProjectPath path] [-PlatformToolset v145]

param(
    [string]$ProjectPath = (Join-Path $PSScriptRoot '..\src\DMX_Remake.vcxproj'),
    [string]$PlatformToolset = 'v145'
)

$ErrorActionPreference = 'Stop'

$ProjectPath = (Resolve-Path $ProjectPath).Path
$msbuild = Get-Command msbuild -ErrorAction SilentlyContinue
if (-not $msbuild) {
    Write-Error 'msbuild not found on PATH. Open a Visual Studio Developer shell or run Setup MSBuild.'
}

$commonArgs = @(
    $ProjectPath,
    '/m',
    '/v:minimal',
    '/p:Platform=Win32',
    "/p:PlatformToolset=$PlatformToolset",
    '/t:Build'
)

$configs = @('Dev', 'Debug', 'Production')
$jobs = @()

foreach ($cfg in $configs) {
    $argList = $commonArgs + "/p:Configuration=$cfg"
    Write-Host "Starting $cfg build..."
    $jobs += Start-Process -FilePath $msbuild.Source -ArgumentList $argList -PassThru -NoNewWindow
}

$jobs | Wait-Process

$exitCode = 0
foreach ($job in $jobs) {
    if ($job.ExitCode -ne 0) {
        Write-Error "Configuration build failed with exit code $($job.ExitCode)"
        $exitCode = $job.ExitCode
    }
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$deployDir = Join-Path $repoRoot 'deploy'

foreach ($exe in @('DMX_DEV.exe', 'DMX_DEBUG.exe', 'DMX_PROD.exe')) {
    $path = Join-Path $deployDir $exe
    if (-not (Test-Path $path)) {
        Write-Error "Missing expected output: deploy\$exe"
    }
    Write-Host "OK: deploy\$exe"
}

exit $exitCode
