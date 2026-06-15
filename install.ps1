# TODO: Remove this hack once the newest DMX.exe is included in the CDN zip files.
# The CDN zips contain an older DMX.exe that would overwrite the newer build in deploy/.
New-Item -ItemType Directory -Force -Path 'deploy\tmp' | Out-Null
Move-Item -Path 'deploy\DMX.exe' -Destination 'deploy\tmp\DMX.exe' -Force

Write-Output "Now downloading 7 zip files, 775MB in total..."

#download 7 zip files
(New-Object Net.WebClient).DownloadFile('https://dmx.bossru.sh/update/DMX_initial_data.zip', 'DMX_initial_data.zip')
(New-Object Net.WebClient).DownloadFile('https://dmx.bossru.sh/update/DMX_initial_video.zip', 'DMX_initial_video.zip')
(New-Object Net.WebClient).DownloadFile('https://dmx.bossru.sh/update/DMX_1st.zip', 'DMX_1st.zip')
(New-Object Net.WebClient).DownloadFile('https://dmx.bossru.sh/update/DMX_2nd.zip', 'DMX_2nd.zip')
(New-Object Net.WebClient).DownloadFile('https://dmx.bossru.sh/update/DMX_Update.zip', 'DMX_Update.zip')
(New-Object Net.WebClient).DownloadFile('https://dmx.bossru.sh/update/DMX_2015120100_DMX_2016051800.zip', 'DMX_2015120100_DMX_2016051800.zip')
(New-Object Net.WebClient).DownloadFile('https://dmx.bossru.sh/update/DMX_2016_2019.zip', 'DMX_2016_2019.zip')

#extract and delete them
Expand-Archive -LiteralPath 'DMX_initial_data.zip' -DestinationPath 'deploy' -Force
Expand-Archive -LiteralPath 'DMX_initial_video.zip' -DestinationPath 'deploy' -Force
Expand-Archive -LiteralPath 'DMX_1st.zip' -DestinationPath 'deploy' -Force
Expand-Archive -LiteralPath 'DMX_2nd.zip' -DestinationPath 'deploy' -Force
Expand-Archive -LiteralPath 'DMX_Update.zip' -DestinationPath 'deploy' -Force
Expand-Archive -LiteralPath 'DMX_2015120100_DMX_2016051800.zip' -DestinationPath 'deploy' -Force
Expand-Archive -LiteralPath 'DMX_2016_2019.zip' -DestinationPath 'deploy' -Force

Remove-Item -LiteralPath 'DMX_initial_data.zip' -Force
Remove-Item -LiteralPath 'DMX_initial_video.zip' -Force
Remove-Item -LiteralPath 'DMX_1st.zip' -Force
Remove-Item -LiteralPath 'DMX_2nd.zip' -Force
Remove-Item -LiteralPath 'DMX_Update.zip' -Force
Remove-Item -LiteralPath 'DMX_2015120100_DMX_2016051800.zip' -Force
Remove-Item -LiteralPath 'DMX_2016_2019.zip' -Force

# TODO: Remove this hack once the newest DMX.exe is included in the CDN zip files.
Remove-Item -LiteralPath 'deploy\DMX.exe' -Force
Move-Item -Path 'deploy\tmp\DMX.exe' -Destination 'deploy\DMX.exe' -Force
Remove-Item -LiteralPath 'deploy\tmp' -Force

#links directly to the 7 zip files
#https://dmx.bossru.sh/update/DMX_initial_data.zip
#https://dmx.bossru.sh/update/DMX_initial_video.zip
#https://dmx.bossru.sh/update/DMX_1st.zip
#https://dmx.bossru.sh/update/DMX_2nd.zip
#https://dmx.bossru.sh/update/DMX_Update.zip
#https://dmx.bossru.sh/update/DMX_2015120100_DMX_2016051800.zip
#https://dmx.bossru.sh/update/DMX_2016_2019.zip
