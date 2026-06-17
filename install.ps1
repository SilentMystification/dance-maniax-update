# TODO: Remove this hack once the newest DMX.exe is included in the CDN zip files.
# The CDN zips contain an older DMX.exe that would overwrite the newer build in deploy/.

function Show-Tls12BlockedMessage {
    Write-Output "Your machine does not support TLS1.2 "
    Write-Output ""
    Write-Output "Run this script on a newer machine and copy the contents of the Deploy folder back to the cab to play."
}

function Wait-PressAnyKeyToClose {
    Write-Output ""
    Write-Output "Press any key to close..."
    $null = $Host.UI.RawUI.ReadKey('NoEcho,IncludeKeyDown')
}

function Test-Tls12Available {
    try {
        [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
        return $true
    } catch {
        try {
            [Net.ServicePointManager]::SecurityProtocol = 3072
            return $true
        } catch {
            return $false
        }
    }
}

if (-not (Test-Tls12Available)) {
    Show-Tls12BlockedMessage
    Wait-PressAnyKeyToClose
    exit 1
}

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
    try {
        (New-Object Net.WebClient).DownloadFile($url, $file)
    } catch {
        Show-Tls12BlockedMessage
        Wait-PressAnyKeyToClose
        exit 1
    }
    Write-Output "$file download complete."
    Write-Output ""
}

Write-Output "Moving files and applying data..."
Write-Output ""

foreach ($file in $files) {
    Write-Output "Extracting $file..."
    Expand-Archive -LiteralPath $file -DestinationPath 'deploy' -Force
    Write-Output "$file extracted."
    Write-Output ""
}

Write-Output "Cleaning up..."
foreach ($file in $files) {
    Remove-Item -LiteralPath $file -Force
}
Write-Output "Done."
Write-Output ""

# TODO: Remove this hack once the newest DMX.exe is included in the CDN zip files.
Remove-Item -LiteralPath 'deploy\DMX.exe' -Force
Move-Item -Path 'deploy\tmp\DMX.exe' -Destination 'deploy\DMX.exe' -Force
Remove-Item -LiteralPath 'deploy\tmp' -Force

Write-Output "Latest DMX.exe copied."
Write-Output ""
# END TODO

Write-Output "LET'S DANCE!"
Write-Output ""

Wait-PressAnyKeyToClose
