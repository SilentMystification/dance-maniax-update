// gameplayMode.cpp implements the main gameplay loop for DMX
// source file created by Allen Seitz 7/18/2009
// 12-21-09: major refactoring to move all rendering and proceedurally generated graphics to gameplayRendering

#include "../headers/common.h"
#include <math.h>

#include "../headers/dwi_read.h"
#include "../headers/xsq_read.h"

#include "../headers/gameStateManager.h"
#include "../headers/inputManager.h"
#include "../headers/lightsManager.h"
#include "../headers/scoreManager.h"
#include "../headers/gameplayRendering.h"
#include "../headers/particleSprites.h"
#include "../headers/specialEffects.h"
#include "../headers/videoManager.h"

extern int* songIDs;
extern std::string* songTitles;
extern std::string* songArtists;
extern std::string* movieScripts;

extern bool isTestingChart;
extern int testChartSongID;
extern int testChartLevel;

//////////////////////////////////////////////////////////////////////////////
// Constants
//////////////////////////////////////////////////////////////////////////////
// timing windows
#define EARLY_MARVELLOUS 24
#define EARLY_PERFECT    48
#define EARLY_GREAT      120
#define EARLY_GOOD       150
//#define EARLY_BAD        248

#define LATE_MARVELLOUS  24
#define LATE_PERFECT     48
#define LATE_GREAT       120
#define LATE_GOOD        150
//#define LATE_BAD         248

#define JUMP_WINDOW      100
#define HOLD_WINDOW      500

// how long it takes a BPM_CHANGE event to smoothly animate
const int BPM_UPDATE_LENGTH = 400;

// for the between-stage banner animation
const UTIME BANNER_ANIM_LENGTH = 1500;

const UTIME RETIRE_TIMEOUT = 30000;

// macros
#define ISNOTE(x) (x == TAP || x == JUMP)


//////////////////////////////////////////////////////////////////////////////
// Program Variables
//////////////////////////////////////////////////////////////////////////////
// graphics
extern RenderingManager rm;
extern GameStateManager gs;
extern LightsManager lm;
extern VideoManager vm;
extern ScoreManager sm;
extern EffectsManager em;
extern unsigned long int frameCounter;
extern unsigned long int totalGameTime;

bool gameplayInitialized = false;
extern BITMAP* m_banner1;
extern BITMAP* m_banner2;

// song transition
bool isMidTransition = false;
UTIME songTransitionTime = 0;
int rememberedCurrentStage = 1;

// input
bool autoplay = false;
bool useAssistClap = false;
bool debugCheats = false;
extern InputManager im;
int retireTimer = 0; // for ending the game early when there is a lack of input
int lampCycle = 0;
extern volatile UTIME g_dspLastChunkWall;  // wall time of last FMOD DSP chunk boundary (main.cpp)
extern volatile UTIME g_dspChunkCount;     // total DSP chunks fired since FMOD init (main.cpp)
extern volatile UTIME g_dspSongStartChunk; // g_dspChunkCount at the moment playSong() was called (main.cpp)
extern UTIME g_songStartWall;              // timeGetTime() at the moment playSong() was called (main.cpp)

// in-song speed adjustment
static const int SPEED_CHANGE_DISPLAY_MS = 3000;
static const int classicSpeeds[]         = {10,15,20,25,30,35,40,50,60,70,80};
static const int numClassicSpeeds        = 11;
int speedChangeTimer[2]                  = {0, 0};

// full combo
int fullComboAnimStep = 0; // 0 = not started, 1 = started
int fullComboAnimTimer = 0;
bool fullComboP1 = false;
bool fullComboP2 = false;
bool fullComboPerfectP1 = false;
bool fullComboPerfectP2 = false;

// announcer
int announcerPlusPoints = 0;
int announcerMinusPoints = 0;
int announcerQuipCycle = 0;
int announcerLastCheckTotal = 0;
int announcerTargetSpeak = 0; // how many "announcer points" are scored before he makes a comment

// songlist - for unlocks
extern SongEntry* songs;
extern int randomExtraStage;

FSOUND_SAMPLE* assistClap = NULL;
FSOUND_SAMPLE* shockSound = NULL;


//////////////////////////////////////////////////////////////////////////////
// function declarations
//////////////////////////////////////////////////////////////////////////////
void doStepZoneLogic(UTIME dt, int player);
// precondition: dt > 0
// postcondition: this function blinks the stepzone and handles other stepzone animations

void doChartLogic(UTIME dt, int player);
// precondition: dt > 0, a frame of logic has run since the last call
// postcondition: a frame of input logic will be run

int findNextNoteLate(int player, int column);
int findNextNoteEarly(int player, int column);
// precondition: currentChart is initialized, the column is 0-9
// postcondition: returns a note index if successful, or -1

void scoreNote(int player, int judgement, int column);
// precondition: the current game state is valid
// postcondition: updates the combo, lifebar, and score

void finalizeCurrentSongStats(int p);
// precondition: gs.currentStage is still the index of the song just completed
// postcondition: computes avgDiff and unstableRate into sm.player[p].currentSet[gs.currentStage], clears noteDiffCount

void loadNextSong();
// precondition: gs.player[] has a setlist setlist, the chart data is loaded
// postcondition: reloads the audio and resets certain variables

void arrangeChart(std::vector<struct ARROW> *chart, std::vector<struct FREEZE> *holds, char type, bool isDoubles, bool isCenter);
// precondition: see description of arguments at function declaration
// postcondition: if type != 0 then the chart and holds will be modified

void applyChartMod(std::vector<struct ARROW> *chart, std::vector<struct FREEZE> *holds, int mod, bool isDoubles, bool isCenter, bool isRightSide);
// precondition: mod is one of 0=Off, 1=Random, 2=S-Random, 3=D-Random, 4=Inverted
// postcondition: if mod != 0 then the chart and holds are rearranged in place

int checkForExtraStages();
// precondition: the game is between stages, and at least gs.numStagesPerCredit have been cleared
// postcondition: modifies the song choices and returns 0-2, the number of bonus songs awarded

void generateTimingReport();
// UNUSED: needed while I was trying to figure out how to sync xsq charts


//////////////////////////////////////////////////////////////////////////////
// function implementations
//////////////////////////////////////////////////////////////////////////////
void firstGameplayLoop()
{
#ifdef DMXDEBUG
	debugCheats = true;
	autoplay = true;
#endif
	if ( isTestingChart )
	{
		autoplay = true;
		useAssistClap = true;
	}

	if ( isTestingChart )
	{
		gs.currentStage = 0; // loop!
	}

	// reset the player's stats
	announcerQuipCycle = rand()%4;
	retireTimer = fullComboAnimTimer = fullComboAnimStep = 0;
	fullComboP1 = fullComboP2 = false;
	for ( int p = 0; p < 2; p++ )
	{
		gs.player[p].nextStage();
		gs.player[p].displayCombo = 0;
		gs.player[p].stepZoneTimePerBeat = BPM_TO_MSEC(gs.player[p].scrollRate);
		gs.player[p].comboColor = COMBO_PERFECT;
	}
	rememberedCurrentStage = gs.currentStage; // this won't change during the transition. for most of the song it will remain the same
	announcerPlusPoints = 0;
	announcerMinusPoints = 0;
	announcerLastCheckTotal = 0;
	announcerTargetSpeak = 0;
	

	// load certain sound effects only during the first run of the game
	if ( !gameplayInitialized )
	{
		assistClap = FSOUND_Sample_Load(FSOUND_FREE, "data/sfx/clap.wav", FSOUND_NORMAL, 0, 0);
		shockSound = FSOUND_Sample_Load(FSOUND_FREE, "data/sfx/shock.wav", FSOUND_NORMAL, 0, 0);
	}

	// DEBUG: if booting directly into gameplay mode, set stuff
	if ( gs.player[0].stagesPlayed[0] <= 0 || isTestingChart )
	{
		TRACE("DEBUG START IN GAME MODE");
		gs.player[0].stagesPlayed[0] = gs.player[0].stagesPlayed[1] = gs.player[0].stagesPlayed[2] = testChartSongID;
		gs.player[0].stagesLevels[0] = gs.player[0].stagesLevels[1] = gs.player[0].stagesLevels[2] = testChartLevel;
		gs.player[1].stagesPlayed[0] = gs.player[1].stagesPlayed[1] = gs.player[1].stagesPlayed[2] = testChartSongID;
		gs.player[1].stagesLevels[0] = gs.player[1].stagesLevels[1] = gs.player[1].stagesLevels[2] = testChartLevel;
		if ( testChartLevel >= DOUBLE_MILD )
		{
			gs.isDoubles = true;
			gs.isVersus = false;
		}
		else
		{
			gs.isDoubles = false;
			gs.isVersus = true;
		}
	}
	clear_keybuf(); // also for debug

	loadNextSong();
	gameplayInitialized = true;
	lampCycle = 0;

	im.setCooldownTime(67); // 1/15th of a second
}

void mainGameplayLoop(UTIME dt)
{
	// Sync song clock before chart logic so timeElapsed is current when hits are judged.
	// Re-anchor to FMOD's position each update; interpolate with wall clock between anchors.
	UTIME now = timeGetTime();
	if (g_dspChunkCount > g_dspSongStartChunk && g_dspChunkCount != gs.bgmLastChunkCount)
	{
		int freq = FSOUND_GetOutputRate();
		if (freq > 0)
		{
			UTIME chunks = g_dspChunkCount - g_dspSongStartChunk;
			int bufLen   = FSOUND_DSP_GetBufferLength();
			gs.bgmAnchorFmodMs = (long long)chunks * bufLen * 1000 / freq;
			gs.bgmAnchorWall   = g_dspLastChunkWall;
			gs.bgmLastChunkCount = g_dspChunkCount;
			gs.bgmSyncAnchored = true;
		}
	}

	if (gs.bgmSyncAnchored)
	{
		long syncedBase = (long)(now - gs.bgmAnchorWall) + gs.bgmAnchorFmodMs;
		int gap0 = gs.bgmGap + sm.player[0].audioOffset;
		int gap1 = gs.bgmGap + sm.player[1].audioOffset;
		gs.player[0].timeElapsed = (UTIME)MAX(0, syncedBase + gap0);
		gs.player[1].timeElapsed = (UTIME)MAX(0, syncedBase + gap1);

		static UTIME lastDriftTrace = 0;
		if (gs.player[0].timeElapsed - lastDriftTrace >= 5000)
		{
			lastDriftTrace = gs.player[0].timeElapsed;
			int freq = FSOUND_GetOutputRate();
			int bufLen = FSOUND_DSP_GetBufferLength();
			UTIME chunks = g_dspChunkCount - g_dspSongStartChunk;
			long chunkMs = freq > 0 ? (long long)chunks * bufLen * 1000 / freq : 0;
			unsigned int fmodPos = FSOUND_GetCurrentPosition(gs.currentSongChannel);
			long fmodMs = freq > 0 ? (fmodPos / freq * 1000 + fmodPos % freq * 1000 / freq) : 0;
			long wallElapsed = (long)(now - g_songStartWall);
			al_trace("drift check: timeElapsed=%d wallElapsed=%ld chunkMs=%ld fmodMs=%ld gap=%d\r\n",
				gs.player[0].timeElapsed, wallElapsed, chunkMs, fmodMs, gap0);
		}
	}
	else
	{
		gs.player[0].timeElapsed += dt;
		gs.player[1].timeElapsed += dt;
	}

	int p = 0;
	for ( p = 0; p < (gs.isVersus ? 2 : 1); p++ )
	{
		doStepZoneLogic(dt, p);
		doChartLogic(dt, p);

		// process BPM gimmicks
		SUBTRACT_TO_ZERO(gs.player[p].stopLength, dt);
		int prevBpmTimer = gs.player[p].bpmUpdateTimer;
		SUBTRACT_TO_ZERO(gs.player[p].bpmUpdateTimer, dt);
		gs.player[p].scrollRate = WEIGHTED_AVERAGE(gs.player[p].scrollRate, gs.player[p].newScrollRate, gs.player[p].bpmUpdateTimer, BPM_UPDATE_LENGTH);
		if ( prevBpmTimer > 0 && gs.player[p].bpmUpdateTimer == 0 )
		{
			al_trace("BPM transition complete p%d: scrollRate=%d newScrollRate=%d timeElapsed=%d\r\n",
				p, gs.player[p].scrollRate, gs.player[p].newScrollRate, gs.player[p].timeElapsed);
		}
	}

	SUBTRACT_TO_ZERO(songTransitionTime, dt);

	// do full combo anim
	if ( fullComboAnimStep > 0 )
	{
		fullComboAnimTimer += dt;
		fullComboAnimStep = fullComboAnimTimer/1000 + 1;
		if ( fullComboAnimTimer >= 4000 )
		{
			fullComboAnimStep = fullComboAnimTimer = 0;
			fullComboP1 = fullComboP2 = false;
			fullComboPerfectP1 = fullComboPerfectP2 = false;
		}
	}

	updateParticles(dt);
	renderGameplay();

	gs.player[0].judgementTime += dt;
	gs.player[1].judgementTime += dt;

	// implement the "retire" timer (game was abandoned) -- disabled in continuous mode
	if ( !autoplay && !gs.isFreestyleMode )
	{
		retireTimer += dt;
		for ( int i = 0; i < 8; i++ )
		{
			if ( im.getReleaseLength(i) < retireTimer )
			{
				retireTimer = 0;
			}
		}
		if ( retireTimer > 20000 )
		{
			finalizeCurrentSongStats(0);
			finalizeCurrentSongStats(1);
			gs.g_currentGameMode = RESULTS;
			gs.g_gameModeTransition = 1;
			sm.savePlayersToDisk();
			em.announcerQuip(86); // say "I can't wait anymore"
			return;
		}
	}

	// quick quit: all 6 buttons held simultaneously ends the song and continues the credit
	if ( im.isKeyDown(MENU_LEFT_1P) && im.isKeyDown(MENU_RIGHT_1P) && im.isKeyDown(MENU_START_1P) &&
	     im.isKeyDown(MENU_LEFT_2P) && im.isKeyDown(MENU_RIGHT_2P) && im.isKeyDown(MENU_START_2P) )
	{
		finalizeCurrentSongStats(0);
		finalizeCurrentSongStats(1);
		sm.player[0].currentSet[gs.currentStage].status = STATUS_FAILED;
		sm.player[1].currentSet[gs.currentStage].status = STATUS_FAILED;
		gs.currentStage++;
		int numBonusStages = 0;
		if ( gs.currentStage >= gs.numSongsPerSet )
		{
			numBonusStages = checkForExtraStages();
		}
		gs.returningToSongwheel = true;
		gs.g_currentGameMode = RESULTS;
		gs.g_gameModeTransition = 1;
		if ( !gs.isFreestyleMode && gs.currentStage >= gs.numSongsPerSet + numBonusStages )
		{
			gs.creditComplete = true;
			sm.savePlayersToDisk();
		}
		return;
	}

	// in-song speed adjustment: LEFT/RIGHT = coarse change, START+LEFT/RIGHT = fine change (fixed mode only)
	for ( int side = 0; side < 2; side++ )
	{
		int target = (gs.isVersus ? side : 0);
		bool left  = im.getKeyState(side == 0 ? MENU_LEFT_1P  : MENU_LEFT_2P)  == JUST_DOWN;
		bool right = im.getKeyState(side == 0 ? MENU_RIGHT_1P : MENU_RIGHT_2P) == JUST_DOWN;
		bool start = im.isKeyDown(side == 0 ? MENU_START_1P : MENU_START_2P) != 0;

		if ( left || right )
		{
			if ( gs.player[target].scrollMode == 1 ) // Fixed mode
			{
				int delta = start ? 5 : 50;
				if ( left )  gs.player[target].fixedScrollPPS = MAX(25,  gs.player[target].fixedScrollPPS - delta);
				if ( right ) gs.player[target].fixedScrollPPS = MIN(700, gs.player[target].fixedScrollPPS + delta);
			}
			else // Classic mode: cycle through discrete speed list
			{
				int idx = 0;
				for ( int i = 0; i < numClassicSpeeds; i++ )
				{
					if ( classicSpeeds[i] == gs.player[target].speedMod ) { idx = i; break; }
				}
				if ( left )  idx = MAX(0, idx - 1);
				if ( right ) idx = MIN(numClassicSpeeds - 1, idx + 1);
				gs.player[target].speedMod = classicSpeeds[idx];
			}
			speedChangeTimer[target] = SPEED_CHANGE_DISPLAY_MS;
		}
	}
	for ( int t = 0; t < (gs.isVersus ? 2 : 1); t++ )
		speedChangeTimer[t] = MAX(0, speedChangeTimer[t] - (int)dt);

	// update the per-column judgements and the "step zone resize" effect when a panel is newly hit (DDR only)
	for ( int i = 0; i < 10; i++ )
	for ( p = 0; p < (gs.isVersus ? 2 : 1); p++ )
	{
		gs.player[p].columnJudgeTime[i] += dt;

		if ( im.getKeyState(i) == JUST_DOWN ) // only matters for DDR
		{
			gs.player[p].stepZoneResizeTimers[i] = RESIZE_TIME;
		}
	}

	// check for the announcer making a comment
	if ( announcerPlusPoints + announcerMinusPoints > announcerLastCheckTotal )
	{
		announcerLastCheckTotal = announcerPlusPoints + announcerMinusPoints;

		if ( announcerLastCheckTotal > announcerTargetSpeak )
		{
			// index 0 and 2 are "long", index 1 and 3 are "short"
			static int GUY_PLAYING_PERFECT[4] = { 107, 132,  99, 140 };
			static int GUY_PLAYING_GREAT[4]   = {  65, 117,  98, 119 };
			static int GUY_PLAYING_UNWELL[4]  = { 116, 144, 111, 146 };
			static int GUY_PLAYING_GIVEUP[4]  = {  93, 127,  94, 128 };
			bool talked = false;

			if ( announcerPlusPoints > 0 && announcerMinusPoints == 0 )
			{
				talked = em.announcerQuipChance(GUY_PLAYING_PERFECT[announcerQuipCycle], 10);
			}
			else if ( announcerPlusPoints > announcerMinusPoints*12 )
			{
				talked = em.announcerQuipChance(GUY_PLAYING_GREAT[announcerQuipCycle], 10);
			}
			else if ( announcerPlusPoints > announcerMinusPoints )
			{
				talked = em.announcerQuipChance(GUY_PLAYING_UNWELL[announcerQuipCycle], 10);
			}
			else if ( announcerPlusPoints < announcerMinusPoints )
			{
				talked = em.announcerQuipChance(GUY_PLAYING_GIVEUP[announcerQuipCycle], 10);
			}

			if ( talked )
			{
				announcerLastCheckTotal = announcerPlusPoints = announcerMinusPoints = 0;
				announcerQuipCycle = (announcerQuipCycle + 1) % 4;
			}
		}
	}

	// check for the game ending suddenly due t0 the battery lifebar
	if ( gs.player[0].useBattery && gs.player[0].lifebarLives <= 0 )
	{
		if ( !gs.isVersus || (gs.player[1].useBattery && gs.player[1].lifebarLives <= 0) )
		{
			// don't leave the clear status blank
			sm.player[0].currentSet[gs.currentStage].status = STATUS_FAILED;
			sm.player[1].currentSet[gs.currentStage].status = gs.isVersus ? STATUS_FAILED : STATUS_NONE;
			sm.savePlayersToDisk();

			// where are we going?
			if ( gs.player[0].useHazard||gs.player[1].useHazard )
			{
				bool shortList = gs.player[0].stagesPlayed[gs.currentStage + 1] <= 0; // true when, for any reason, not enough songs were picked to fill the setlist
				if ( gs.currentStage+1 >= gs.numSongsPerSet || shortList )
				{
					gs.g_currentGameMode = FAILURE;
					em.announcerQuip(GUY_STAGE_HAZARD_FAILED);
				}
				else
				{
					gs.currentStage++;
					gs.g_currentGameMode = GAMEPLAY;
					em.announcerQuip(GUY_STAGE_HAZARD_FAILED);
				}
			}
			else
			{
				gs.g_currentGameMode = FAILURE;
				em.announcerQuip(GUY_STAGE_FAILED);
			}

			gs.g_gameModeTransition = 1;
			gs.killSong();
			em.playSample(SFX_FAILURE_WHOOSH);
		}
	}

	// update the lamps
	static int orbCycle[4] = { 0, 2, 1, 2 }; // red, purple, blue, purple, each 4/4 measure
	for ( int orb = 0; orb < 4; orb++ )
	{
		lm.setOrbColor(0, orb, orbCycle[lampCycle%4], 100);
		lm.setOrbColor(1, orb, orbCycle[lampCycle%4], 100);
		if ( lampCycle % 2 == 0 )
		{
			lm.setLamp(spotlightA, gs.player[0].stepZoneTimePerBeat / 4);
			lm.setLamp(spotlightC, gs.player[0].stepZoneTimePerBeat / 4);
		}
		else
		{
			lm.setLamp(spotlightB, gs.player[0].stepZoneTimePerBeat / 4);
		}
	}

	// these should only work in debug mode
	while (keypressed() == TRUE && (debugCheats || isTestingChart) )
	{
		int k = readkey() >> 8;

		if ( k == KEY_O ) // restart the song
		{
			gs.g_currentGameMode = GAMEPLAY;
			gs.g_gameModeTransition = 1;
		}
		if ( k == KEY_F6 )
		{
			gs.player[0].useBattery = true;
			gs.player[0].lifebarLives = 4;
			if ( gs.isVersus )
			{
				gs.player[1].useBattery = true;
				gs.player[1].lifebarLives = 4;
			}
		}
		if ( k == KEY_F7 )
		{
			gs.player[0].useBattery = false;
			if ( gs.isVersus )
			{
				gs.player[1].useBattery = false;
			}
		}

		if ( k == KEY_F10 )
		{
			autoplay = !autoplay;
		}
		if ( k == KEY_F11 )
		{
			useAssistClap = !useAssistClap;
		}
		if ( k == KEY_V )
		{
			gs.player[0].centerLeft = !gs.player[0].centerLeft;
		}
		if ( k == KEY_B )
		{
			gs.player[0].centerRight = !gs.player[0].centerRight;
		}
		if ( k == KEY_M )
		{
			gs.player[0].danceManiaxMode = !gs.player[0].danceManiaxMode;
		}
		if ( k == KEY_COMMA )
		{
			gs.player[0].drummaniaMode = !gs.player[0].drummaniaMode;
		}
		if ( k == KEY_OPENBRACE && fullComboAnimTimer == 0 )
		{
			gs.setSongPosition(-5000);
		}
		if ( k == KEY_CLOSEBRACE && fullComboAnimTimer == 0 )
		{
			gs.setSongPosition(+5000);
		}
		if ( k == KEY_K )
		{
			gs.player[0].shockAnimTimer = SHOCK_ANIM_TIME;
			playSFXOnce(shockSound);
		}
	}
}

void doStepZoneLogic(UTIME dt, int p)
{
	gs.player[p].stepZoneBeatTimer += dt;
	SUBTRACT_TO_ZERO(gs.player[p].stepZoneBlinkTimer, dt);
	SUBTRACT_TO_ZERO(gs.player[p].shockAnimTimer, dt);
	for ( int i = 0; i < 10; i++ )
	{
		SUBTRACT_TO_ZERO(gs.player[p].stepZoneResizeTimers[i], dt);
		SUBTRACT_TO_ZERO(gs.player[p].laneFlareTimers[i], dt);
	}
	for ( int i = 0; i < 4; i++ )
	{
		SUBTRACT_TO_ZERO(gs.player[p].drummaniaCombo[i], dt);
	}

	if ( gs.player[p].stepZoneBeatTimer > gs.player[p].stepZoneTimePerBeat )
	{
		gs.player[p].stepZoneBeatTimer -= gs.player[p].stepZoneTimePerBeat;
		gs.player[p].stepZoneBlinkTimer = gs.player[p].stepZoneTimePerBeat / 4;
		if ( p == 0 )
		{
			lampCycle++;
		}
		gs.player[p].stepZoneTimePerBeat = BPM_TO_MSEC(gs.player[p].scrollRate);
		gs.player[p].colorCycle = gs.player[p].colorCycle == 3 ? 0 : gs.player[p].colorCycle + 1; // used by DDR for the 4-frame DDR arrow animation
	}
	//al_trace("%d\n", stepZoneBlinkTimer);
}

void doChartLogic(UTIME dt, int p)
{
	unsigned int n = gs.player[p].currentNote;

	// step through the chart until the next non-current note is found
	while ( gs.player[p].currentChart.size() > 0 && gs.player[p].currentChart[n].timing < gs.player[p].timeElapsed )
	{
		//al_trace("time elapsed is %d\r\n", gs.player[p].timeElapsed);
		// mark notes as hit
		if ( autoplay )
		{
			if ( ISNOTE(gs.player[p].currentChart[n].type) )
			{
				scoreNote(p, PERFECT, gs.player[p].currentChart[n].columns[0]);
				gs.player[p].currentChart[n].judgement = PERFECT;
				gs.player[p].laneFlareTimers[gs.player[p].currentChart[n].columns[0]] = HIT_FLASH_DISPLAY_TIME;
				gs.player[p].laneFlareColors[gs.player[p].currentChart[n].columns[0]] = 0;
			}
		}
		if ( useAssistClap && assistClap && ISNOTE(gs.player[p].currentChart[n].type) )
		{
			playSFXOnce(assistClap); 
		}
		if ( gs.player[p].currentChart[n].type == BPM_CHANGE )
		{
			int targetRate = gs.player[p].currentChart[n].color;
			al_trace("BPM_CHANGE p%d: timing=%d timeElapsed=%d color=%d scrollRate=%d newScrollRate=%d\r\n",
				p, gs.player[p].currentChart[n].timing, gs.player[p].timeElapsed,
				targetRate, gs.player[p].scrollRate, gs.player[p].newScrollRate);
			if ( gs.player[p].newScrollRate != targetRate )
			{
				gs.player[p].bpmUpdateTimer = BPM_UPDATE_LENGTH;
				gs.player[p].newScrollRate  = targetRate;
			}
			// else: look-ahead already seeded the animation; leave bpmUpdateTimer counting down
		}
		if ( gs.player[p].currentChart[n].type == SCROLL_STOP )
		{
			gs.player[p].stopLength = gs.player[p].currentChart[n].color;
			gs.player[p].stopTime = gs.player[p].timeElapsed;
		}
		if ( gs.player[p].currentChart[n].type == NEW_SECTION )
		{
			// should not happen in DMX
		}
		if ( p == 0 && gs.player[p].currentChart[n].type == END_SONG ) // NOT A BUG! Only P1's chart ends the song (could be weird in versus mode)
		{
			int bestStatus = MAX(sm.player[0].currentSet[gs.currentStage].calculateStatus(), sm.player[1].currentSet[gs.currentStage].calculateStatus());
			TRACE("Best Clear Status: %d (%d %d)", bestStatus, sm.player[0].currentSet[gs.currentStage].status, sm.player[1].currentSet[gs.currentStage].status);

			finalizeCurrentSongStats(0);
			finalizeCurrentSongStats(1);
			gs.currentStage++;
			isMidTransition = false; // prevent a crash with the fast-foward debug key

			int numBonusStages = 0;
			if ( gs.currentStage >= gs.numSongsPerSet )
			{
				numBonusStages = checkForExtraStages();
			}

			if ( fullComboAnimStep > 0 ) // let the full combo animation play out before per-song results
			{
				gs.currentStage--;
				return;
			}
			gs.returningToSongwheel = true;
			gs.g_currentGameMode = RESULTS;
			gs.g_gameModeTransition = 1;
			if ( !gs.isFreestyleMode && gs.currentStage >= gs.numSongsPerSet + numBonusStages )
			{
				gs.creditComplete = true;
				sm.savePlayersToDisk();
			}
			return;
		}
		n++;

		++gs.player[p].currentNote;

		if ( gs.player[p].currentNote == (int)gs.player[p].currentChart.size() )
		{
			gs.player[p].currentNote = (int)gs.player[p].currentChart.size() - 1;
			break;
		}
	}

	// check for late misses
	n = gs.player[p].lastLateNote;
	while ( (int)n < gs.player[p].currentNote )
	{
		// time to advance?
		if ( gs.player[p].currentChart[n].timing + LATE_GOOD < gs.player[p].timeElapsed )
		{
			if ( gs.player[p].currentChart[n].judgement == UNSET )
			{
				if ( gs.player[p].currentChart[n].type == TAP || gs.player[p].currentChart[n].type == JUMP )
				{
					gs.player[p].lastJudgementDiff = (int)(gs.player[p].timeElapsed - gs.player[p].currentChart[n].timing);
					gs.player[p].lastJudgementEarly = false;
					scoreNote(p, MISS, gs.player[p].currentChart[n].columns[0]);

					// Drummania and IIDX don't have 'jumps', each note is separate, hmm....
					if ( gs.player[p].drummaniaMode && gs.player[p].currentChart[n].columns[1] != -1 )
					{
						gs.player[p].columnJudgement[gs.player[p].currentChart[n].columns[1]] = MISS;
						gs.player[p].columnJudgeTime[gs.player[p].currentChart[n].columns[1]] = 0;
					}
					gs.player[p].currentChart[n].judgement = MISS;
				}

				// shock arrows use a slightly smaller late window
				if ( gs.player[p].currentChart[n].type == SHOCK && (gs.player[p].currentChart[n].timing + LATE_GOOD < gs.player[p].timeElapsed) )
				{
					gs.player[p].currentChart[n].judgement = OK;
					scoreNote(p, OK, -1);
					gs.player[p].displayCombo++;
					gs.player[p].currentSongCombo++;
				}
			}
			gs.player[p].lastLateNote++;
		}
		n++;
	}

	// check for player input
	if ( !autoplay )
	{
		for (int i = 0; i < 10; i++ ) // each column
		{
			int lateNote = findNextNoteLate(p, i);
			int earlyNote = findNextNoteEarly(p, i);
			int diff = 0, closestNote = -1;

			// which is closer, the next early note, or the next late note?
			if ( lateNote == -1 && earlyNote != -1 )
			{
				closestNote = earlyNote;
				diff = gs.player[p].currentChart[earlyNote].timing - gs.player[p].timeElapsed;
			}
			else if ( lateNote != -1 && earlyNote == -1 )
			{
				closestNote = lateNote;
				diff = gs.player[p].timeElapsed - gs.player[p].currentChart[lateNote].timing;
			}
			else if ( lateNote != -1 && earlyNote != -1 )
			{
				int late = gs.player[p].timeElapsed - gs.player[p].currentChart[lateNote].timing;
				int early = gs.player[p].currentChart[earlyNote].timing - gs.player[p].timeElapsed;
				closestNote = late < early ? lateNote : earlyNote; // HELL: reverse this condition
				diff = MIN(late, early); // HELL: change this to a MAX as well
			}

			if ( closestNote == -1 )
			{
				continue;
			}

			if ( im.getKeyState(i) == JUST_DOWN )
			{
				//char debugJudges[11] = "?MPGDBMon!";
				int judgement = 0;

				// check the time difference against the late or early windows
				if ( gs.player[p].currentChart[closestNote].timing > gs.player[p].timeElapsed )
				{
					if ( diff < EARLY_MARVELLOUS )
						judgement = MARVELLOUS;
					else if ( diff < EARLY_PERFECT )
						judgement = PERFECT;
					else if ( diff < EARLY_GREAT )
						judgement = GREAT;
					else if ( diff < EARLY_GOOD )
						judgement = GOOD;
					//else if ( diff < EARLY_BAD )
					//	judgement = BAD;			

					//al_trace("[%d] EARLY %c: %ld\r\n", closestNote, debugJudges[judgement], diff);
				}
				else
				{
					if ( diff < EARLY_MARVELLOUS )
						judgement = MARVELLOUS;
					else if ( diff < EARLY_PERFECT )
						judgement = PERFECT;
					else if ( diff < EARLY_GREAT )
						judgement = GREAT;
					else if ( diff < EARLY_GOOD )
						judgement = GOOD;
					//else if ( diff < EARLY_BAD )
					//	judgement = BAD;

					//al_trace("[%d] LATE %c: %ld\r\n", closestNote, debugJudges[judgement], diff);
				}

				// jumps need both arrows to be pressed (NOTE: unless a special exception is made, 'jumps' in DMX charts are read as separate singles!)
				int col1 = gs.player[p].currentChart[closestNote].columns[0];
				int col2 = gs.player[p].currentChart[closestNote].columns[1];
				if ( gs.player[p].currentChart[closestNote].type == JUMP )
				{
					if ( !im.isKeyDown(col1) || !im.isKeyDown(col2) || im.getHoldLength(col1) > JUMP_WINDOW || im.getHoldLength(col2) > JUMP_WINDOW )
					{
						continue; // failed to hit the two keys simultaneously
					}
				}

				if ( gs.player[p].currentChart[closestNote].type == SHOCK )
				{
					// oops! did the player tap a shock arrow?
					if ( im.isKeyInUse(i) && judgement >= MARVELLOUS && judgement <= GOOD && gs.player[p].currentChart[closestNote].judgement == UNSET )
					{
						scoreNote(p, NG, -1);
						gs.player[p].currentChart[closestNote].judgement = NG;
						gs.player[p].shockAnimTimer = SHOCK_ANIM_TIME;
						playSFXOnce(shockSound);
					}
				}
				else
				{
					gs.player[p].lastJudgementDiff = diff;
					gs.player[p].lastJudgementEarly = gs.player[p].currentChart[closestNote].timing > gs.player[p].timeElapsed;
					scoreNote(p,judgement, col1);
					if ( judgement == MARVELLOUS || judgement == PERFECT || judgement == GREAT )
					{
						gs.player[p].laneFlareTimers[col1] = HIT_FLASH_DISPLAY_TIME;
						gs.player[p].laneFlareColors[col1] = judgement == MARVELLOUS ? 0 : 1;
						if ( col2 != -1 )
						{
							gs.player[p].laneFlareTimers[col2] = HIT_FLASH_DISPLAY_TIME;
							gs.player[p].laneFlareColors[col2] = judgement == MARVELLOUS ? 0 : 1;
						}
					}
					gs.player[p].currentChart[closestNote].judgement = judgement;
				}
			}
			else if ( im.getKeyState(i) == HELD_DOWN )
			{
				if ( gs.player[p].currentChart[closestNote].type == SHOCK )
				{
					// oops! did the player hold a shock arrow
					if ( im.isKeyInUse(i) && im.getHoldLength(i) > JUMP_WINDOW && gs.player[p].currentChart[closestNote].judgement == UNSET )
					{
						scoreNote(p, NG, -1);
						gs.player[p].currentChart[closestNote].judgement = NG;
						gs.player[p].shockAnimTimer = SHOCK_ANIM_TIME;
						playSFXOnce(shockSound);
					}
				}
			}
		}
	}

	// process freeze arrow logic
	n = gs.player[p].currentFreeze;
	while ( n < (int)gs.player[p].freezeArrows.size() )
	{
		int col1 = gs.player[p].freezeArrows[n].columns[0];
		int col2 = gs.player[p].freezeArrows[n].columns[1];

		// first, is this hold graded? if so no further work needs to be done on it
		if ( gs.player[p].freezeArrows[n].judgement == OK || gs.player[p].freezeArrows[n].judgement == NG )
		{
			UTIME tailTime = 3000 + MAX(gs.player[p].freezeArrows[n].endTime1, gs.player[p].freezeArrows[n].endTime2);
			bool holdIsAncientHistory = (gs.player[p].freezeArrows[n].judgement == NG && tailTime < gs.player[p].timeElapsed) || gs.player[p].freezeArrows[n].judgement == OK;

			if ( (int)n == gs.player[p].currentFreeze && holdIsAncientHistory )
			{
				gs.player[p].currentFreeze++;
			}
			n++;
			continue;
		}

		// did the player start this hold?
		if ( gs.player[p].freezeArrows[n].startTime < gs.player[p].timeElapsed && im.isKeyDown(col1) && (col2 == -1 || im.isKeyDown(col2)) )
		{
			gs.player[p].freezeArrows[n].isHeld = 1;
		}

		// next, would this hold melt? update the melt time
		if ( gs.player[p].freezeArrows[n].startTime < gs.player[p].timeElapsed || gs.player[p].freezeArrows[n].isHeld == 1 )
		{
			// update flashing (holding) animation
			if ( gs.player[p].freezeArrows[n].isHeld == 1 )
			{
				gs.player[p].laneFlareTimers[col1] = HIT_FLASH_DISPLAY_TIME;
				gs.player[p].laneFlareColors[col1] = 2; //(gs.player[p].timeElapsed / 50) % 2 + 1;
			}
			
			// autoplay needs this to render properly
			if ( autoplay )
			{
				gs.player[p].freezeArrows[n].isHeld = 1;
			}

			if ( (!im.isKeyDown(col1) || gs.player[p].freezeArrows[n].isHeld != 1 ) && !autoplay )
			{
				gs.player[p].freezeArrows[n].meltTime1 += dt;
			}
			else if ( gs.player[p].freezeArrows[n].isHeld == 1 )
			{
				gs.player[p].freezeArrows[n].meltTime1 = 0;
			}

			if ( col2 != -1 )
			{
				// update flashing (holding) animation on column 2
				if ( gs.player[p].freezeArrows[n].isHeld == 1 )
				{
					gs.player[p].laneFlareTimers[col2] = HIT_FLASH_DISPLAY_TIME;
					gs.player[p].laneFlareColors[col2] = 2; //(gs.player[p].timeElapsed / 50) % 2 + 1;
				}

				if ( (!im.isKeyDown(col2) || gs.player[p].freezeArrows[n].isHeld != 1 ) && !autoplay )
				{
					gs.player[p].freezeArrows[n].meltTime2 += dt;
				}
				else if ( gs.player[p].freezeArrows[n].isHeld == 1 )
				{
					gs.player[p].freezeArrows[n].meltTime2 = 0;
				}
			}

			// did it completely melt?
			if ( gs.player[p].freezeArrows[n].meltTime1 > HOLD_WINDOW || gs.player[p].freezeArrows[n].meltTime2 > HOLD_WINDOW )
			{
				gs.player[p].freezeArrows[n].judgement = NG;
				scoreNote(p, NG, gs.player[p].freezeArrows[n].columns[0]);
				gs.player[p].freezeArrows[n].isHeld = 0;

				gs.player[p].laneFlareColors[col1] = 0;
				if ( col2 != -1 )
				{
					gs.player[p].laneFlareColors[col2] = 0;
				}
			}
		}

		// next, is this hold completed OK?
		if ( gs.player[p].freezeArrows[n].endTime1 <= gs.player[p].timeElapsed && ( gs.player[p].freezeArrows[n].endTime2 == -1 || gs.player[p].freezeArrows[n].endTime2 <= gs.player[p].timeElapsed ) )
		{
			// but for really short hold notes, if you never even began to hold them, then they're NG
			if ( gs.player[p].freezeArrows[n].isHeld == 0 )
			{
				gs.player[p].freezeArrows[n].judgement = NG;
				scoreNote(p, NG, gs.player[p].freezeArrows[n].columns[0]);
			}
			else
			{
				gs.player[p].freezeArrows[n].judgement = OK;
				scoreNote(p, OK, gs.player[p].freezeArrows[n].columns[0]);

				gs.player[p].laneFlareTimers[col1] = HIT_FLASH_DISPLAY_TIME;
				gs.player[p].laneFlareColors[col1] = 4;
				if ( col2 != -1 )
				{
					gs.player[p].laneFlareTimers[col2] = HIT_FLASH_DISPLAY_TIME;
					gs.player[p].laneFlareColors[col2] = 4;
				}
			}
			gs.player[p].freezeArrows[n].isHeld = 0;	
		}

		// finally, are we done looping through freeze arrows in the early+late window?
		if ( gs.player[p].freezeArrows[n].startTime >= gs.player[p].timeElapsed + EARLY_GOOD )
		{
			break;
		}
		n++;
	}
}

int findNextNoteLate(int p, int column)
{
	unsigned int n = gs.player[p].currentNote - 1;
	UTIME endTime = gs.player[p].timeElapsed - LATE_GOOD;

	if ( gs.player[p].currentNote == 0 || gs.player[p].timeElapsed < LATE_GOOD )
	{
		return -1; // prevent a crash
	}

	while ( n >= 0 && gs.player[p].currentChart[n].timing > endTime )
	{
		if ( ISNOTE(gs.player[p].currentChart[n].type) && gs.player[p].currentChart[n].judgement == UNSET )
		{
			if ( gs.player[p].currentChart[n].columns[0] == column || gs.player[p].currentChart[n].columns[1] == column )
			{
				return n;
			}
		}
		if ( gs.player[p].currentChart[n].type == SHOCK )
		{
			return n;
		}

		if ( n == 0 )
		{
			break;
		}
		else
		{
			n--;
		}
	}
	return -1;
}

int findNextNoteEarly(int p, int column)
{
	unsigned int n = gs.player[p].currentNote;
	UTIME endTime = gs.player[p].timeElapsed + EARLY_GOOD;

	while ( n < (int)gs.player[p].currentChart.size() && gs.player[p].currentChart[n].timing < endTime )
	{
		if ( ISNOTE(gs.player[p].currentChart[n].type) && gs.player[p].currentChart[n].judgement == UNSET )
		{
			if ( gs.player[p].currentChart[n].columns[0] == column || gs.player[p].currentChart[n].columns[1] == column )
			{
				return n;
			}
		}
		if ( gs.player[p].currentChart[n].type == SHOCK )
		{
			return n;
		}
		n++;
	}
	return -1;
}

void finalizeCurrentSongStats(int p)
{
	SONG_RECORD& rec = sm.player[p].currentSet[gs.currentStage];
	int n = (int)gs.player[p].noteDiffs.size();
	if ( n == 0 )
	{
		return;
	}
	rec.avgDiff = 0;
	rec.unstableRate = 0;

	long sum = 0;
	for ( int i = 0; i < n; i++ ) sum += gs.player[p].noteDiffs[i];
	double mean = (double)sum / n;

	double variance = 0.0;
	for ( int i = 0; i < n; i++ )
	{
		double d = gs.player[p].noteDiffs[i] - mean;
		variance += d * d;
	}
	double stddev = sqrt(variance / n);
	rec.unstableRate = stddev * 10.0;

	double trimLow  = mean - 2.0 * stddev;
	double trimHigh = mean + 2.0 * stddev;
	long   trimSum  = 0;
	int    trimN    = 0;
	for ( int i = 0; i < n; i++ )
	{
		if ( gs.player[p].noteDiffs[i] >= trimLow && gs.player[p].noteDiffs[i] <= trimHigh )
		{
			trimSum += gs.player[p].noteDiffs[i];
			trimN++;
		}
	}
	rec.avgDiff = (trimN > 0) ? (double)trimSum / trimN : mean;
	gs.player[p].noteDiffs.clear();
}

void scoreNote(int p, int judgement, int column)
{
	// set the judgement display
	if ( (gs.player[p].drummaniaMode || judgement == OK || judgement == NG) && column != -1 )
	{
		gs.player[p].columnJudgement[column] = judgement;
		gs.player[p].columnJudgeTime[column] = 0;
	}
	else
	{
		gs.player[p].lastJudgement = judgement;
		gs.player[p].judgementTime = 0;
	}

	// tally, early/late split, and diff recording
	{
		SONG_RECORD& rec = sm.player[p].currentSet[gs.currentStage];
		bool isEarly = gs.player[p].lastJudgementEarly;
		long signedDiff = isEarly ? (long)gs.player[p].lastJudgementDiff : -(long)gs.player[p].lastJudgementDiff;
		switch (judgement)
		{
		case MARVELLOUS:
			rec.perfects++;
			isEarly ? rec.earlyMarvellous++ : rec.lateMarvellous++;
			gs.player[p].noteDiffs.push_back(signedDiff);
			break;
		case PERFECT:
			rec.perfects++;
			isEarly ? rec.earlyPerfect++ : rec.latePerfect++;
			gs.player[p].noteDiffs.push_back(signedDiff);
			break;
		case GREAT:
			rec.greats++;
			isEarly ? rec.earlyGreat++ : rec.lateGreat++;
			gs.player[p].noteDiffs.push_back(signedDiff);
			break;
		case GOOD:
			rec.goods++;
			isEarly ? rec.earlyGood++ : rec.lateGood++;
			gs.player[p].noteDiffs.push_back(signedDiff);
			break;
		case OK:
			rec.perfects++;
			break;
		case MISS:
		case NG:
			rec.misses++;
			break;
		default:
			TRACE("Strange judgement encountered: %d", judgement);
		}
	}

	sm.player[p].currentSet[gs.currentStage].calculateGrade();
	sm.player[p].currentSet[gs.currentStage].calculatePoints();
	sm.player[p].currentSet[gs.currentStage].calculateEXGrade();
	gs.player[p].lifebarPercent = sm.player[p].currentSet[gs.currentStage].getScore()/1000; // yes really

	// how does this judgement affect the combo?
	if ( judgement == MISS || judgement == NG )
	{
		if ( gs.player[p].displayCombo >= 500 )
		{
			em.announcerQuip(126); // ouch! your combos end here
			announcerLastCheckTotal = 0;
		}
		gs.player[p].displayCombo = 0;
		gs.player[p].currentSongCombo = 0;
		gs.player[p].comboColor = COMBO_MISS;
		announcerMinusPoints++;
		if ( gs.player[p].useBattery && gs.player[p].lifebarLives > 0 )
		{
			SUBTRACT_TO_ZERO(gs.player[p].lifebarLives, 1);
			em.playSample(SFX_BATTERY_ZAP);
		}
	}
	if ( judgement == MARVELLOUS || judgement == PERFECT || judgement == GREAT || judgement == GOOD )
	{
		gs.player[p].currentSongCombo++;
		gs.player[p].displayCombo++;
		announcerPlusPoints++;

		// what happens to the drummania combo animation?
		gs.player[p].drummaniaCombo[0] = DRUMMANIA_COMBO_BOUNCE_TIME;
		if ( gs.player[p].displayCombo % 10 == 0 )
		{
			gs.player[p].drummaniaCombo[1] = DRUMMANIA_COMBO_BOUNCE_TIME;
		}
		if ( gs.player[p].displayCombo % 100 == 0 )
		{
			gs.player[p].drummaniaCombo[2] = DRUMMANIA_COMBO_BOUNCE_TIME;
			em.announceCombo(gs.player[p].displayCombo);
			announcerLastCheckTotal = announcerPlusPoints = announcerMinusPoints = 0;
		}
		if ( gs.player[p].displayCombo % 1000 == 0 )
		{
			gs.player[p].drummaniaCombo[3] = DRUMMANIA_COMBO_BOUNCE_TIME;
		}
	}

	// update the max combo (NOTE: this needs to be tracked different from the display combo)
	if ( gs.player[p].currentSongMaxCombo < gs.player[p].currentSongCombo )
	{
		gs.player[p].currentSongMaxCombo = gs.player[p].currentSongCombo;
	}
	if ( sm.player[p].currentSet[gs.currentStage].maxCombo < gs.player[p].currentSongMaxCombo )
	{
		sm.player[p].currentSet[gs.currentStage].maxCombo = gs.player[p].currentSongMaxCombo;
	}

	// set the combo color
	if ( judgement == MARVELLOUS && gs.player[p].displayCombo == 1 )
	{
		gs.player[p].comboColor = COMBO_PERFECT;
	}
	if ( judgement == PERFECT && (gs.player[p].comboColor == COMBO_MARVELOUS || gs.player[p].displayCombo == 1) )
	{
		gs.player[p].comboColor = COMBO_PERFECT;
	}
	if ( judgement == GREAT && (gs.player[p].comboColor == COMBO_MARVELOUS || gs.player[p].comboColor == COMBO_PERFECT || gs.player[p].displayCombo == 1) )
	{
		gs.player[p].comboColor = COMBO_GREAT;
	}
	if ( judgement == GOOD && (gs.player[p].comboColor == COMBO_MARVELOUS || gs.player[p].comboColor == COMBO_PERFECT || gs.player[p].comboColor == COMBO_GREAT || gs.player[p].displayCombo == 1) )
	{
		gs.player[p].comboColor = COMBO_GOOD;
	}

	// did a full combo happen? start the animation
	int totalSteps = sm.player[p].currentSet[rememberedCurrentStage].perfects + sm.player[p].currentSet[rememberedCurrentStage].greats + sm.player[p].currentSet[rememberedCurrentStage].goods;
	if ( totalSteps == sm.player[p].currentSet[rememberedCurrentStage].maxPoints/2 ) // works for holds, since OK == perfect and NG == miss
	{
		em.playSample(SFX_FULL_COMBO_SPLASH);
		fullComboAnimStep = 1;
		fullComboAnimTimer = 0;

		int comboType = 0;
		if ( sm.player[p].currentSet[rememberedCurrentStage].goods == 0 )
		{
			comboType = 1;
		}
		if ( sm.player[p].currentSet[rememberedCurrentStage].goods == 0 && sm.player[p].currentSet[rememberedCurrentStage].greats == 0 )
		{
			comboType = 2;
		}

		if ( p == 0 )
		{
			fullComboP1 = true;
			fullComboPerfectP1 = comboType == 2;
			createFullComboParticles(0, comboType);
		}
		else
		{
			fullComboP2 = true;
			fullComboPerfectP2 = comboType == 2;
			createFullComboParticles(1, comboType);
		}

		if ( fullComboPerfectP1 || fullComboPerfectP2 )
		{
			em.announcerQuip(129); // perfect
			em.playSample(9);
		}
	}
}

void loadNextSong()
{
	int p1maxscore = 0, p2maxscore = 0;

	int (*readChart)(std::vector<struct ARROW> *chart, std::vector<struct FREEZE> *holds, int songID, int chartType) = &readDWI;
	int (*readChart2P)(std::vector<struct ARROW> *chart, std::vector<struct FREEZE> *holds, int songID, int chartType) = &readDWI2P;
	int (*readChartCenter)(std::vector<struct ARROW> *chart, std::vector<struct FREEZE> *holds, int songID, int chartType) = &readDWICenter;

	// For original songs, use xsq files. Else use dwi files.
	if ( doesExistXSQ(gs.player[0].stagesPlayed[gs.currentStage]) )
	{
		readChart = &readXSQ;
		readChart2P = &readXSQ2P;
		readChartCenter = &readXSQCenter;
	}

	if ( gs.isVersus )
	{
		p1maxscore = readChart(&gs.player[0].currentChart, &gs.player[0].freezeArrows, gs.player[0].stagesPlayed[gs.currentStage], gs.player[0].stagesLevels[gs.currentStage]);
		p2maxscore = readChart2P(&gs.player[1].currentChart, &gs.player[1].freezeArrows, gs.player[1].stagesPlayed[gs.currentStage], gs.player[1].stagesLevels[gs.currentStage]);
	}
	else if ( gs.isSingles() )
	{
		if ( gs.player[0].centerLeft )
		{
			p1maxscore = readChart(&gs.player[0].currentChart, &gs.player[0].freezeArrows, gs.player[0].stagesPlayed[gs.currentStage], gs.player[0].stagesLevels[gs.currentStage]);
		}
		else if ( gs.player[0].centerRight )
		{
			p1maxscore = readChart2P(&gs.player[0].currentChart, &gs.player[0].freezeArrows, gs.player[0].stagesPlayed[gs.currentStage], gs.player[0].stagesLevels[gs.currentStage]);
		}
		else
		{
			p1maxscore = readChartCenter(&gs.player[0].currentChart, &gs.player[0].freezeArrows, gs.player[0].stagesPlayed[gs.currentStage], gs.player[0].stagesLevels[gs.currentStage]);
		}
	}
	else
	{
		p1maxscore = readChart(&gs.player[0].currentChart, &gs.player[0].freezeArrows, gs.player[0].stagesPlayed[gs.currentStage], gs.player[0].stagesLevels[gs.currentStage]);
	}

	// versus co-random: if both players share the same random mod, seed rand() identically
	// so both charts receive the same random arrangement before other mods are applied
	unsigned int sharedChartModSeed = (unsigned int)rand();
	bool useSharedChartModSeed = gs.isVersus
		&& gs.player[0].chartMod == gs.player[1].chartMod
		&& gs.player[0].chartMod >= 1
		&& gs.player[0].chartMod <= 3; // only random-family mods (not Inverted, which is deterministic)

	for ( int p = 0; p < (gs.isVersus ? 2 : 1); p++ )
	{
		gs.player[p].nextStage();

		// capture initial BPM for fixed scroll mode pps ratio, and pre-set scrollRate so the
		// first render uses the correct pps regardless of the previous song's ending BPM.
		gs.player[p].baseBPM = 0;
		for ( int i = 0; i < (int)gs.player[p].currentChart.size(); i++ )
		{
			if ( gs.player[p].currentChart[i].type == BPM_CHANGE )
			{
				gs.player[p].baseBPM     = gs.player[p].currentChart[i].color;
				gs.player[p].scrollRate    = gs.player[p].baseBPM;
				gs.player[p].newScrollRate = gs.player[p].baseBPM;
				break;
			}
		}
		al_trace("loadNextSong p%d: songID=%d baseBPM=%d scrollRate=%d newScrollRate=%d speedMod=%d scrollMode=%d fixedScrollPPS=%d\r\n",
			p, gs.player[p].stagesPlayed[gs.currentStage],
			gs.player[p].baseBPM, gs.player[p].scrollRate, gs.player[p].newScrollRate,
			gs.player[p].speedMod, gs.player[p].scrollMode, gs.player[p].fixedScrollPPS);

		sm.player[p].currentSet[gs.currentStage].resetData();
		sm.player[p].currentSet[gs.currentStage].time = time(NULL);
		sm.player[p].currentSet[gs.currentStage].status = STATUS_NONE;
		sm.player[p].currentSet[gs.currentStage].songID = gs.player[p].stagesPlayed[gs.currentStage];
		sm.player[p].currentSet[gs.currentStage].chartID = gs.player[p].stagesLevels[gs.currentStage];
		sm.player[p].currentSet[gs.currentStage].grade = GRADE_NONE;
		sm.player[p].currentSet[gs.currentStage].maxPoints = p == 0 ? p1maxscore : p2maxscore;
		if ( gs.currentStage != 0 )
		{
			sm.player[p].currentSet[gs.currentStage - 1].calculateStatus();
		}

		bool isCenter    = gs.isSingles() && gs.player[0].isCenter();
		bool isRightSide = (gs.isVersus && p == 1) || (gs.isSingles() && gs.player[0].centerRight);
		if ( gs.player[p].chartMod > 0 )
		{
			if ( useSharedChartModSeed ) { srand(sharedChartModSeed); }
			applyChartMod(&gs.player[p].currentChart, &gs.player[p].freezeArrows, gs.player[p].chartMod, gs.isDoubles, isCenter, isRightSide);
		}
		if ( gs.player[p].arrangeModifier > 0 )
		{
			arrangeChart(&gs.player[p].currentChart, &gs.player[p].freezeArrows, gs.player[p].arrangeModifier, gs.isDoubles, isCenter);
		}
	}
	
	// check for alternate versions of songs? (used for maniax charts)
	bool useAlternateMusic = false;
	if ( (songs[songID_to_listID(gs.player[0].stagesPlayed[gs.currentStage])].specialFlag & SPECIAL_FLAG_HAS_ALTERNATE_MUSIC) != 0 )
	{
		if ( gs.player[0].stagesLevels[gs.currentStage] == SINGLE_ANOTHER || gs.player[0].stagesLevels[gs.currentStage] == DOUBLE_ANOTHER || (gs.isVersus && gs.player[1].stagesLevels[gs.currentStage] == SINGLE_ANOTHER) )
		{
			useAlternateMusic = true;
		}
	}

	gs.loadSong(gs.player[0].stagesPlayed[gs.currentStage], false, useAlternateMusic);
	vm.loadScript(movieScripts[songID_to_listID(gs.player[0].stagesPlayed[gs.currentStage])].c_str()); // I love the "])]" on this line!!!
	gs.playSong();
	gs.bgmSyncAnchored    = false;
	gs.bgmAnchorWall      = 0;
	gs.bgmAnchorFmodMs    = 0;
	gs.bgmLastChunkCount  = 0;
	g_dspLastChunkWall    = 0; // reset so a stale value from the previous song is not used
	vm.play();
	isMidTransition = true;
	songTransitionTime = BANNER_ANIM_LENGTH;

	// implement hazard mode
	for ( int i = 0; i < 2; i++ )
	{
		if ( gs.player[i].useHazard )
		{
			gs.player[i].useBattery = true;
			gs.player[i].lifebarLives = 1;
		}
	}

	// extra stage battery: Classic and Bonus use 4-miss battery, applied here so it doesn't show during song 3 results
	if ( gs.currentStage == gs.numSongsPerSet && (gs.extraTrackMode == 0 || gs.extraTrackMode == 1) )
	{
		for ( int i = 0; i < 2; i++ )
		{
			gs.player[i].useBattery = true;
			gs.player[i].lifebarLives = 4;
		}
	}

	announcerTargetSpeak = (p1maxscore + p2maxscore)/7; // speak approximately 7 times per song (although it is both random and dependant on other factors)
}

static void shuffleInts(int* arr, int count)
{
	for ( int i = 0; i < count - 1; i++ )
	{
		int j = i + rand() % (count - i);
		int tmp = arr[i]; arr[i] = arr[j]; arr[j] = tmp;
	}
}

static void applyColumnRandom(std::vector<struct ARROW>* chart, std::vector<struct FREEZE>* holds, bool isDoubles, bool isDRandom, bool isCenter, bool isRightSide)
{
	int perm[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };

	if ( isDoubles )
	{
		if ( isDRandom )
		{
			shuffleInts(perm + 2, 4); // scramble center 4 first so they can cross sides
			shuffleInts(perm, 4);     // then scramble each half independently
			shuffleInts(perm + 4, 4);
		}
		else { shuffleInts(perm, 4); shuffleInts(perm + 4, 4); }
	}
	else if ( isCenter )
	{
		shuffleInts(perm + 2, 4); // active columns are 2-5
	}
	else if ( isRightSide )
	{
		shuffleInts(perm + 4, 4); // active columns are 4-7
	}
	else
	{
		shuffleInts(perm, 4); // active columns are 0-3
	}

	for ( std::vector<struct ARROW>::iterator c = chart->begin(); c != chart->end(); c++ )
	{
		for ( int i = 0; i < 4; i++ )
		{
			if ( c->columns[i] >= 0 && c->columns[i] <= 7 )
				c->columns[i] = (char)perm[c->columns[i]];
		}
	}

	for ( std::vector<struct FREEZE>::iterator h = holds->begin(); h != holds->end(); h++ )
	{
		for ( int i = 0; i < 2; i++ )
		{
			if ( h->columns[i] >= 0 && h->columns[i] <= 7 )
				h->columns[i] = (char)perm[h->columns[i]];
		}
	}
}

static void applySRandom(std::vector<struct ARROW>* chart, std::vector<struct FREEZE>* holds, bool isDoubles, bool isCenter, bool isRightSide)
{
	int poolSize = isDoubles ? 8 : 4;
	int i = 0;

	while ( i < (int)chart->size() )
	{
		UTIME t = (*chart)[i].timing;

		// collect all active column slots sharing this timing
		int refArrows[32];
		int refSlots[32];
		int oldCols[32];
		int refCount = 0;

		int j = i;
		while ( j < (int)chart->size() && (*chart)[j].timing == t )
		{
			for ( int s = 0; s < 4; s++ )
			{
				if ( (*chart)[j].columns[s] >= 0 && (*chart)[j].columns[s] <= 7 && refCount < 32 )
				{
					refArrows[refCount] = j;
					refSlots[refCount]  = s;
					oldCols[refCount]   = (*chart)[j].columns[s];
					refCount++;
				}
			}
			j++;
		}

		if ( refCount > 0 )
		{
			int poolBase = isCenter ? 2 : (isRightSide ? 4 : 0);
			int pool[8];
			for ( int k = 0; k < poolSize; k++ ) pool[k] = poolBase + k;
			int n = refCount < poolSize ? refCount : poolSize;
			for ( int k = 0; k < n; k++ )
			{
				int r = k + rand() % (poolSize - k);
				int tmp = pool[k]; pool[k] = pool[r]; pool[r] = tmp;
			}
			for ( int k = 0; k < n; k++ )
			{
				(*chart)[refArrows[k]].columns[refSlots[k]] = (char)pool[k];
			}

			// build old->new mapping and apply to freeze arrows that start at this timing
			int remap[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
			for ( int k = 0; k < n; k++ )
				remap[oldCols[k]] = pool[k];

			for ( std::vector<struct FREEZE>::iterator h = holds->begin(); h != holds->end(); h++ )
			{
				if ( h->startTime == t )
				{
					for ( int ci = 0; ci < 2; ci++ )
					{
						if ( h->columns[ci] >= 0 && h->columns[ci] <= 7 )
							h->columns[ci] = (char)remap[h->columns[ci]];
					}
				}
			}
		}

		i = j;
	}
}

static void applyInverted(std::vector<struct ARROW>* chart, std::vector<struct FREEZE>* holds, bool isDoubles, bool isCenter, bool isRightSide)
{
	// left singles: 0<->1, 2<->3; center: 2<->3, 4<->5; right singles: 4<->5, 6<->7; doubles: all pairs
	static const char leftInvert[8]    = { 1, 0, 3, 2, 4, 5, 6, 7 };
	static const char centerInvert[8]  = { 0, 1, 3, 2, 5, 4, 6, 7 };
	static const char rightInvert[8]   = { 0, 1, 2, 3, 5, 4, 7, 6 };
	static const char doublesInvert[8] = { 1, 0, 3, 2, 5, 4, 7, 6 };
	const char* inv = isDoubles ? doublesInvert : (isCenter ? centerInvert : (isRightSide ? rightInvert : leftInvert));

	for ( std::vector<struct ARROW>::iterator c = chart->begin(); c != chart->end(); c++ )
	{
		for ( int i = 0; i < 4; i++ )
		{
			if ( c->columns[i] >= 0 && c->columns[i] <= 7 )
				c->columns[i] = inv[c->columns[i]];
		}
	}

	for ( std::vector<struct FREEZE>::iterator h = holds->begin(); h != holds->end(); h++ )
	{
		for ( int i = 0; i < 2; i++ )
		{
			if ( h->columns[i] >= 0 && h->columns[i] <= 7 )
				h->columns[i] = inv[h->columns[i]];
		}
	}
}

// mod: 1=Random, 2=S-Random, 3=D-Random (all 8 cols; acts as Random in singles), 4=Inverted
void applyChartMod(std::vector<struct ARROW>* chart, std::vector<struct FREEZE>* holds, int mod, bool isDoubles, bool isCenter, bool isRightSide)
{
	switch ( mod )
	{
	case 1: applyColumnRandom(chart, holds, isDoubles, false, isCenter, isRightSide); break;
	case 2: applySRandom(chart, holds, isDoubles, isCenter, isRightSide);             break;
	case 3: applyColumnRandom(chart, holds, isDoubles, true,  isCenter, isRightSide); break;
	case 4: applyInverted(chart, holds, isDoubles, isCenter, isRightSide);            break;
	}
}

// chart - list of tap notes
// holds - list of hold notes
// type     - 0 = no change, 1 = mirror (horizontal), 2 = upside down (v-mirror), 3 = shuffle
// isDoubles - matters for some types
// isCenter  - true when playing singles in center position (cols 2-5 active)
void arrangeChart(std::vector<struct ARROW> *chart, std::vector<struct FREEZE> *holds, char type, bool isDoubles, bool isCenter)
{
	char arrangeMatrix[4][3][8] = {
		{ {0,1,2,3,4,5,6,7}, {0,1,2,3,4,5,6,7}, {0,1,2,3,4,5,6,7} }, // original chart
		{ {3,2,1,0,7,6,5,4}, {7,6,5,4,3,2,1,0}, {0,1,5,4,3,2,7,6} }, // mirror (center: swap 2<->5, 3<->4)
		{ {1,0,3,2,5,4,7,6}, {1,0,3,2,5,4,7,6}, {1,0,3,2,5,4,7,6} }, // upside down
		{ {2,1,0,3,6,5,4,7}, {0,1,2,3,4,5,6,7}, {0,1,2,3,4,5,6,7} }, // one shuffle pattern
	};
	int mode = isCenter ? 2 : (isDoubles ? 1 : 0);

	for ( std::vector<struct ARROW>::iterator c = chart->begin(); c != chart->end(); c++ )
	{
		for ( int i = 0; i < 4; i++ )
		{
			if ( c->columns[i] >= 0 && c->columns[i] <= 7 ) // -1 means no note here (very important for triples
			{
				c->columns[i] = arrangeMatrix[type][mode][c->columns[i]];
			}
		}
	}

	UNUSED(holds); // modifying the head of the freezes automatically moves the holds
}

int checkForExtraStages()
{
	bool awardedExtra = gs.player[0].stagesPlayed[gs.numSongsPerSet] > 0; // extra stage was already awarded
	bool awardedEncore = gs.player[0].stagesPlayed[gs.numSongsPerSet+1] > 0; // encore stage was already awarded

	int sumP1 = 0, sumP2 = 0;
	int levelP1 = 0, levelP2 = 0;
	int songToPlay = 126;

	// calculate total score and average difficulty level
	for ( int i = 0; i < gs.numSongsPerSet; i++ )
	{
		sumP1 += sm.player[0].currentSet[i].getScore();
		sumP2 += sm.player[1].currentSet[i].getScore();
		levelP1 += sm.player[0].currentSet[i].chartID;
		levelP2 += sm.player[1].currentSet[i].chartID;
	}
	sumP1 /= gs.numSongsPerSet;
	sumP2 /= gs.numSongsPerSet;
	levelP1 /= gs.numSongsPerSet;
	levelP2 /= gs.numSongsPerSet;
	if ( levelP1 == SINGLE_ANOTHER || levelP1 == DOUBLE_ANOTHER )
	{
		levelP1 -= 1;
	}
	if ( levelP2 == SINGLE_ANOTHER )
	{
		levelP2 -= 1;
	}

	if ( !awardedExtra )
	{
		if ( gs.extraTrackMode == 3 )
			return 0; // Disabled: no extra stage

		// basic extra stage requirement: are your combined scores at least 900,000 on average?
		if ( sumP1 >= 900000 || sumP2 >= 900000 )
		{
			awardedExtra = true;

			// count the total number of full combos across all songs - unused
			//int numFullComboP1 = sm.player[0].getNumStars(levelP1, STATUS_FULL_GOOD_COMBO);
			//int numFullComboP2 = sm.player[1].getNumStars(levelP2, STATUS_FULL_GOOD_COMBO);

			// count the number of full combos specifically on EXTRA STAGE songs - unused
			/*
			static int extraStages[10] = { 126, 230, 245, 246, 247, 248, 249, 250, 303, 324 };
			int numSpecialP1 = 0;
			int numSpecialP2 = 0;
			for ( int i = 0; i < 10; i++ )
			{
				if ( sm.player[0].getStatusOnSong(extraStages[i], levelP1) >= STATUS_FULL_GOOD_COMBO )
				{
					numSpecialP1++;
				}
				if ( sm.player[1].getStatusOnSong(extraStages[i], levelP2) >= STATUS_FULL_GOOD_COMBO )
				{
					numSpecialP2++;
				}
			}
			//*/

			// check for a full combo with miss count == 0
			int numMissesP1 = 0;
			int numMissesP2 = 0;
			for ( int i = 0; i < gs.numSongsPerSet; i++ )
			{
				numMissesP1 += sm.player[0].currentSet[i].misses;
				numMissesP2 += sm.player[1].currentSet[i].misses;
			}

			// give them megamix 1 for sure if they picked 3 songs from 1st mix
			if ( gs.player[0].pickedAllFromVersion(1) || (gs.isVersus && gs.player[1].pickedAllFromVersion(1)) )
			{
				songToPlay = 126;
			}
			// ... or give them megamix 2 for sure if they picked 3 songs from second mix or j-append
			else if ( gs.player[0].pickedAllFromVersion(2) || (gs.isVersus && gs.player[1].pickedAllFromVersion(2)) )
			{
				songToPlay = 230;
				
				// eh... they've covered this one. Maybe give them an edit instead
				if ( sm.player[0].getStatusOnSong(230, levelP1) >= STATUS_FULL_GOOD_COMBO || sm.player[1].getStatusOnSong(230, levelP2) >= STATUS_FULL_GOOD_COMBO )
				{
					songToPlay = pickRandomInt(7, 230, 245, 246, 247, 248, 249, 250);
				}
			}
			// ... or give the player a song that they haven't unlocked yet if they picked form mixed versions
			else
			{
//#define NUM_OLD_EXTRA_STAGES 2
#define NUM_SPECIAL_EXTRA_STAGES 3
				//int oldExtraStages[NUM_OLD_EXTRA_STAGES] = { 126, 230, 245, 246, 247, 248, 249, 250 }; // megamix 1, megamix 2, and the edits for megamix 2
				int premiumExtraStages[NUM_SPECIAL_EXTRA_STAGES] = { 303, 324, 357 }; // POSSESSION, Elemental Creation, MAX 300
				std::vector<int> usableExtraStages;

				/*
				// add the old extra stages to the pool if the player hasn't unlocked them yet
				for ( int i = 0; i < NUM_OLD_EXTRA_STAGES; i++ )
				{
					if ( !sm.player[0].isSongUnlockedForPlayer(oldExtraStages[i], levelP1) || (gs.isVersus && !sm.player[1].isSongUnlockedForPlayer(oldExtraStages[i], levelP2)) )
					{
						usableExtraStages.push_back(oldExtraStages[i]);
					}
				}
				*/

				// the player did exceptionally well if they averaged one miss per stage or averaged an S rank
				//if ( numMissesP1 < gs.numSongsPerSet || sumP1 >= 950000 || (gs.isVersus && numMissesP2 < gs.numSongsPerSet) || sumP2 >= 950000 )
				{
					for ( int i = 0; i < NUM_SPECIAL_EXTRA_STAGES; i++ )
					{
						if ( !sm.player[0].isSongUnlockedForPlayer(premiumExtraStages[i], levelP1) || (gs.isVersus && !sm.player[1].isSongUnlockedForPlayer(premiumExtraStages[i], levelP2)) )
						{
							usableExtraStages.push_back(premiumExtraStages[i]);
						}
					}
				}

				// is everything unlocked? well good job you win the prize. enjoy machine random
				if ( usableExtraStages.size() == 0 )
				{
					songToPlay = randomExtraStage; // from songwheel mode, picked randomly from songs that were shown last visit to songwheel mode
				}
				else
				{
					songToPlay = usableExtraStages[rand() % usableExtraStages.size()];
				}
			}

			// make it happen
			if ( gs.extraTrackMode == 0 ) // Classic: auto-select song
			{
				gs.player[0].stagesPlayed[gs.numSongsPerSet] = songToPlay;
				gs.player[0].stagesLevels[gs.numSongsPerSet] = levelP1;
				if ( gs.isVersus )
				{
					gs.player[1].stagesPlayed[gs.numSongsPerSet] = songToPlay;
					gs.player[1].stagesLevels[gs.numSongsPerSet] = levelP2;
				}
			}
			// Battery for Classic/Bonus applied in loadNextSong() when the extra stage slot loads

			em.announcerQuip( GUY_EARN_EXTRA );
		}
	}
	else if ( !awardedEncore )
	{
		/*
		// count the number of full combos
		int numFullComboP1 = sm.player[0].getNumStars(levelP1, STATUS_FULLCOMBO);
		int numFullComboP2 = sm.player[1].getNumStars(levelP2, STATUS_FULLCOMBO);

		// count the number of EXTRA STAGE full combos
		static int extraStages[8] = { 126, 230, 245, 246, 247, 248, 249, 250 };
		int numSpecialP1 = 0;
		int numSpecialP2 = 0;
		for ( int i = 0; i < 8; i++ )
		{
			if ( sm.player[0].getStatusOnSong(extraStages[i], levelP1) >= STATUS_FULLCOMBO )
			{
				numSpecialP1++;
			}
			if ( sm.player[1].getStatusOnSong(extraStages[i], levelP2) >= STATUS_FULLCOMBO )
			{
				numSpecialP2++;
			}
		}

		if ( )
		{
			em.announcerQuip( GUY_EARN_SPECIAL_EXTRA );
		}
		*/
	}

	// check for permanently unlocking an extra stage by scoring 90% while on the extra stage
	if ( awardedExtra )
	{
		int songIndex = songID_to_listID(gs.player[0].stagesPlayed[gs.numSongsPerSet]);
		if ( songIndex == -1 )
		{
			songIndexError(gs.player[0].stagesPlayed[gs.numSongsPerSet]);
		}
		if ( awardedExtra && songs[songIndex].specialFlag & SPECIAL_FLAG_UNLOCK_METHOD_EXTRA_STAGE )
		{
			if ( sm.player[0].currentSet[gs.numSongsPerSet].getScore() >= 900000 )
			{
				sm.player[0].currentSet[gs.numSongsPerSet].unlockStatus = 1;
			}
			if ( sm.player[1].currentSet[gs.numSongsPerSet].getScore() >= 900000 )
			{
				sm.player[0].currentSet[gs.numSongsPerSet].unlockStatus = 1;
			}
		}
	}

	return 0 + (awardedExtra) + (awardedEncore);
}

void generateTimingReport()
{
	std::vector<int> diffs;

	if ( gs.player[0].currentChart.size() != gs.player[1].currentChart.size() )
	{
		al_trace("generateTimingReport() failed, charts are different: %d %d\r\n", gs.player[0].currentChart.size(), gs.player[1].currentChart.size());
		return;
	}

	for ( unsigned int i = 0; i < gs.player[0].currentChart.size(); i++ )
	{
		int diff  = gs.player[0].currentChart[i].timing - gs.player[1].currentChart[i].timing;
		diffs.push_back(diff);
		if ( i > 0 )
		{
			al_trace("diff: %d (%d)\r\n", diff, diff - diffs[i-1]);
		}
	}

	al_trace("Report complete. %d/%d\r\n", diffs[1], diffs[diffs.size()-2]);
}