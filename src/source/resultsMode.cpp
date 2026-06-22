// resultsMode.cpp implements the main results loop for Dance Maniax
// source file created by Allen Seitz 2/29/2012

#include "common.h"

#include "../headers/gameStateManager.h"
#include "../headers/inputManager.h"
#include "../headers/lightsManager.h"
#include "../headers/scoreManager.h"
#include "../headers/songwheelMode.h"
#include "../headers/gameplayRendering.h"

extern RenderingManager rm;
extern GameStateManager gs;
extern ScoreManager     sm;
extern LightsManager	lm;
extern InputManager     im;
extern EffectsManager   em;
extern unsigned long int frameCounter;
extern unsigned long int totalGameTime;

extern UTIME timeRemaining;
extern void playTimeLowSFX(UTIME dt);

//////////////////////////////////////////////////////////////////////////////
// Graphics
//////////////////////////////////////////////////////////////////////////////
BITMAP* m_resultsBG = NULL;
BITMAP* m_resultTop = NULL;
BITMAP* m_resultBottom = NULL;
BITMAP* m_clearStatus = NULL;
BITMAP* m_resultSub = NULL;
BITMAP* m_pcStar = NULL;
BITMAP* m_marvLabel[3] = { NULL, NULL, NULL }; // tinted copies of PERFECT row: blue, red, green
extern BITMAP** m_banners;


//////////////////////////////////////////////////////////////////////////////
// Variables
//////////////////////////////////////////////////////////////////////////////
int resultFadeTimer = 0;
int secondAnimTimer = 0;
bool doneIntroAnim = false;
int currentPlayer = 0; // will be 1 if showing results for 2P
int scrollX = 64;
int originalX = 64;
int targetScrollX = -1;
UTIME scrollTweenTime = 0;
int stageLimit = 3;
bool isMidCreditResults = false;
int midCreditDisplayStage = 0;

#define INTRO_ANIM_LENGTH 1000


//////////////////////////////////////////////////////////////////////////////
// Functions
//////////////////////////////////////////////////////////////////////////////
void renderResult(int which, int x, int player);
// precondition: which is a stage number, player is 0 or 1
// postcondition: renders graphics to the backbuf

void renderResultAdvanced(int which, int player);
// precondition: player's scoreMode == 1, isMidCreditResults is true, not versus
// postcondition: renders the advanced judgement breakdown centered on screen

void firstResultsLoop()
{
	// load assets
	if ( m_resultsBG == NULL )
	{
		m_resultsBG = loadImage("DATA/results/results.tga");
		m_resultTop = loadImage("DATA/results/result_high.tga");
		m_resultBottom = loadImage("DATA/results/result_low.tga");
		m_clearStatus = loadImage("DATA/results/clear_status.tga");
		m_resultSub = loadImage("DATA/results/result_sub.tga");
		m_pcStar = loadImage("DATA/results/pc_star.tga");

		// create 3 tinted copies of the PERFECT row for the MARVELOUS cycling display
		// order matches bold font palette: blue, red, green
		for ( int i = 0; i < 3; i++ )
		{
			m_marvLabel[i] = create_bitmap(192, 32);
			blit(m_resultSub, m_marvLabel[i], 0, 0, 0, 0, 192, 32);
		}
		tintFillBitmap(m_marvLabel[0], 170, 170, 255); // blue
		tintFillBitmap(m_marvLabel[1], 255, 170, 170); // red
		tintFillBitmap(m_marvLabel[2], 170, 255, 170); // green
	}

	resultFadeTimer = secondAnimTimer = 0;
	doneIntroAnim = false;
	currentPlayer = 0;
	scrollX = originalX = 740; // offscreen, gotta scroll in
	targetScrollX = 64;
	scrollTweenTime = 0;
	blit(rm.m_backbuf, rm.m_backbuf1, 0, 0, 0, 0, 640, 480); // prepare for the animation
	blit(rm.m_backbuf, rm.m_backbuf2, 0, 0, 0, 0, 640, 480);

	if ( gs.returningToSongwheel )
	{
		// mid-credit: show only the stage that was just played
		isMidCreditResults = true;
		midCreditDisplayStage = gs.currentStage - 1;
	}
	else
	{
		// final results: show all stages that were played
		isMidCreditResults = false;
		midCreditDisplayStage = 0;
		for ( int i = 0; i < MAX_SONGS_PER_SET; i++ )
		{
			if ( sm.player[currentPlayer].currentSet[i].songID < 100 )
			{
				break;
			}
			stageLimit = i-1;
		}
	}

	em.playSample(SFX_RESULTS_APPEAR);
	timeRemaining = 20000;
	lm.loadLampProgram("results.txt");

	im.setCooldownTime(0);
}

void mainResultsLoop(UTIME dt)
{
	resultFadeTimer += dt;
	secondAnimTimer = (secondAnimTimer + dt) % 750;
	if ( !gs.isEventMode && !gs.isFreestyleMode )
	{
		SUBTRACT_TO_ZERO(timeRemaining, dt);
	}
	playTimeLowSFX(dt);

	// intro animation?
	if ( resultFadeTimer >= INTRO_ANIM_LENGTH && !doneIntroAnim )
	{
		resultFadeTimer = 0;
		doneIntroAnim = true;
		if ( gs.currentSong != BGM_RESULT )
		{
			gs.loadSong(BGM_RESULT);
			gs.playSong();
		}

		// the announcer has a few words for you
		int averageScore = 0;
		int numStages = 0;
		for ( int i = 0; i < MAX_SONGS_PER_SET; i++ )
		{
			if ( sm.player[currentPlayer].currentSet[i].songID >= 100 )
			{
				averageScore += sm.player[currentPlayer].currentSet[i].getScore();
				numStages++;
			}
		}
		if ( averageScore > 0 && numStages > 0 )
		{
			averageScore /= numStages;
		}

		if ( averageScore >= 900000 )
		{
			em.announcerQuip(GUY_RESULT_S);
		}
		else if ( averageScore >= 800000 )
		{
			em.announcerQuip(GUY_RESULT_B);
		}
		else if ( averageScore >= 700000 )
		{
			em.announcerQuip(GUY_RESULT_C);
		}
		else if ( averageScore > 400000 )
		{
			em.announcerQuip(GUY_RESULT_D);
		}
		else
		{
			em.announcerQuip(GUY_RESULT_E);
		}
	}
	if ( !doneIntroAnim && resultFadeTimer < (INTRO_ANIM_LENGTH/2) )
	{
		rm.renderWipeAnim(getValueFromRange(0, 14, resultFadeTimer*100/(INTRO_ANIM_LENGTH/2)));
		return;
	}

	blit(m_resultsBG, rm.m_backbuf, 0, 0, 0, 0, 640, 480);
	rectfill(rm.m_backbuf, 0, 460, 640, 480, 0);

	// render the two words 'result' which fade in and out
	if ( resultFadeTimer > 2000 )
	{
		resultFadeTimer -= 2000;
	}
	int topAlpha = getValueFromRange(0, 255, resultFadeTimer * 100 / 1000);
	if ( resultFadeTimer > 1000 )
	{
		topAlpha = getValueFromRange(255, 0, (resultFadeTimer-1000) * 100 / 1000);
	}
	int bottomAlpha = 255 - topAlpha;

	set_trans_blender(0,0,0,topAlpha);
	draw_trans_sprite(rm.m_backbuf, m_resultTop, 216, 2);
	set_trans_blender(0,0,0,bottomAlpha);
	draw_trans_sprite(rm.m_backbuf, m_resultBottom, 0, 402);	
	set_alpha_blender(); // the game assumes the graphics are left in this mode

	if ( gs.isVersus && isMidCreditResults )
	{
		int p1Len = 0;
		while ( p1Len < 8 && sm.player[0].displayName[p1Len] != 0 ) p1Len++;
		renderNameString(sm.player[0].displayName, 160 - p1Len * 16, 72, 0);
		int p2Len = 0;
		while ( p2Len < 8 && sm.player[1].displayName[p2Len] != 0 ) p2Len++;
		renderNameString(sm.player[1].displayName, 480 - p2Len * 16, 72, 0);
	}
	else
	{
		int nameLen = 0;
		while ( nameLen < 8 && sm.player[currentPlayer].displayName[nameLen] != 0 ) nameLen++;
		renderNameString(sm.player[currentPlayer].displayName, (640 - nameLen * 32) / 2, 40, 0);
	}

	// render the song results
	scrollX = getValueFromRange(targetScrollX, originalX, scrollTweenTime*100/400 ); // quickly slide the results in from the right
	SUBTRACT_TO_ZERO(scrollTweenTime, dt);
	if ( isMidCreditResults )
	{
		if ( gs.isVersus )
		{
			renderResult(midCreditDisplayStage, 96, 0);
			renderResult(midCreditDisplayStage, 416, 1);
		}
		else
		{
			//renderResult(midCreditDisplayStage, scrollX, 0);
			renderResult(midCreditDisplayStage, 256, 0);
		}
	}
	else
	{
		for ( int i = 0; i < MAX_SONGS_PER_SET; i++ )
		{
			renderResult(i, scrollX + (192*i), currentPlayer);
		}
	}

	// render the rest of the intro animation
	if ( !doneIntroAnim && resultFadeTimer >= (INTRO_ANIM_LENGTH/2) )
	{
		int fadeTime = resultFadeTimer-(INTRO_ANIM_LENGTH/2);
		rm.dimScreen(getValueFromRange(100, 0, fadeTime*100/(INTRO_ANIM_LENGTH/2)));
	}

	// all done rendering
	if ( doneIntroAnim )
	{
		renderTimeRemaining(5, 36);
	}

	// check for input — no scrolling in mid-credit results (only one stage shown)
	if ( !isMidCreditResults )
	{
		if ( (im.getKeyState(MENU_LEFT_1P) == HELD_DOWN || im.getKeyState(MENU_LEFT_2P) == HELD_DOWN) && scrollTweenTime == 0 && scrollX < 64 )
		{
			scrollTweenTime = 400;
			targetScrollX += 192;
			originalX = scrollX;
			em.playSample(SFX_SONGWHEEL_MOVE);
		}
		if ( (im.getKeyState(MENU_RIGHT_1P) == HELD_DOWN || im.getKeyState(MENU_RIGHT_2P) == HELD_DOWN) && scrollTweenTime == 0 && scrollX > (stageLimit-2)*(-192) )
		{
			scrollTweenTime = 400;
			targetScrollX -= 192;
			originalX = scrollX;
			em.playSample(SFX_SONGWHEEL_MOVE);
		}
	}
	if ( im.getKeyState(MENU_START_1P) == JUST_DOWN || im.getKeyState(MENU_START_2P) == JUST_DOWN || timeRemaining <= 0 )
	{
		if ( gs.isVersus && !isMidCreditResults && currentPlayer == 0 )
		{
			firstResultsLoop(); // reboot the mode, lol
			currentPlayer = 1;
		}
		else
		{
			if ( gs.returningToSongwheel && gs.creditComplete )
			{
				// per-song result for the final song was shown; now show full credit results
				gs.returningToSongwheel = false;
				gs.creditComplete = false;
				gs.g_currentGameMode = RESULTS;
				gs.g_gameModeTransition = 1;
			}
			else if ( gs.returningToSongwheel )
			{
				if ( gs.isFreestyleMode && (sm.player[0].isLoggedIn || (gs.isVersus && sm.player[1].isLoggedIn)) )
				{
					sm.savePlayersToDisk();
				}
				gs.g_currentGameMode = SONGWHEEL;
			}
			else
			{
				if ( sm.player[0].isLoggedIn || (gs.isVersus && sm.player[1].isLoggedIn) )
				{
					gs.g_currentGameMode = GAMEOVER;
				}
				else
				{
					gs.g_currentGameMode = GAMEOVER;
				}
			}

			gs.g_gameModeTransition = 1;
		}
	}
}

void renderResult(int which, int x, int player)
{
	static int colors[3] = { makeacol(41, 239, 115, 255), makeacol(247, 41, 173, 255), makeacol(76, 0, 190, 255) };

	if ( sm.player[player].currentSet[which].songID < 100 )
	{
		//renderWhiteString("STAGE", x + 20, 100);
		//renderWhiteNumber(which, x, 100);
		return; // it's fine to call this function on every stage. It just won't do anything for the non-stages.
	}

	if ( sm.player[currentPlayer].scoreMode == 1 && sm.player[currentPlayer].useExpertMenu && isMidCreditResults && !gs.isVersus )
	{
		renderResultAdvanced(which, player);
		return;
	}

	// song title
	//renderBoldString(songTitles[songID_to_listID(sm.player[player].currentSet[which].songID)], x-32, 72, 192, false);
	//renderWhiteString(songTitles[songID_to_listID(sm.player[player].currentSet[which].songID)], x-32, 72);

	// status
	int frame = getValueFromRange(0, 10, secondAnimTimer * 100 / 750);
	int statusy = 73; // 357
	switch( sm.player[player].currentSet[which].status )
	{
	case STATUS_FULL_PERFECT_COMBO:
	case STATUS_FULL_GREAT_COMBO:
	case STATUS_FULL_GOOD_COMBO:
		masked_blit(m_clearStatus, rm.m_backbuf, 0, frame*32, x-32, statusy, 192, 32);
		break;
	case STATUS_FAILED:
		masked_blit(m_clearStatus, rm.m_backbuf, 0, 352, x-32, statusy, 192, 32);
		break;
	case STATUS_CLEARED:
		masked_blit(m_clearStatus, rm.m_backbuf, 0, 384, x-32, statusy, 192, 32);
		break;
	default:
		break;
	}

	// draw a box around the banner to indicate difficulty
	stretch_blit(m_banners[songID_to_listID(sm.player[player].currentSet[which].songID)], rm.m_backbuf, 0, 0, 256, 256, x, 100, 128, 128);
	int level = sm.player[player].currentSet[which].chartID % 10;
	rect(rm.m_backbuf, x, 100, x+128, 228, colors[level]);
	rect(rm.m_backbuf, x+1, 101, x+127, 227, colors[level]);

	// counts
	masked_blit(m_resultSub, rm.m_backbuf, 0, 0, x-32, 230, 192, 32);
	renderScoreNumber(sm.player[player].currentSet[which].perfects, x-32+108, 230-4, 3);
	if ( sm.player[player].currentSet[which].getScore() == 1000000 )
	{
		int star = getValueFromRange(0, 11, secondAnimTimer * 100 / 750);
		masked_blit(m_pcStar, rm.m_backbuf, (star/4)*64, (star%4)*64, x+32, 280, 64, 64);
	}
	else
	{
		masked_blit(m_resultSub, rm.m_backbuf, 0, 32, x-32, 260, 192, 32);
		renderScoreNumber(sm.player[player].currentSet[which].greats, x-32+108, 230+26, 3);

		masked_blit(m_resultSub, rm.m_backbuf, 0, 64, x-32, 290, 192, 32);
		renderScoreNumber(sm.player[player].currentSet[which].goods, x-32+108, 230+56, 3);

		masked_blit(m_resultSub, rm.m_backbuf, 0, 96, x-32, 320, 192, 32);
		renderScoreNumber(sm.player[player].currentSet[which].misses, x-32+108, 230+90, 3);
	}

	// score
	renderScoreNumber(sm.player[player].currentSet[which].getScore(), x-32+6, 357, 7);
}

void renderResultAdvanced(int which, int player)
{
	SONG_RECORD& rec = sm.player[player].currentSet[which];

	// album art — horizontally centered between left edge and text block, vertically centered on screen
	const int ART_X  = 52;                         // center of [0, LABEL_X=232] minus half art width
	const int ART_Y  = (SCREEN_HEIGHT - 128) / 2;  // = 176, center at 240
	const int ART_CY = ART_Y + 64;                 // = 240

	// judgement text block — right-aligned, vertically centered with album art
	// block: 5 rows × 38px spacing + 32px last row = 184px; BOX_WIDTH covers label→sec-value
	const int BOX_WIDTH    = 392;
	const int LABEL_X      = SCREEN_WIDTH - 16 - BOX_WIDTH;  // = 232
	const int COUNT_X      = LABEL_X + 116;
	const int SEC_LABEL_X  = LABEL_X + 232;
	const int SEC_VALUE_X  = LABEL_X + 302;
	const int BLOCK_HEIGHT = 4 * 38 + 32;                    // = 184
	const int Y_MARV  = ART_CY - BLOCK_HEIGHT / 2;  // = 148
	const int Y_PERF  = Y_MARV + 38;
	const int Y_GREAT = Y_MARV + 76;
	const int Y_GOOD  = Y_MARV + 114;
	const int Y_MISS  = Y_MARV + 152;

	// LINE2: gap between the two secondary lines (EARLY/LATE or AVG/UR) — 12px colored text
	// secondary block: spans Y_ROW to Y_ROW+32 (LINE2+12), center at Y_ROW+16
	// offsets derived from pixel-sampling the source bitmaps:
	//   PERF/GREAT/GOOD glyph: 12px, top_pad=8, cell center=+14 → LABEL_BMP_Y=+2 shifts center to Y_ROW+16
	//   MISS glyph:            12px, top_pad=12, cell center=+18 → MISS_BMP_Y=-2 shifts center to Y_ROW+16
	//   score digit:           24px, top_pad=6,  cell center=+18 → SCORE_Y_OFF=-2 shifts center to Y_ROW+16
	const int LINE2       = 20;
	const int LABEL_BMP_Y =  2;
	const int MISS_BMP_Y  = -2;
	const int SCORE_Y_OFF = -2;

	// status graphic (centered at x=256, same as existing renderResult at x=256)
	int frame = getValueFromRange(0, 10, secondAnimTimer * 100 / 750);
	switch ( rec.status )
	{
	case STATUS_FULL_PERFECT_COMBO:
	case STATUS_FULL_GREAT_COMBO:
	case STATUS_FULL_GOOD_COMBO:
		masked_blit(m_clearStatus, rm.m_backbuf, 0, frame*32, 224, 73, 192, 32);
		break;
	case STATUS_FAILED:
		masked_blit(m_clearStatus, rm.m_backbuf, 0, 352, 224, 73, 192, 32);
		break;
	case STATUS_CLEARED:
		masked_blit(m_clearStatus, rm.m_backbuf, 0, 384, 224, 73, 192, 32);
		break;
	}

	// song banner with difficulty border — left side, vertically centered
	stretch_blit(m_banners[songID_to_listID(rec.songID)], rm.m_backbuf, 0, 0, 256, 256, ART_X, ART_Y, 128, 128);
	static int diffColors[3] = { makeacol(41, 239, 115, 255), makeacol(247, 41, 173, 255), makeacol(76, 0, 190, 255) };
	int level = rec.chartID % 10;
	rect(rm.m_backbuf, ART_X,     ART_Y,     ART_X + 128, ART_Y + 128, diffColors[level]);
	rect(rm.m_backbuf, ART_X + 1, ART_Y + 1, ART_X + 127, ART_Y + 127, diffColors[level]);

	// derived counts from early/late trackers
	int marvCount  = rec.earlyMarvellous + rec.lateMarvellous;
	int perfCount  = rec.earlyPerfect    + rec.latePerfect;
	int greatCount = rec.earlyGreat      + rec.lateGreat;
	int goodCount  = rec.earlyGood       + rec.lateGood;

	char buf[32];

	// MARVELOUS — col1: tinted PERFECT sprite cycling blue/red/green every 100ms
	// %+.2f always emits a sign so '+' and '-' keep subsequent digits aligned
	int marvColor = (totalGameTime / 50) % 3;
	masked_blit(m_marvLabel[marvColor], rm.m_backbuf, 0, 0, LABEL_X - 16, Y_MARV + LABEL_BMP_Y, 192, 32);
	renderScoreNumber(marvCount,  COUNT_X, Y_MARV  + SCORE_Y_OFF, marvCount  >= 1000 ? 4 : 3);
	double exPct = rec.maxExScore > 0 ? (double)rec.exScore / rec.maxExScore * 100.0 : 0.0;
	static const int exMarvColors[3] = { TEXT_COLOR_RED, TEXT_COLOR_GREEN, TEXT_COLOR_BLUE };
	int labelColor = exPct >= 90.0 ? exMarvColors[(totalGameTime / 50) % 3] : TEXT_COLOR_GOLD;
	renderOutlinedColoredString("EXP:", SEC_LABEL_X, Y_MARV, labelColor);
	sprintf_s(buf, 32, "%d", rec.exScore);
	renderOutlinedColoredString(buf, SEC_VALUE_X, Y_MARV, TEXT_COLOR_GOLD);
	renderOutlinedColoredString("MAX:", SEC_LABEL_X, Y_MARV + LINE2, labelColor);
	sprintf_s(buf, 32, "%.2f%%", exPct);
	renderOutlinedColoredString(buf, SEC_VALUE_X, Y_MARV + LINE2, TEXT_COLOR_GOLD);

	// PERFECT
	masked_blit(m_resultSub, rm.m_backbuf, 0, 0,  LABEL_X - 16, Y_PERF  + LABEL_BMP_Y, 192, 32);
	renderScoreNumber(perfCount,  COUNT_X, Y_PERF  + SCORE_Y_OFF, perfCount  >= 1000 ? 4 : 3);
	renderOutlinedColoredString("EARLY:", SEC_LABEL_X, Y_PERF, TEXT_COLOR_RED);
	sprintf_s(buf, 32, "%d", rec.earlyPerfect);
	renderOutlinedColoredString(buf, SEC_VALUE_X, Y_PERF, TEXT_COLOR_RED);
	renderOutlinedColoredString("LATE:", SEC_LABEL_X, Y_PERF + LINE2, TEXT_COLOR_BLUE);
	sprintf_s(buf, 32, "%d", rec.latePerfect);
	renderOutlinedColoredString(buf, SEC_VALUE_X, Y_PERF + LINE2, TEXT_COLOR_BLUE);

	// GREAT
	masked_blit(m_resultSub, rm.m_backbuf, 0, 32, LABEL_X - 16, Y_GREAT + LABEL_BMP_Y, 192, 32);
	renderScoreNumber(greatCount, COUNT_X, Y_GREAT + SCORE_Y_OFF, greatCount >= 1000 ? 4 : 3);
	renderOutlinedColoredString("EARLY:", SEC_LABEL_X, Y_GREAT, TEXT_COLOR_RED);
	sprintf_s(buf, 32, "%d", rec.earlyGreat);
	renderOutlinedColoredString(buf, SEC_VALUE_X, Y_GREAT, TEXT_COLOR_RED);
	renderOutlinedColoredString("LATE:", SEC_LABEL_X, Y_GREAT + LINE2, TEXT_COLOR_BLUE);
	sprintf_s(buf, 32, "%d", rec.lateGreat);
	renderOutlinedColoredString(buf, SEC_VALUE_X, Y_GREAT + LINE2, TEXT_COLOR_BLUE);

	// GOOD
	masked_blit(m_resultSub, rm.m_backbuf, 0, 64, LABEL_X - 16, Y_GOOD  + LABEL_BMP_Y, 192, 32);
	renderScoreNumber(goodCount,  COUNT_X, Y_GOOD  + SCORE_Y_OFF, goodCount  >= 1000 ? 4 : 3);
	renderOutlinedColoredString("EARLY:", SEC_LABEL_X, Y_GOOD, TEXT_COLOR_RED);
	sprintf_s(buf, 32, "%d", rec.earlyGood);
	renderOutlinedColoredString(buf, SEC_VALUE_X, Y_GOOD, TEXT_COLOR_RED);
	renderOutlinedColoredString("LATE:", SEC_LABEL_X, Y_GOOD + LINE2, TEXT_COLOR_BLUE);
	sprintf_s(buf, 32, "%d", rec.lateGood);
	renderOutlinedColoredString(buf, SEC_VALUE_X, Y_GOOD + LINE2, TEXT_COLOR_BLUE);

	// MISS
	masked_blit(m_resultSub, rm.m_backbuf, 0, 96, LABEL_X - 16, Y_MISS  + MISS_BMP_Y,  192, 32);
	renderScoreNumber(rec.misses, COUNT_X, Y_MISS  + SCORE_Y_OFF, rec.misses >= 1000 ? 4 : 3);
	renderOutlinedColoredString("AVG:", SEC_LABEL_X, Y_MISS, 0);
	sprintf_s(buf, 32, "%+.2fms", rec.avgDiff);
	int avgColor = rec.avgDiff > 0.0 ? TEXT_COLOR_RED : (rec.avgDiff < 0.0 ? TEXT_COLOR_BLUE : TEXT_COLOR_WHITE);
	renderOutlinedColoredString(buf, SEC_VALUE_X, Y_MISS, avgColor);
	renderOutlinedColoredString("UR:", SEC_LABEL_X, Y_MISS + LINE2, 0);
	sprintf_s(buf, 32, "%.2f", rec.unstableRate);
	int urColor = rec.unstableRate < 200.0 ? TEXT_COLOR_GREEN : TEXT_COLOR_WHITE;
	renderOutlinedColoredString(buf, SEC_VALUE_X, Y_MISS + LINE2, urColor);

	// grade — horizontally centered with album art, vertically centered with score row
	const int SCORE_CY   = 357 + 16;                    // center of the 32px score glyph
	const int GRADE_X    = ART_X + 64 - 24;             // album center x minus half grade width
	const int GRADE_Y    = SCORE_CY - 24;               // center of 48px grade glyph
	renderGrade(rec.calculateGrade(), GRADE_X, GRADE_Y);

	// score + high score diff
	renderScoreNumber(rec.getScore(), 230, 357, 7);
	int songIndex  = songID_to_listID(rec.songID);
	int chartIndex = getChartIndexFromType(rec.chartID);
	if ( songIndex >= 0 && chartIndex >= 0 )
	{
		int highScore = sm.player[player].allTime[songIndex][chartIndex].getScore();
		int diff      = rec.getScore() - highScore;
		sprintf_s(buf, 32, "(%+07d)", diff);
		int diffColor = diff >= 0 ? TEXT_COLOR_GREEN : TEXT_COLOR_GREY;
		renderOutlinedColoredString(buf, 424, 367, diffColor);
	}
}
