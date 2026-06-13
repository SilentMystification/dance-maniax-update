Judgement Display

Advanced results screen

Add login functionality to Continuous Mode



Audio backend changes:
* Optional native ASIO support
** Global offset in operator menu
** Per player offset in profile
** Wall Clock Synchronization of audio playback to reduce jitter

Phoenix IO - Enhanced extio firmware that runs at 115200 BAUD instead of 9600 BAUD 
* Game runs at 1000 ticks per second internally, 
  * At 9600 BAUD the time it takes for the serial data to make a round trip is ~4ms
  * at 115200 BAUD RTT is < 1ms
* IO is now evaluated PER GAME TICK instead of every 4ish ms
* IO is synced to game 


option files added