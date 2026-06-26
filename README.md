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

# Installation Instructions
The installation instructions have been UPDATED as of June 16th, 2026.
Installing the game is now easier than ever!

To install the game download the latest InitialInstall.zip file in the [Releases tab](https://github.com/SilentMystification/dance-maniax-update/releases) 

Unzip `InitialInstall.zip` and right click + run `install.ps1`

The PowerShell script that will download 7 zip files (about 775 MB total),
extract them (about 2 GB total), and then delete the zip files.

If you get a permissions error, run the following in PowerShell from the install directory
to temporarily bypasss execution policies: 
```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
./install.ps1
```

To run the game simply run DMX.exe after a successful install. The first run of
the program will prompt you to change the default settings for the cab.

# Build Instructions
If you want to build from source, pull the latest main-update or master branch 
and fork off of it.  

Build requirements:
Visual Studio or VS Build Tools 2022 / 2026 (earlier will probably work too)

There are 3 types of builds that can be generated:
* Debug
 * Compiler optimizations disabled
 * Game runs in Debug Mode with special overlays and performance metrics
* Development
 * Most optimizations enabled
 * Frame pointers still enabled so traces are readable
 * PDB files generated 
 * Should be used to diagnose crashes
* Production
 * All optimizations enabled including: MaxSpeed + FavorSpeed + OmitFramePointers + whole-program/link-time code generation.
 * No PDB files generated
 * What should actually be deployed to a machine


# System Requirements
This will run on basically anything.

Recommended Hardware (I run on this, but have run on much less)
* Windows 7 or higher *
* 4GB RAM
* Intel i3 4330
* Intel HD Graphics 


\* XP support should be possible if built with VS 2012 but work would need to be done to make the project openable in VS 2012 

# MAJOR CHANGES IN 2026 UPDATE

## GAMEPLAY CHANGES
No more pick 3 play 3 style gameplay. Now you will pick 1 song and play 1 song
in all modes other than Courses.

Continuous Mode is now much more friendly for home use. You can now sign in
on a profile and enjoy endless plays. Scores are saved at the end of each song
instead of at the end of each credit.

## Mid-Credit Settings Screen
There is now a settings screen that can be opened with LEFT + RIGT + START on each side.
The menu can be closed by holding LEFT + RIGHT together.

This menu will let you configure all of the options set after logging in.
Additionally this menu will let you configure all of the newly added features.

Some new expert options:
Fixed scroll speed + Green Number
Configurable Judgement Display
Chart mods (Random, S-Random, Inverted)
Visual Offset
Audio Offset

## JUDGEMENT CHANGES
The way judgements are done for hits has been changed a bit. The timing windows remain
the exact same however, how the game computes your judgement windows has been tightened quite
a bit. This may result in the game feeling different. With no changes you will probably
notice that you are consistently hitting early.

You will most likely need to configure your personal / cab offsets described above
to truly benefit from the changes implemented here.

Additionally, a Marvelous timing window is now available to the player. 
These are all timing windows:

early ←———————— 0 ms ————————→ late

Marvelous: [±24 ms]
Perfect:   [±48 ms]
Great:     [±120 ms]
Good:      [±150 ms]
MISS:      after +150 ms (or no valid hit)

## EX-SCORE
Refered to as EXP in DanceManiax. 

Computed as follows:
Marvelous: +5
Perfect:   +4
Great:     +2
Good:      +1
MISS:      +0

These scores are automatically saved and are viewable by switching to Adavanced Scoring.

## ADVANCED SCORE SCREENS
Expert Settings > Score Mode > Expert
Shows the following information:
* How many Early / Late notes you get per judgement window
* EXP / MAX%
* UR (Unstable Rate)
* AVG (Average offset of a hit note) 

## PHOENIX IO EXTIO FIRMWARE
The 2026 update brings an updated firmware for cab IO. It is fully backwards compatible with devices previously flashed with extio firmware. 

To enable the new PHOENIX IO, flash the `extio_phoenix.ino` located in 
`.\firmware\extio_phoenix\` in Arduino IDE or your preferred Arduino flashing tool.

After, enable the new IO backend by renaming the `usephoenixio.option` file to `usephoenixio`

Both of these must done or else the game will fail to properly initialize IO.

If you want to revert back to the original firmware, flash the original `extio.ino`
located in `.\firmware\extio_phoenix\` and rename or remove the `usephoenixio` file.

At it's core the new firmware only changes the BAUD rate of the IO's serial device from 9600 BAUD to 115200 BAUD.

This change should reduce input latency and make the game engine drive IO sync instead of waiting for the serial device to respond.

For in depth instuctions consult with the `Installation Instructions.md` file in the 
firmware folder.

## ASIO SUPPORT
With the 2026 Update, native ASIO is now supported! Originally DMX Update used DirectSound as it's audio backend. 

Now it is togglable between DirectSound and ASIO. To toggle the ASIO backend simply rename the `enableasio.option` file to `enableasio` and boot the game!

This was done a part of the larger goal of improving judgement accuracy. 
Additionally, while this does reduce the latency in the audio pipeline the major gain from this is to consistently set a buffer size that can be compensated for with the new audio offset features.

To enable support you MUST meet the following prerequisites:
* Native ASIO driver for your hardware that supports outputting on channel 0
* 32-bit ASIO driver for your hardware (64-bit OS is fine, but you must have a 32-bit driver for this application to function as it is 32-bit)

Do note, most Realtek ALC chipsets natively support ASIO and meet this requirement,
just not out of the box. If you have a realtek audio device you can try to enable
ASIO support using the bundled installer in the `.\ASIO` folder. 

Restart after installing and use VBAASIOTest32 to check if ASIO mode is functioning. 
Devices > Realtek ASIO

If you hear a sin wave after selecting you ASIO is working. If Realtek ASIO does not 
show up, your device does not support native ASIO. 

You can tweak the ASIO settings under Device > Control Panel to adjust or view your buffer size. 
Use this information below to configure Cab Offsets below.

On boot you will see one of the 3 messages printed during the DanceManiax System Startup boot screen:
| Boot message | Meaning |
|---|---|
| `SOUND CHECK: OK` | Original DirectSound backend used, ASIO is NOT enabled with an option file |
| `SOUND CHECK: OK - ASIO` | ASIO is enabled via option and successfully initialized |
| `SOUND CHECK: OK - ASIO -> DSOUND` | ASIO is enabled but it did NOT successfully initialize, falling back to DirectSound |

### AUDIO SETUP
If you're using ASIO (or even if you're not) you should set your audio device to
16 bit / 44100hz and disable all sound enhancements.

## CAB OFFSETS
There is now a configurable cab offset located in the operator menu under Sound Setttings -> Audio Offset.

Additionally in the new Settings Menu there are per-player visual and audio offsets.

You should take some time to calibrate these.

At a minimum, you should set it at the same value as your ASIO buffer compensate for the ASIO buffer or directsound buffer. 

For ASIO it is simple:
Offset = (Buffer Length in Samples / Sample Rate in Hz) * 1000

My setup has an ASIO buffer of 384 samples and is running at 44100hz:
Offset = (384 / 44100) * 1000 = 11.6ms
Cab Offset = +12ms

Assuming you just have a line out straight to dumb speakers, this should be your cab offset.

If you have wacky audio effects enabled or otherwise have an audio setup with more latency,

This is a starting point for what the cab offset should be. If your 

If unable to use ASIO, the DirectSound buffer size is shared among other applications so it is
both larger and not directly exposed as a static value. Start at 10ms and work your way up.

Optimally, the Cab's Audio offset should only be used to compensate for hardware audio latency
while player's can compensate for personal preference below.

### PER PLAYER AUDIO AND VISUAL OFFSETS
In the Expert Settings menu there are two options for configuring Visual and Audio offsets. 

Visual Offset (ms) will adjust the Y position of each not on the screen by 1 ms increments.
Judgement windows are not changed or moved AT ALL when using visual offsets. 
* A +42 offset will move all notes to where they would be visually 42ms in the future.
* A -69 offset will move all notes to where they would be visually 69ms in the past.

Audio Offset (ms) will adjust the timing window for judgements in 1ms increments.
The judgement window will slide forwards or backwards but will remain the same size.
* A +20 offset will move the judgement window forward 20ms throughout the entire song.
* A -16 offset will move the judgement window backwards 16ms throughout the entire song.

The Per-Player Audio Offset is meant to be used in tandem with the Audio Offset 
in the Operator Menu to compensate for personal preference. 

**THIS VALUE A DELTA TO WHAT IS SET AS THE CAB OFFSET.**

Cab offset = 12
Player = 4
Total offset = 16

Cab offset = 14
player = -4
Total offset = 10

Ideally, if your cab is calibrated correctly with that offset, the player offset should be 0ms.

To narrow in on this value, enable Score Mode -> Expert in the Expert Settings and pay attention 
to the AVG value you get at the end of each song.

AVG represents the average timing within 2 standard deviations of the mean. 
Effectively it is your average hit time with the outliers trimmed out.

The average AVG that you get across multiple plays is a safe value to use as a personal offset.







