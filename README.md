# THIS PROJECT IS NO LONGER BEING UPDATED
For many years I was updating the source code here with new features, compatibility
for new and different io boards, and bugfixes. Mostly bugfixes.

But now the engine is starting to show its age and it needs a lot of work to remain
useful. I've been putting off updates to the window resolution, the song selection,
the background videos, nonstop mode, and more. And honestly Stepmania just does a
better job now. I don't have much motivation to continue to update my code when
Stepmania is just simply a better engine and a better choice.

In the future I may start a new project, such as a curated Stepmania theme plus
songpack which would be designed for modern rhythm game fans. Such a project likely
would not be allowed on GitHub. In the meantime besides Stepmania I also recommend
running the original game in MAME!

# dance-maniax-update
This repo is for my fan-made recreation of Konami's 1999 arcade game, Dance Maniax.

Unlike other fan game projects which are usually clones or simulators, my goal
was to make a modern Windows port. My software is designed to replace the
original software (and hardware, which may be failing) in a Dance Maniax or
Dance Freaks arcade machine.

The source code in this repo is copyrighted by me and MAY NOT be used to create
further derivative works. (Use Stepmania if that is what you want. This project
is not based on Stepmania, nor is it related to it.)

Permission is granted to run the software and play the game however you like
without restriction.

The installation instructions have been UPDATED as of September 7th, 2022.
Installing the game is now easier than ever!

To install the game download the file "install.ps1" above and run it. This is a
Windows PowerShell script that will download 7 zip files (about 775 MB total),
extract them (about 2 GB total), and then delete the zip files. To run a
Powershell file, first open a Powershell prompt, then type "./" followed by the
name of the file:
`./install.ps1`

If you get a permissions error, run:
`Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass`

The output of the working game will be in the `deploy` folder. This can be run as-is or if you build this project for debugging and need further testing capabilities  

To run the game simply run DMX.exe after a successful install. The first run of
the program will prompt you to change the default settings.
It will also download even more songs!



This will run on basically anything.

Recommended Hardware (I run on this, but have run on much less)
* Windows 7 or higher
* 4GB RAM
* Intel i3 4330
* Intel HD Graphics 

### MAJOR CHANGES IN 2026 UPDATE
# CAB OFFSETS
There is now a configurable cab offset located in the operator menu under Sound Setttings -> Audio Offset.

You should take some time to calibrate this

At a minimum, it should compensate for the 


# PHOENIX IO
The 2026 update brings an updated firmware for cab IO. It is fully backwards compatible with devices previously flashed with extio firmware. 

To enable the new PHOENIX IO, flash the `extio_phoenix.ino` located in `.\firmware\extio_phoenix\` in Arduino Studio or your preferred Arduino flashing tool.

After, enable the new IO backend by renaming the `usephoenixio.option` file to `usephoenixio`

Both of these must done or else the game will fail to properly initialize IO.

If you want to revert back to the original firmware, flash the original `extio.ino` and rename or remove the `usephoenixio` file.

At it's core the new firmware only changes the BAUD rate of the IO's serial device from 9600 BAUD to 115200 BAUD.
This change should reduce input latency and make the game engine drive IO sync instead of waiting for the serial device to respond.

# ASIO SUPPORT
With the 2026 Update, native ASIO is now supported! Originally DMX Update used DirectSound as it's audio backend. 

Now it is togglable between DirectSound and ASIO. To toggle the ASIO backend simply rename the `enableasio.option` file to `enableasio` and boot the game!

This was done a part of the larger goal of improving judgement accuracy. 
Additionally, while this does reduce the latency in the audio pipeline the major gain from this is to consistently set a buffer size that can be compensated for with the new audio offset features.

To enable support you MUST meet the following prerequisites:
* Native ASIO driver for your hardware that supports outputting on channel 0
* 32-bit ASIO driver for your hardware (64-bit OS is fine, but you must have a 32-bit driver for this application to function as it is 32-bit)

Do note, most Realtek ALC chipsets natively support ASIO, just not out of the box. 
If you have a realtek audio device you can try to enable ASIO support using the bundled installer in the `.\ASIO` folder.

On boot you will see one of the 3 messages printed during the DanceManiax System Startup boot screen:
| Boot message | Meaning |
|---|---|
| `SOUND CHECK: OK` | Original DirectSound backend used, ASIO is NOT enabled with an option file |
| `SOUND CHECK: OK - ASIO` | ASIO is enabled via option and successfully initialized |
| `SOUND CHECK: OK - ASIO -> DSOUND` | ASIO is enabled but it did NOT successfully initialize, falling back to DirectSound |