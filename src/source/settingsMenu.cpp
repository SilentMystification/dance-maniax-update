// settingsMenu.cpp — per-player settings panel for the songwheel

#include <stdio.h>
#include "../headers/common.h"
#include "../headers/settingsMenu.h"
#include "../headers/gameStateManager.h"
#include "../headers/scoreManager.h"
#include "../headers/inputManager.h"

extern GameStateManager gs;
extern ScoreManager     sm;
extern RenderingManager rm;
extern InputManager     im;
extern EffectsManager   em;
extern SongEntry*       songs;

//////////////////////////////////////////////////////////////////////////////
// Visual constants — tweak these without touching logic
//////////////////////////////////////////////////////////////////////////////
#define SETTINGS_HIGHLIGHT_COLOR makeacol(255, 170, 0, 255) // goldish-orange selection border
#define SETTINGS_PANEL_WIDTH     256
#define SETTINGS_SLIDE_MS        200
#define SETTINGS_OPTION_SLIDE_MS 80
#define SETTINGS_ITEM_HEIGHT     60
#define SETTINGS_SELECTOR_SPEED  800
#define SETTINGS_ITEM_OUTLINE_COLOR makecol(45, 5, 82)
#define SETTINGS_START_Y         16
#define SETTINGS_OPTION_SLOT_W   80  // width of each option slot in the option row

#define HOLD_INITIAL_DELAY    250   // ms before auto-repeat begins after initial press
#define HOLD_INTERVAL_START   300   // ms between repeats at start
#define HOLD_INTERVAL_END      60   // ms between repeats at full speed
#define HOLD_RAMP_DURATION   1000   // ms to ramp from start rate to full speed

#define SETTINGS_SCROLL_SPEED 500   // px/sec for smooth scroll animation

//////////////////////////////////////////////////////////////////////////////
// Static option label arrays
//////////////////////////////////////////////////////////////////////////////
static const char* s_scoreModeOptions[]    = { "Classic", "Expert" };
static const int   s_scoreModeValues[]     = { 0, 1 };

static const char* s_scrollModeOptions[]   = { "Classic", "Fixed" };
static const int   s_scrollModeValues[]    = { 0, 1 };

static const char* s_classicSpeedOptions[] = { "1.0x","1.5x","2.0x","2.5x","3.0x","5.0x","8.0x" };
static const int   s_classicSpeedValues[]  = { 10, 15, 20, 25, 30, 50, 80 };

static const char* s_judgPosOptions[]      = { "Off","Bottom","Lower","Upper","Top" };
static const int   s_judgPosValues[]       = { 0, 1, 2, 3, 4 };

static const char* s_judgTextOptions[]     = { "All","Perfect-","Great-","Good-" };
static const int   s_judgTextValues[]      = { 0, 1, 2, 3 };

static const char* s_judgMsOptions[]       = { "All","Perfect-","Great-","Good-","Never" };
static const int   s_judgMsValues[]        = { 0, 1, 2, 3, 4 };

static const char* s_reverseOptions[]      = { "Off", "Reverse", "Cross" };
static const int   s_reverseValues[]       = { 0, 1, 2 };

static const char* s_mirrorOptions[]       = { "Off", "Mirror", "V-Flip" };
static const int   s_mirrorValues[]        = { 0, 1, 2 };

static const char* s_invertNoteColorsOptions[] = { "Off", "On" };
static const int   s_invertNoteColorsValues[]  = { 0, 1 };

static const char* s_positionOptions[]     = { "Left", "Center", "Right" };
static const int   s_positionValues[]      = { 1, 0, 2 };

static const char* s_chartModOptionsSingles[] = { "Off", "Random", "S-Random", "Inverted" };
static const int   s_chartModValuesSingles[]  = { 0, 1, 2, 4 };
static const char* s_chartModOptionsDoubles[] = { "Off", "Random", "S-Random", "D-Random", "Inverted" };
static const int   s_chartModValuesDoubles[]  = { 0, 1, 2, 3, 4 };

//////////////////////////////////////////////////////////////////////////////
// SettingsMenu implementation
//////////////////////////////////////////////////////////////////////////////

// Draw a directional arrow with a 3px black outline. dir: 0=left, 1=right, 2=up, 3=down.
// tipX/tipY is the point of the arrow. halfBase and depth control the triangle size.
static void drawNavArrow(int tipX, int tipY, int dir, int halfBase, int depth, int rgb)
{
	const int B = 3;
	int bx1, by1, bx2, by2;
	int sbx1, sby1, sbx2, sby2, stX, stY;
	int fx, fy;
	int px, py; // unit step the tip points: left (-1,0), right (1,0), up (0,-1), down (0,1)

	switch ( dir )
	{
	case 0: px = -1; py = 0; break;
	case 1: px =  1; py = 0; break;
	case 2: px =  0; py = -1; break;
	default: px = 0; py = 1; break;
	}

	if ( py == 0 ) // horizontal — base is a vertical edge
	{
		bx1 = tipX - px * depth; by1 = tipY - halfBase;
		bx2 = tipX - px * depth; by2 = tipY + halfBase;
		stX = tipX + px * B;     stY = tipY;
		sbx1 = bx1 - px * B;     sby1 = by1 - B;
		sbx2 = bx2 - px * B;     sby2 = by2 + B;
		fx = -px; fy = 0;
	}
	else // vertical — base is a horizontal edge
	{
		bx1 = tipX - halfBase; by1 = tipY - py * depth;
		bx2 = tipX + halfBase; by2 = tipY - py * depth;
		stX = tipX;            stY = tipY + py * B;
		sbx1 = bx1 - B;        sby1 = by1 - py * B;
		sbx2 = bx2 + B;        sby2 = by2 - py * B;
		fx = 0; fy = -py;
	}

	triangle(rm.m_backbuf, stX, stY, sbx1, sby1, sbx2, sby2, makecol(0, 0, 0));
	triangle(rm.m_backbuf, tipX + fx, tipY + fy, bx1 + fx, by1 + fy, bx2 + fx, by2 + fy, rgb);
}

static void fillToggleItem(SettingsItem& item, PLAYER_DATA& p)
{
	static const char* s_menuModeOptions[] = { "Simple", "Expert" };
	static const int   s_menuModeValues[]  = { 0, 1 };
	item.name             = "Menu Mode";
	item.type             = SETTINGS_LIST;
	item.dependency       = DEP_NONE;
	item.options          = s_menuModeOptions;
	item.optionValues     = s_menuModeValues;
	item.optionCount      = 2;
	item.value            = &p.useSimpleMenu;
	item.flagToSetOnChange= NULL;
}

void SettingsMenu::resetSettings(int playerData)
{
	m_selectedItem = 0;
	gs.player[playerData].chartMod = 0;
	sm.applyProfileToCredit(playerData);
}

void SettingsMenu::buildItemList(int player)
{
	m_itemCount  = 0;
	m_isAdvanced = (sm.player[player].useSimpleMenu == 1);
	memset(m_items, 0, sizeof(m_items));
	PLAYER_DATA& p = sm.player[player];

	if ( !m_isAdvanced )
	{
		// Simple menu: Speed, Reverse, Mirror, Play Position, toggle
		m_items[m_itemCount].name             = "Lane Speed";
		m_items[m_itemCount].type             = SETTINGS_LIST;
		m_items[m_itemCount].dependency       = DEP_NONE; // scroll mode forced Classic; always visible
		m_items[m_itemCount].options          = s_classicSpeedOptions;
		m_items[m_itemCount].optionValues     = s_classicSpeedValues;
		m_items[m_itemCount].optionCount      = 7;
		m_items[m_itemCount].value            = &p.speedMod;
		m_items[m_itemCount].flagToSetOnChange= NULL;
		m_itemCount++;

		m_items[m_itemCount].name             = "Reverse";
		m_items[m_itemCount].type             = SETTINGS_LIST;
		m_items[m_itemCount].dependency       = DEP_NONE;
		m_items[m_itemCount].options          = s_reverseOptions;
		m_items[m_itemCount].optionValues     = s_reverseValues;
		m_items[m_itemCount].optionCount      = 3;
		m_items[m_itemCount].value            = &p.reverseMode;
		m_items[m_itemCount].flagToSetOnChange= NULL;
		m_itemCount++;

		m_items[m_itemCount].name             = "Mirror";
		m_items[m_itemCount].type             = SETTINGS_LIST;
		m_items[m_itemCount].dependency       = DEP_NONE;
		m_items[m_itemCount].options          = s_mirrorOptions;
		m_items[m_itemCount].optionValues     = s_mirrorValues;
		m_items[m_itemCount].optionCount      = 3;
		m_items[m_itemCount].value            = &p.mirrorMode;
		m_items[m_itemCount].flagToSetOnChange= NULL;
		m_itemCount++;

		m_items[m_itemCount].name             = "Play Position";
		m_items[m_itemCount].type             = SETTINGS_LIST;
		m_items[m_itemCount].dependency       = DEP_PLAY_POSITION;
		m_items[m_itemCount].options          = s_positionOptions;
		m_items[m_itemCount].optionValues     = s_positionValues;
		m_items[m_itemCount].optionCount      = 3;
		m_items[m_itemCount].value            = &p.playPosition;
		m_items[m_itemCount].flagToSetOnChange= NULL;
		m_itemCount++;

		fillToggleItem(m_items[m_itemCount], p);
		m_itemCount++;
	}
	else
	{
		// Advanced menu: 14 items, toggle at index 5
		// 0. Lane Speed (Classic)
		m_items[m_itemCount].name             = "Lane Speed";
		m_items[m_itemCount].type             = SETTINGS_LIST;
		m_items[m_itemCount].dependency       = DEP_SCROLL_CLASSIC;
		m_items[m_itemCount].options          = s_classicSpeedOptions;
		m_items[m_itemCount].optionValues     = s_classicSpeedValues;
		m_items[m_itemCount].optionCount      = 7;
		m_items[m_itemCount].value            = &p.speedMod;
		m_items[m_itemCount].flagToSetOnChange= NULL;
		m_itemCount++;

		// 1. Lane Speed (Fixed)
		m_items[m_itemCount].name             = "Lane Speed";
		m_items[m_itemCount].type             = SETTINGS_RANGE;
		m_items[m_itemCount].dependency       = DEP_SCROLL_FIXED;
		m_items[m_itemCount].minVal           = 25;
		m_items[m_itemCount].maxVal           = 700;
		m_items[m_itemCount].step             = 5;
		m_items[m_itemCount].value            = &p.fixedScrollPPS;
		m_items[m_itemCount].flagToSetOnChange= NULL;
		m_itemCount++;

		// 2. Reverse
		m_items[m_itemCount].name             = "Reverse";
		m_items[m_itemCount].type             = SETTINGS_LIST;
		m_items[m_itemCount].dependency       = DEP_NONE;
		m_items[m_itemCount].options          = s_reverseOptions;
		m_items[m_itemCount].optionValues     = s_reverseValues;
		m_items[m_itemCount].optionCount      = 3;
		m_items[m_itemCount].value            = &p.reverseMode;
		m_items[m_itemCount].flagToSetOnChange= NULL;
		m_itemCount++;

		// 3. Mirror
		m_items[m_itemCount].name             = "Mirror";
		m_items[m_itemCount].type             = SETTINGS_LIST;
		m_items[m_itemCount].dependency       = DEP_NONE;
		m_items[m_itemCount].options          = s_mirrorOptions;
		m_items[m_itemCount].optionValues     = s_mirrorValues;
		m_items[m_itemCount].optionCount      = 3;
		m_items[m_itemCount].value            = &p.mirrorMode;
		m_items[m_itemCount].flagToSetOnChange= NULL;
		m_itemCount++;

		// 4. Play Position
		m_items[m_itemCount].name             = "Play Position";
		m_items[m_itemCount].type             = SETTINGS_LIST;
		m_items[m_itemCount].dependency       = DEP_PLAY_POSITION;
		m_items[m_itemCount].options          = s_positionOptions;
		m_items[m_itemCount].optionValues     = s_positionValues;
		m_items[m_itemCount].optionCount      = 3;
		m_items[m_itemCount].value            = &p.playPosition;
		m_items[m_itemCount].flagToSetOnChange= NULL;
		m_itemCount++;

		// 5. Menu Mode toggle (between Play Position and Judgment Position)
		fillToggleItem(m_items[m_itemCount], p);
		m_itemCount++;

		// 7. Judgment Position
		m_items[m_itemCount].name             = "Judgment Position";
		m_items[m_itemCount].type             = SETTINGS_LIST;
		m_items[m_itemCount].dependency       = DEP_NONE;
		m_items[m_itemCount].options          = s_judgPosOptions;
		m_items[m_itemCount].optionValues     = s_judgPosValues;
		m_items[m_itemCount].optionCount      = 5;
		m_items[m_itemCount].value            = &p.judgementPositionMode;
		m_items[m_itemCount].flagToSetOnChange= NULL;
		m_itemCount++;

		// 8. Early / Late Display
		m_items[m_itemCount].name             = "Early / Late Display";
		m_items[m_itemCount].type             = SETTINGS_LIST;
		m_items[m_itemCount].dependency       = DEP_JUDGMENT_ON;
		m_items[m_itemCount].options          = s_judgTextOptions;
		m_items[m_itemCount].optionValues     = s_judgTextValues;
		m_items[m_itemCount].optionCount      = 4;
		m_items[m_itemCount].value            = &p.judgementEarlyLateMode;
		m_items[m_itemCount].flagToSetOnChange= NULL;
		m_itemCount++;

		// 9. +- MS Display
		m_items[m_itemCount].name             = "+- MS Display";
		m_items[m_itemCount].type             = SETTINGS_LIST;
		m_items[m_itemCount].dependency       = DEP_JUDGMENT_ON;
		m_items[m_itemCount].options          = s_judgMsOptions;
		m_items[m_itemCount].optionValues     = s_judgMsValues;
		m_items[m_itemCount].optionCount      = 5;
		m_items[m_itemCount].value            = &p.judgementMsDisplayMode;
		m_items[m_itemCount].flagToSetOnChange= NULL;
		m_itemCount++;

		// 9. Invert Note Colors
		m_items[m_itemCount].name             = "Invert Note Colors";
		m_items[m_itemCount].type             = SETTINGS_LIST;
		m_items[m_itemCount].dependency       = DEP_NONE;
		m_items[m_itemCount].options          = s_invertNoteColorsOptions;
		m_items[m_itemCount].optionValues     = s_invertNoteColorsValues;
		m_items[m_itemCount].optionCount      = 2;
		m_items[m_itemCount].value            = &p.invertNoteColors;
		m_items[m_itemCount].flagToSetOnChange= NULL;
		m_itemCount++;

		// 10. Chart Mods — not saved between credits; value lives in gs.player (not sm.player)
		// D-Random is doubles-only: reset to Regular Random if in singles with D-Random selected
		if ( !gs.isDoubles && gs.player[player].chartMod == 3 )
			gs.player[player].chartMod = 1;
		m_items[m_itemCount].name             = "Chart Mods";
		m_items[m_itemCount].type             = SETTINGS_LIST;
		m_items[m_itemCount].dependency       = DEP_NONE;
		m_items[m_itemCount].optionSlotW      = 90;
		if ( gs.isDoubles )
		{
			m_items[m_itemCount].options      = s_chartModOptionsDoubles;
			m_items[m_itemCount].optionValues = s_chartModValuesDoubles;
			m_items[m_itemCount].optionCount  = 5;
		}
		else
		{
			m_items[m_itemCount].options      = s_chartModOptionsSingles;
			m_items[m_itemCount].optionValues = s_chartModValuesSingles;
			m_items[m_itemCount].optionCount  = 4;
		}
		m_items[m_itemCount].value            = &gs.player[player].chartMod;
		m_items[m_itemCount].flagToSetOnChange= NULL;
		m_itemCount++;

		// 11. Visual Offset
		m_items[m_itemCount].name             = "Visual Offset";
		m_items[m_itemCount].type             = SETTINGS_RANGE;
		m_items[m_itemCount].dependency       = DEP_NONE;
		m_items[m_itemCount].minVal           = -100;
		m_items[m_itemCount].maxVal           = 100;
		m_items[m_itemCount].step             = 1;
		m_items[m_itemCount].value            = &p.visualOffset;
		m_items[m_itemCount].flagToSetOnChange= NULL;
		m_itemCount++;

		// 12. Audio Offset
		m_items[m_itemCount].name             = "Audio Offset";
		m_items[m_itemCount].type             = SETTINGS_RANGE;
		m_items[m_itemCount].dependency       = DEP_NONE;
		m_items[m_itemCount].minVal           = -500;
		m_items[m_itemCount].maxVal           = 500;
		m_items[m_itemCount].step             = 1;
		m_items[m_itemCount].value            = &p.audioOffset;
		m_items[m_itemCount].flagToSetOnChange= &p.hasCustomAudioOffset;
		m_itemCount++;

		// 13. Score Mode
		m_items[m_itemCount].name             = "Score Mode";
		m_items[m_itemCount].type             = SETTINGS_LIST;
		m_items[m_itemCount].dependency       = DEP_NONE;
		m_items[m_itemCount].options          = s_scoreModeOptions;
		m_items[m_itemCount].optionValues     = s_scoreModeValues;
		m_items[m_itemCount].optionCount      = 2;
		m_items[m_itemCount].value            = &p.scoreMode;
		m_items[m_itemCount].flagToSetOnChange= NULL;
		m_itemCount++;

		// 14. Scroll Mode
		m_items[m_itemCount].name             = "Scroll Mode";
		m_items[m_itemCount].type             = SETTINGS_LIST;
		m_items[m_itemCount].dependency       = DEP_NONE;
		m_items[m_itemCount].options          = s_scrollModeOptions;
		m_items[m_itemCount].optionValues     = s_scrollModeValues;
		m_items[m_itemCount].optionCount      = 2;
		m_items[m_itemCount].value            = &p.scrollMode;
		m_items[m_itemCount].flagToSetOnChange= NULL;
		m_itemCount++;
	}
}

bool SettingsMenu::isItemVisible(int index) const
{
	const SettingsItem& item = m_items[index];
	switch ( item.dependency )
	{
	case DEP_NONE:
		return true;
	case DEP_SCROLL_CLASSIC:
		return sm.player[m_playerData].scrollMode == 0;
	case DEP_SCROLL_FIXED:
		return sm.player[m_playerData].scrollMode == 1;
	case DEP_JUDGMENT_ON:
		return sm.player[m_playerData].judgementPositionMode != 0;
	case DEP_PLAY_POSITION:
		return !gs.isDoubles && !gs.isVersus;
	}
	return true;
}

void SettingsMenu::advanceSelectionIfHidden()
{
	// if selected item is hidden, advance to next visible item
	for ( int i = 0; i < m_itemCount; i++ )
	{
		int candidate = (m_selectedItem + i) % m_itemCount;
		if ( isItemVisible(candidate) )
		{
			m_selectedItem = candidate;
			return;
		}
	}
}

void SettingsMenu::open(int playerData, int side)
{
	em.playSample(SFX_COURSE_PREVIEW_LOAD);
	m_player       = side;
	m_playerData   = playerData;
	m_isOpen       = true;
	m_isClosing    = false;
	m_slideTimer   = 0;
	m_slideOffsetX = (m_player == 0) ? -SETTINGS_PANEL_WIDTH : SCREEN_WIDTH;
	m_isEditingItem= false;
	m_optionSlideOffset = 0;
	m_optionSlideTimer  = 0;
	m_optionSlideDir    = 0;
	m_activeSlotW       = SETTINGS_OPTION_SLOT_W;
	m_holdDir           = 0;
	m_holdTime          = 0;
	m_repeatTimer       = 0;
	m_scrollY              = 0;
	m_targetScrollY        = 0;
	m_selectorPanelY       = -9999; // snap to correct position on first render frame
	m_targetSelectorPanelY = SETTINGS_START_Y;
	m_snapSelector         = false;
	m_bobTimer             = 0;
	m_cancelHoldTimer      = 0;
	m_lastNavTimer         = 9999; // large value so no chord suppression on first open
	m_lastNavDir           = 0;
	m_waitForRelease       = false;

	// sync current modifier state from gs.player into sm.player backing fields
	{
		PLAYER_DATA& pd = sm.player[m_playerData];
		unsigned char rv = (unsigned char)gs.player[m_playerData].reverseModifier;
		pd.reverseMode  = (rv == 0x00) ? 0 : (rv == 0x99 ? 2 : 1);
		pd.mirrorMode   = (int)gs.player[m_playerData].arrangeModifier;
		if ( !gs.isDoubles && !gs.isVersus )
		{
			pd.playPosition = gs.player[m_playerData].centerLeft ? 1 : (gs.player[m_playerData].centerRight ? 2 : 0);
		}
	}

	buildItemList(m_playerData);

	// snapshot saved values at open time
	for ( int i = 0; i < m_itemCount; i++ )
	{
		m_items[i].savedValue = *m_items[i].value;
	}
	// audio offset: if not custom, show bgmGap as the "saved" value
	for ( int i = 0; i < m_itemCount; i++ )
	{
		if ( m_items[i].flagToSetOnChange == &sm.player[m_playerData].hasCustomAudioOffset )
		{
			if ( !sm.player[m_playerData].hasCustomAudioOffset )
			{
				m_items[i].savedValue = gs.bgmGap;
				*m_items[i].value     = gs.bgmGap;
			}
			break;
		}
	}

	advanceSelectionIfHidden();

	// advanced mode: snap scroll immediately so there's no animation on open
	if ( m_isAdvanced )
	{
		int selCenter = SCREEN_HEIGHT / 2 - SETTINGS_ITEM_HEIGHT / 2;
		int selVisIdx = 0;
		for ( int i = 0; i < m_selectedItem; i++ )
			if ( isItemVisible(i) ) selVisIdx++;
		m_scrollY       = SETTINGS_START_Y - selCenter + selVisIdx * SETTINGS_ITEM_HEIGHT;
		m_targetScrollY = m_scrollY;
	}
}

void SettingsMenu::close()
{
	if ( !m_isClosing )
	{
		em.playSample(SFX_COURSE_APPEAR);
		m_isClosing  = true;
		m_slideTimer = 0;
	}
}

void SettingsMenu::forceClose()
{
	// revert any in-progress (unconfirmed) edit back to the last confirmed value
	if ( m_isEditingItem && m_selectedItem >= 0 && m_selectedItem < m_itemCount )
	{
		*m_items[m_selectedItem].value = m_items[m_selectedItem].savedValue;
	}
	m_isOpen    = false;
	m_isClosing = false;
	m_waitForRelease = false;
}

bool SettingsMenu::isFullyClosed() const
{
	return !m_isOpen;
}

bool SettingsMenu::isEditing() const
{
	return m_isEditingItem;
}

bool SettingsMenu::needsReleaseBeforeClose() const
{
	return m_waitForRelease;
}

void SettingsMenu::handleInput(UTIME dt)
{
	int left  = im.getKeyState(m_player == 0 ? MENU_LEFT_1P  : MENU_LEFT_2P);
	int right = im.getKeyState(m_player == 0 ? MENU_RIGHT_1P : MENU_RIGHT_2P);
	int start = im.getKeyState(m_player == 0 ? MENU_START_1P : MENU_START_2P);

	bool leftDown  = (left  == JUST_DOWN);
	bool rightDown = (right == JUST_DOWN);
	bool startDown = (start == JUST_DOWN);
	bool bothDown  = (left != 0) && (right != 0);

	// always advance the nav-recency timer
	m_lastNavTimer += (int)dt;

	if ( m_waitForRelease && !bothDown )
		m_waitForRelease = false;

	if ( !m_isEditingItem )
	{
		m_holdDir = 0; m_holdTime = 0; m_repeatTimer = 0;

		if ( bothDown )
		{
			// undo the last navigation if it happened very recently (accidental press before L+R chord)
			if ( m_lastNavTimer < 150 && m_lastNavDir != 0 )
			{
				int undoDir = -m_lastNavDir; // opposite direction
				int prevItem = m_selectedItem;
				for ( int i = 1; i <= m_itemCount; i++ )
				{
					int candidate = (m_selectedItem + undoDir * i + m_itemCount) % m_itemCount;
					if ( isItemVisible(candidate) )
					{
						m_selectedItem = candidate;
						break;
					}
				}
				if ( m_selectedItem != prevItem && m_isAdvanced )
				{
					int totalVisible = 0;
					for ( int i = 0; i < m_itemCount; i++ )
						if ( isItemVisible(i) ) totalVisible++;
					int totalListH = totalVisible * SETTINGS_ITEM_HEIGHT;
					if ( undoDir > 0 ) m_scrollY -= totalListH;
					else               m_scrollY += totalListH;
				}
				m_lastNavDir   = 0;
				m_lastNavTimer = 9999;
			}
			return; // close handled by caller
		}

		int prevSelected = m_selectedItem;
		if ( leftDown )
		{
			// move up — expert mode wraps infinitely; simple mode stops at top
			for ( int i = 1; i <= m_itemCount; i++ )
			{
				int candidate = (m_selectedItem - i + m_itemCount) % m_itemCount;
				if ( !m_isAdvanced && candidate > m_selectedItem ) break; // no wrap in simple
				if ( isItemVisible(candidate) )
				{
					m_selectedItem = candidate;
					break;
				}
			}
		}
		else if ( rightDown )
		{
			// move down — expert mode wraps infinitely; simple mode stops at bottom
			for ( int i = 1; i <= m_itemCount; i++ )
			{
				int candidate = (m_selectedItem + i) % m_itemCount;
				if ( !m_isAdvanced && candidate < m_selectedItem ) break; // no wrap in simple
				if ( isItemVisible(candidate) )
				{
					m_selectedItem = candidate;
					break;
				}
			}
		}
		if ( m_selectedItem != prevSelected )
		{
			bool wrapped = (leftDown  && m_selectedItem > prevSelected) ||
			               (rightDown && m_selectedItem < prevSelected);
			if ( wrapped )
			{
				if ( m_isAdvanced )
				{
					// offset m_scrollY so the animation continues one step in the same direction
					int totalVisible = 0;
					for ( int i = 0; i < m_itemCount; i++ )
						if ( isItemVisible(i) ) totalVisible++;
					int totalListH = totalVisible * SETTINGS_ITEM_HEIGHT;
					if ( rightDown ) m_scrollY -= totalListH; // last→first: keep scrolling up
					else             m_scrollY += totalListH; // first→last: keep scrolling down
				}
				else
				{
					m_snapSelector = true; // simple mode (no wrap, safety only)
				}
			}
			m_lastNavTimer = 0;
			m_lastNavDir   = leftDown ? -1 : 1;
			em.playSample(SFX_SONGWHEEL_MOVE);
		}
		if ( startDown )
		{
			em.playSample(SFX_SONGWHEEL_PICK);
			m_isEditingItem    = true;
			m_cancelHoldTimer  = 0;
			m_optionSlideOffset = 0;
		}
	}
	else
	{
		// inner mode: adjust value with hold-to-repeat
		SettingsItem& item = m_items[m_selectedItem];

		// L+R held while editing: cancel after 1.5 seconds, reverting to saved value
		if ( bothDown )
		{
			m_cancelHoldTimer += (int)dt;
			if ( m_cancelHoldTimer >= 1500 )
			{
				*m_items[m_selectedItem].value = m_items[m_selectedItem].savedValue;
				m_isEditingItem   = false;
				m_cancelHoldTimer = 0;
				m_holdDir = 0; m_holdTime = 0; m_repeatTimer = 0;
				m_waitForRelease  = true;
				em.playSample(SFX_COURSE_APPEAR);
			}
			return;
		}
		m_cancelHoldTimer = 0;

		if ( startDown )
		{
			em.playSample(SFX_SONGWHEEL_APPEAR);
			int prevSaved = m_items[m_selectedItem].savedValue; // capture before overwrite
			m_items[m_selectedItem].savedValue = *m_items[m_selectedItem].value;
			m_isEditingItem = false;
			m_holdDir = 0; m_holdTime = 0; m_repeatTimer = 0;

			// check if the committed item is the menu mode toggle
			if ( m_items[m_selectedItem].value == &sm.player[m_playerData].useSimpleMenu )
			{
				int newMode = sm.player[m_playerData].useSimpleMenu;
				if ( newMode == prevSaved )
				{
					// no change — exit edit mode silently
					return;
				}
				if ( newMode == 1 )
					em.announcerQuip(82); // switching to Advanced: GAME_0082.wav
				else
					em.announcerQuip(79); // switching to Simple: GAME_0079.wav

				buildItemList(m_playerData);

				// find the toggle item in the new list and land on it
				m_selectedItem = 0;
				for ( int i = 0; i < m_itemCount; i++ )
				{
					if ( m_items[i].value == &sm.player[m_playerData].useSimpleMenu )
					{
						m_selectedItem = i;
						break;
					}
				}
				// snapshot savedValues for the rebuilt list
				for ( int i = 0; i < m_itemCount; i++ )
					m_items[i].savedValue = *m_items[i].value;
				// audio offset special case: seed display from bgmGap if not custom
				for ( int i = 0; i < m_itemCount; i++ )
				{
					if ( m_items[i].flagToSetOnChange == &sm.player[m_playerData].hasCustomAudioOffset )
					{
						if ( !sm.player[m_playerData].hasCustomAudioOffset )
						{
							m_items[i].savedValue = gs.bgmGap;
							*m_items[i].value     = gs.bgmGap;
						}
						break;
					}
				}
				// reset scroll so the new list starts fresh
				m_scrollY              = 0;
				m_targetScrollY        = 0;
				m_selectorPanelY       = -9999; // snap on next render
				m_snapSelector         = false;
				return;
			}

			advanceSelectionIfHidden(); // item may have become hidden (e.g. scroll mode change)
			return;
		}

		// hold-to-repeat: track direction held, fire on tap and on timer expiry
		int thisDir = (left != 0) ? -1 : ((right != 0) ? 1 : 0);
		if ( thisDir != m_holdDir )
		{
			m_holdDir     = thisDir;
			m_holdTime    = 0;
			m_repeatTimer = HOLD_INITIAL_DELAY;
		}

		bool fireLeft  = leftDown;
		bool fireRight = rightDown;
		if ( thisDir != 0 && !leftDown && !rightDown )
		{
			m_holdTime    += (int)dt;
			m_repeatTimer -= (int)dt;
			if ( m_repeatTimer <= 0 )
			{
				if ( thisDir == -1 ) fireLeft  = true;
				else                 fireRight = true;
				int holdAge  = MAX(0, m_holdTime - HOLD_INITIAL_DELAY);
				int rampT    = MIN(holdAge, HOLD_RAMP_DURATION);
				m_repeatTimer = HOLD_INTERVAL_START - rampT * (HOLD_INTERVAL_START - HOLD_INTERVAL_END) / HOLD_RAMP_DURATION;
			}
		}

		int prevVal = *item.value;

		if ( item.type == SETTINGS_LIST )
		{
			// find current index in options
			int idx = 0;
			for ( int i = 0; i < item.optionCount; i++ )
			{
				if ( item.optionValues[i] == *item.value ) { idx = i; break; }
			}
			if ( fireLeft  && idx > 0 )                    { idx--; m_optionSlideDir = -1; }
			if ( fireRight && idx < item.optionCount - 1 ) { idx++; m_optionSlideDir =  1; }
			*item.value = item.optionValues[idx];
		}
		else // SETTINGS_RANGE
		{
			if ( fireLeft  ) { *item.value = MAX(item.minVal, *item.value - item.step); m_optionSlideDir = -1; }
			if ( fireRight ) { *item.value = MIN(item.maxVal, *item.value + item.step); m_optionSlideDir =  1; }
		}

		if ( *item.value != prevVal )
		{
			em.playSample(SFX_DIFFICULTY_MOVE);
			if ( item.flagToSetOnChange != NULL )
			{
				*item.flagToSetOnChange = true;
			}
			m_activeSlotW       = (item.type == SETTINGS_LIST && item.optionSlotW > 0) ? item.optionSlotW : SETTINGS_OPTION_SLOT_W;
			m_optionSlideOffset = m_optionSlideDir * m_activeSlotW;
			m_optionSlideTimer  = SETTINGS_OPTION_SLIDE_MS;
		}
	}
}

void SettingsMenu::render(UTIME dt)
{
	// update slide timer
	m_slideTimer += dt;
	m_bobTimer   += dt;
	if ( m_slideTimer > SETTINGS_SLIDE_MS )
	{
		m_slideTimer = SETTINGS_SLIDE_MS;
		if ( m_isClosing )
		{
			m_isOpen = false; // fully closed
			return;
		}
	}

	// update option slide animation
	if ( m_optionSlideTimer > 0 )
	{
		int step = (int)dt;
		if ( step > (int)m_optionSlideTimer ) step = (int)m_optionSlideTimer;
		m_optionSlideTimer -= step;
		int remainPct = (m_optionSlideTimer * 100 / SETTINGS_OPTION_SLIDE_MS);
		m_optionSlideOffset = m_optionSlideDir * getValueFromRange(0, m_activeSlotW, remainPct);
	}
	else
	{
		m_optionSlideOffset = 0;
	}

	// compute current panel left X
	int pct = (int)(m_slideTimer * 100 / SETTINGS_SLIDE_MS);
	if ( !m_isClosing )
	{
		if ( m_player == 0 )
			m_slideOffsetX = getValueFromRange(-SETTINGS_PANEL_WIDTH, 0, pct);
		else
			m_slideOffsetX = getValueFromRange(SCREEN_WIDTH, SCREEN_WIDTH - SETTINGS_PANEL_WIDTH, pct);
	}
	else
	{
		if ( m_player == 0 )
			m_slideOffsetX = getValueFromRange(0, -SETTINGS_PANEL_WIDTH, pct);
		else
			m_slideOffsetX = getValueFromRange(SCREEN_WIDTH - SETTINGS_PANEL_WIDTH, SCREEN_WIDTH, pct);
	}

	int panelLeft    = m_slideOffsetX;
	int panelRight   = m_slideOffsetX + SETTINGS_PANEL_WIDTH - 1;
	int panelCenterX = panelLeft + SETTINGS_PANEL_WIDTH / 2;

	// draw semi-transparent panel background
	set_alpha_blender();
	drawing_mode(DRAW_MODE_TRANS, NULL, 0, 0);
	rectfill(rm.m_backbuf, panelLeft, 0, panelRight, SCREEN_HEIGHT - 1, makeacol(70, 8, 128, 220));

	// switch to solid mode for border and all item rendering
	solid_mode();

	// 2-pixel black border along the panel edges
	rect(rm.m_backbuf, panelLeft,     0, panelRight,     SCREEN_HEIGHT - 1, makecol(0, 0, 0));
	rect(rm.m_backbuf, panelLeft + 1, 1, panelRight - 1, SCREEN_HEIGHT - 2, makecol(0, 0, 0));

	// count visible items and find selected item's visible index
	int selectedVisibleIdx = 0;
	int totalVisible       = 0;
	for ( int i = 0; i < m_itemCount; i++ )
	{
		if ( !isItemVisible(i) ) continue;
		if ( i < m_selectedItem ) selectedVisibleIdx++;
		totalVisible++;
	}
	int totalListH = totalVisible * SETTINGS_ITEM_HEIGHT;

	if ( !m_isAdvanced )
	{
		// Simple mode: center items vertically; selector slides to selected item
		int startY = MAX(0, (SCREEN_HEIGHT - totalListH) / 2);
		m_targetScrollY = SETTINGS_START_Y - startY; // negative = items pushed below SETTINGS_START_Y

		m_targetSelectorPanelY = SETTINGS_START_Y + selectedVisibleIdx * SETTINGS_ITEM_HEIGHT;
		if ( m_selectorPanelY == -9999 || m_snapSelector )
		{
			m_selectorPanelY = m_targetSelectorPanelY;
			m_snapSelector   = false;
		}
		else if ( m_selectorPanelY != m_targetSelectorPanelY )
		{
			int sdiff    = m_targetSelectorPanelY - m_selectorPanelY;
			int smaxStep = MAX(1, SETTINGS_SELECTOR_SPEED * (int)dt / 1000);
			if ( sdiff < 0 ? -sdiff <= smaxStep : sdiff <= smaxStep )
				m_selectorPanelY = m_targetSelectorPanelY;
			else
				m_selectorPanelY += (sdiff > 0) ? smaxStep : -smaxStep;
		}
	}
	else
	{
		// Advanced mode: selector fixed at screen center; items scroll to it
		int selCenter   = SCREEN_HEIGHT / 2 - SETTINGS_ITEM_HEIGHT / 2;
		m_targetScrollY = SETTINGS_START_Y - selCenter + selectedVisibleIdx * SETTINGS_ITEM_HEIGHT;
		// m_selectorPanelY tracks m_scrollY so (panelY - scrollY) == selCenter always
		m_selectorPanelY = selCenter + m_scrollY;
		m_snapSelector   = false;
	}

	// animate scroll toward target
	if ( m_scrollY != m_targetScrollY )
	{
		int diff    = m_targetScrollY - m_scrollY;
		int maxStep = MAX(1, SETTINGS_SCROLL_SPEED * (int)dt / 1000);
		if ( diff < 0 ? -diff <= maxStep : diff <= maxStep )
			m_scrollY = m_targetScrollY;
		else
			m_scrollY += (diff > 0) ? maxStep : -maxStep;
	}

	// clip to panel inner area — set before outlines and selector so nothing overdraws the border
	int savedCX1, savedCY1, savedCX2, savedCY2;
	get_clip_rect(rm.m_backbuf, &savedCX1, &savedCY1, &savedCX2, &savedCY2);
	set_clip_rect(rm.m_backbuf, panelLeft + 2, 0, panelRight - 2, SCREEN_HEIGHT - 1);

	// pre-pass: draw item outlines; advanced mode draws 3 repetitions for infinite wrap
	{
		int numReps = m_isAdvanced ? 3 : 1;
		for ( int rep = 0; rep < numReps; rep++ )
		{
			int repOffset = m_isAdvanced ? (rep - 1) * totalListH : 0;
			int preY = SETTINGS_START_Y - m_scrollY + repOffset;
			for ( int i = 0; i < m_itemCount; i++ )
			{
				if ( !isItemVisible(i) ) continue;
				int iy = preY;
				preY += SETTINGS_ITEM_HEIGHT;
				if ( iy + SETTINGS_ITEM_HEIGHT <= 0 || iy >= SCREEN_HEIGHT ) continue;
				rect(rm.m_backbuf, panelLeft + 2, iy, panelRight - 2, iy + SETTINGS_ITEM_HEIGHT - 2, SETTINGS_ITEM_OUTLINE_COLOR);
			}
		}
	}

	// selector: transparent warm fill + 2px gold outline, both move together with m_selectorPanelY
	{
		int selectorScreenY = m_selectorPanelY - m_scrollY;
		int hx1 = panelLeft  + 2, hy1 = selectorScreenY;
		int hx2 = panelRight - 2, hy2 = selectorScreenY + SETTINGS_ITEM_HEIGHT - 2;

		set_alpha_blender();
		drawing_mode(DRAW_MODE_TRANS, NULL, 0, 0);
		rectfill(rm.m_backbuf, hx1 + 2, hy1 + 2, hx2 - 2, hy2 - 2, makeacol(255, 220, 100, 35));
		solid_mode();
		rect(rm.m_backbuf, hx1,     hy1,     hx2,     hy2,     SETTINGS_HIGHLIGHT_COLOR);
		rect(rm.m_backbuf, hx1 + 1, hy1 + 1, hx2 - 1, hy2 - 1, SETTINGS_HIGHLIGHT_COLOR);
	}

	// ---- draw items (advanced mode loops 3 times for infinite wrap) ----
	int numReps = m_isAdvanced ? 3 : 1;
	for ( int rep = 0; rep < numReps; rep++ )
	{
	bool isMainRep = (!m_isAdvanced || rep == 1); // edit-mode decorations only on the center pass
	int repOffset  = m_isAdvanced ? (rep - 1) * totalListH : 0;
	int visItemY   = SETTINGS_START_Y - m_scrollY + repOffset;
	for ( int i = 0; i < m_itemCount; i++ )
	{
		if ( !isItemVisible(i) ) continue;

		int itemY = visItemY;
		visItemY += SETTINGS_ITEM_HEIGHT;

		// skip items fully off-screen
		if ( itemY + SETTINGS_ITEM_HEIGHT <= 0 || itemY >= SCREEN_HEIGHT ) continue;

		bool isSelected = isMainRep && (i == m_selectedItem);

		int nameW  = getBoldStringWidth(m_items[i].name);
		int titleX = MAX(panelLeft + 4, panelLeft + SETTINGS_PANEL_WIDTH / 2 - nameW / 2);
		renderBoldString(m_items[i].name, titleX, itemY + 10, SETTINGS_PANEL_WIDTH - 8, false, 0);

		// option row: color font (12px tall) — 4px gap below name → top at itemY+38
		int slideOff = (isSelected && m_isEditingItem) ? m_optionSlideOffset : 0;
		int optRowY  = itemY + 38;

		if ( m_items[i].type == SETTINGS_LIST )
		{
			// find current option index
			int idx = 0;
			for ( int j = 0; j < m_items[i].optionCount; j++ )
			{
				if ( m_items[i].optionValues[j] == *m_items[i].value ) { idx = j; break; }
			}

			// current option — centered, green if confirmed, white if not yet confirmed
			const char* curLabel = m_items[i].options[idx];
			int cx = panelCenterX + slideOff;
			int cw = (int)strlen(curLabel) * 10;
			int centerColor = (m_items[i].optionValues[idx] == m_items[i].savedValue) ? TEXT_COLOR_GREEN : TEXT_COLOR_WHITE;
			renderOutlinedColoredString(curLabel, cx - cw / 2, optRowY, centerColor);

			int listSlotW = (m_items[i].optionSlotW > 0) ? m_items[i].optionSlotW : SETTINGS_OPTION_SLOT_W;
			// previous option: green if it's the savedValue, white otherwise
			if ( idx > 0 )
			{
				const char* prevLabel = m_items[i].options[idx - 1];
				int prevColor = (m_items[i].optionValues[idx - 1] == m_items[i].savedValue)
					? TEXT_COLOR_GREEN : TEXT_COLOR_WHITE;
				int px = panelCenterX - listSlotW + slideOff;
				int pw = (int)strlen(prevLabel) * 10;
				renderOutlinedColoredString(prevLabel, px - pw/2, optRowY, prevColor);
			}
			// next option: green if it's the savedValue, white otherwise
			if ( idx < m_items[i].optionCount - 1 )
			{
				const char* nextLabel = m_items[i].options[idx + 1];
				int nextColor = (m_items[i].optionValues[idx + 1] == m_items[i].savedValue)
					? TEXT_COLOR_GREEN : TEXT_COLOR_WHITE;
				int nx = panelCenterX + listSlotW + slideOff;
				int nw = (int)strlen(nextLabel) * 10;
				renderOutlinedColoredString(nextLabel, nx - nw/2, optRowY, nextColor);
			}

			// navigation triangles in edit mode — blue, bob outward
			if ( isSelected && m_isEditingItem )
			{
				int phase     = (int)(m_bobTimer % 600);
				int bobOffset = (phase < 300) ? (phase * 3 / 300) : ((600 - phase) * 3 / 300);
				int triCy     = optRowY + 7;
				int blue      = makecol(0, 180, 255);

				if ( idx > 0 )
					drawNavArrow(panelLeft + 5 - bobOffset, triCy, 0, 5, 6, blue);
				if ( idx < m_items[i].optionCount - 1 )
					drawNavArrow(panelRight - 5 + bobOffset, triCy, 1, 5, 6, blue);
			}
		}
		else // SETTINGS_RANGE
		{
			bool isAudioOffset = (m_items[i].flagToSetOnChange != NULL);
			int dispVal = *m_items[i].value;
			if ( isAudioOffset && !(*m_items[i].flagToSetOnChange) )
				dispVal = gs.bgmGap;

			// compute slot width from the widest possible label in this range
			char tmpBuf[16];
			int maxChars = 1;
			sprintf_s(tmpBuf, sizeof(tmpBuf), "%d", m_items[i].minVal);
			if ( (int)strlen(tmpBuf) > maxChars ) maxChars = (int)strlen(tmpBuf);
			sprintf_s(tmpBuf, sizeof(tmpBuf), "%d", m_items[i].maxVal);
			if ( (int)strlen(tmpBuf) > maxChars ) maxChars = (int)strlen(tmpBuf);

			// 5-slot strip: divide 90% of panel width into 5 equal slots
			const int numH  = 2;   // 2 neighbors on each side = 5 total
			const int slotW = (SETTINGS_PANEL_WIDTH * 9 / 10) / (2 * numH + 1);
			const int charW = 10;  // outlined font char width for neighbors

			// scale slide animation to match slot width
			int rangeSlide = (isSelected && m_isEditingItem)
				? (m_optionSlideOffset * slotW / m_activeSlotW) : 0;
			int cx = panelCenterX + rangeSlide;

			// helper to pick color for a given range value (neighbors use outlined colored string)
			#define RANGE_COLOR(v) \
				((isAudioOffset && (v) == gs.bgmGap) ? TEXT_COLOR_BLUE  : \
				 ((v) == m_items[i].savedValue        ? TEXT_COLOR_GREEN : TEXT_COLOR_WHITE))

			// center value
			{
				char buf[16];
				sprintf_s(buf, sizeof(buf), "%d", dispVal);
				int cw = (int)strlen(buf) * 10;
				renderOutlinedColoredString(buf, cx - cw / 2, optRowY, RANGE_COLOR(dispVal));
			}
			// left neighbors
			for ( int k = 1; k <= numH; k++ )
			{
				int v = dispVal - k * m_items[i].step;
				if ( v < m_items[i].minVal ) break;
				char buf[16];
				sprintf_s(buf, sizeof(buf), "%d", v);
				int kx = cx - k * slotW;
				renderOutlinedColoredString(buf, kx - (int)strlen(buf) * charW / 2, optRowY, RANGE_COLOR(v));
			}
			// right neighbors
			for ( int k = 1; k <= numH; k++ )
			{
				int v = dispVal + k * m_items[i].step;
				if ( v > m_items[i].maxVal ) break;
				char buf[16];
				sprintf_s(buf, sizeof(buf), "%d", v);
				int kx = cx + k * slotW;
				renderOutlinedColoredString(buf, kx - (int)strlen(buf) * charW / 2, optRowY, RANGE_COLOR(v));
			}

			#undef RANGE_COLOR

			if ( isSelected && m_isEditingItem )
			{
				int phase     = (int)(m_bobTimer % 600);
				int bobOffset = (phase < 300) ? (phase * 3 / 300) : ((600 - phase) * 3 / 300);
				int triCy     = optRowY + 7;
				int blue      = makecol(0, 180, 255);

				if ( dispVal > m_items[i].minVal )
					drawNavArrow(panelLeft + 5 - bobOffset, triCy, 0, 5, 6, blue);
				if ( dispVal < m_items[i].maxVal )
					drawNavArrow(panelRight - 5 + bobOffset, triCy, 1, 5, 6, blue);
			}
		}

		// bobbing confirm triangle (gold, pointing up, below option row) — disappears on confirm
		if ( isSelected && m_isEditingItem )
		{
			int phase     = (int)(m_bobTimer % 600);
			int bobOffset = (phase < 300) ? (phase * 3 / 300) : ((600 - phase) * 3 / 300);
			drawNavArrow(panelCenterX, optRowY + 12 + bobOffset, 2, 5, 5, makecol(255, 215, 0));
		}
	}
	} // end rep loop

	// expert mode: yellow scroll indicators at top and bottom of panel (hidden while editing a value)
	if ( m_isAdvanced && !m_isEditingItem )
	{
		int bpmPeriod = 600;
		int songIdx   = songID_to_listID(gs.currentSong);
		if ( songIdx >= 0 && songs != NULL && songs[songIdx].minBPM > 0 )
			bpmPeriod = 60000 / songs[songIdx].minBPM;

		int phase     = (int)(m_bobTimer % (UTIME)bpmPeriod);
		int half      = bpmPeriod / 2;
		int bobOffset = (half > 0) ? ((phase < half) ? (phase * 4 / half) : ((bpmPeriod - phase) * 4 / half)) : 0;
		int yellow    = makecol(255, 220, 0);

		drawNavArrow(panelCenterX, 8 - bobOffset,                  2, 8, 10, yellow); // up at top
		drawNavArrow(panelCenterX, SCREEN_HEIGHT - 8 + bobOffset,  3, 8, 10, yellow); // down at bottom
	}

	// restore clip rect and alpha blending mode
	set_clip_rect(rm.m_backbuf, savedCX1, savedCY1, savedCX2, savedCY2);
	drawing_mode(DRAW_MODE_TRANS, NULL, 0, 0);
	set_alpha_blender();
}
