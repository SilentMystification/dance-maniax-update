# Legacy installer for Windows 7 / older PowerShell (no Expand-Archive).
# Same flow as install.ps1; uses TLS 1.2 and Shell.Application for zip extract.
# Win7: install .NET 4.5+ and TLS 1.2 updates (KB3140245) if downloads fail.

function Enable-Tls12 {
    try {
        [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    } catch {
        [Net.ServicePointManager]::SecurityProtocol = 3072
    }
}

function Expand-ZipArchiveLegacy {
    param(
        [string]$Path,
        [string]$DestinationPath
    )

    if (-not (Test-Path $DestinationPath)) {
        New-Item -ItemType Directory -Force -Path $DestinationPath | Out-Null
    }

    $zipFull = (Resolve-Path $Path).Path
    $destFull = (Resolve-Path $DestinationPath).Path
    $shell = New-Object -ComObject Shell.Application
    $zip = $shell.NameSpace($zipFull)
    $dest = $shell.NameSpace($destFull)

    if ($null -eq $zip) {
        throw "Could not open zip: $Path"
    }

    # 0x14 = no progress UI, overwrite existing files
    $dest.CopyHere($zip.Items(), 0x14)

    # Shell copy is async on older Windows — wait until file count stabilizes
    Start-Sleep -Seconds 1
    $stable = 0
    $lastCount = -1
    $timeout = 600
    while ($stable -lt 4 -and $timeout -gt 0) {
        Start-Sleep -Seconds 1
        $timeout = $timeout - 1
        $count = 0
        if (Test-Path $DestinationPath) {
            $count = @(Get-ChildItem -Path $DestinationPath -Recurse -ErrorAction SilentlyContinue).Count
        }
        if ($count -eq $lastCount) {
            $stable = $stable + 1
        } else {
            $stable = 0
            $lastCount = $count
        }
    }
}

Enable-Tls12

# TODO: Remove this hack once the newest DMX.exe is included in the CDN zip files.
# The CDN zips contain an older DMX.exe that would overwrite the newer build in deploy/.
New-Item -ItemType Directory -Force -Path 'deploy\tmp' | Out-Null
Move-Item -Path 'deploy\DMX.exe' -Destination 'deploy\tmp\DMX.exe' -Force

$urls = @(
    'https://dmx.bossru.sh/update/DMX_initial_data.zip',
    'https://dmx.bossru.sh/update/DMX_initial_video.zip',
    'https://dmx.bossru.sh/update/DMX_1st.zip',
    'https://dmx.bossru.sh/update/DMX_2nd.zip',
    'https://dmx.bossru.sh/update/DMX_Update.zip',
    'https://dmx.bossru.sh/update/DMX_2015120100_DMX_2016051800.zip',
    'https://dmx.bossru.sh/update/DMX_2016_2019.zip'
)

$files = $urls | ForEach-Object { [System.IO.Path]::GetFileName($_) }

Write-Output "Now downloading 7 zip files, 775MB in total..."
Write-Output ""

for ($i = 0; $i -lt $urls.Count; $i++) {
    $url  = $urls[$i]
    $file = $files[$i]
    Write-Output "Downloading $url..."
    (New-Object Net.WebClient).DownloadFile($url, $file)
    Write-Output "$file download complete."
    Write-Output ""
}

Write-Output "Moving files and applying data..."
Write-Output ""

foreach ($file in $files) {
    Write-Output "Extracting $file..."
    Expand-ZipArchiveLegacy -Path $file -DestinationPath 'deploy'
    Write-Output "$file extracted."
    Write-Output ""
}

Write-Output "Cleaning up..."
foreach ($file in $files) {
    Remove-Item -Path $file -Force
}
Write-Output "Done."
Write-Output ""

# TODO: Remove this hack once the newest DMX.exe is included in the CDN zip files.
Remove-Item -Path 'deploy\DMX.exe' -Force
Move-Item -Path 'deploy\tmp\DMX.exe' -Destination 'deploy\DMX.exe' -Force
Remove-Item -Path 'deploy\tmp' -Force

Write-Output "Latest DMX.exe copied."
Write-Output ""
# END TODO

Write-Output "LET'S DANCE!"
Write-Output ""

foreach ($i in 5..1) {
    Write-Output "Install script closing in $i..."
    Start-Sleep -Seconds 1
}
