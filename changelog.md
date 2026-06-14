Judgement Display

Advanced results screen

Add login functionality to Continuous Mode



Audio backend changes:
* Optional native ASIO support
* Global offset in operator menu
** Per player offset in profile
* Wall Clock Synchronization of audio playback to reduce jitter

Phoenix IO - Enhanced extio firmware that runs at 115200 BAUD instead of 9600 BAUD 
* Game runs at 1000 ticks per second internally, 
  * At 9600 BAUD the time it takes for the serial data to make a round trip is ~4ms
  * at 115200 BAUD RTT is < 1ms
* IO is now evaluated PER GAME TICK instead of every 4ish ms making the game engine what drives the sync instead of waiting for IO to respond.


option files added

Song select options menu for P1 and P2
* Animated gold selector slides smoothly between items; snaps on wrap-around
* Selector combines transparent warm highlight fill + gold outline into one moving unit
* Dark purple outline drawn around each setting item box
* Bobbing gold triangle appears below the active value when editing a setting, disappears on confirm
* Blue bobbing triangles replace < > text for left/right navigation arrows
* Sound effects: open/close sounds, song-select sound on entering edit, mild/wild sound on value change, songwheel appear sound on confirm
* Scroll sound plays when navigating between settings
* "Upside-Down" mirror option renamed to "V-Flip" to prevent text clipping

Center + Mirror bug fix
* Selecting Center play position and Mirror modifier simultaneously caused notes to render at the
  outer columns (0,1,6,7) instead of the center columns (2,3,4,5)
* Fixed by adding a dedicated center-mode mirror matrix to arrangeChart that correctly swaps
  columns within the center range (col2<->col5, col3<->col4)