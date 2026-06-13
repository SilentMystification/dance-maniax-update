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

//////////////////////////////////////////////////////////////////////////////
// Visual constants — tweak these without touching logic
//////////////////////////////////////////////////////////////////////////////
#define SETTINGS_BG_COLOR        makecol(70, 8, 128)
#define SETTINGS_HIGHLIGHT_COLOR makecol(255, 255, 255)
#define SETTINGS_PANEL_WIDTH     256
#define SETTINGS_SLIDE_MS        200
#define SETTINGS_OPTION_SLIDE_MS 80
#define SETTINGS_ITEM_HEIGHT     46
#define SETTINGS_START_Y         16
#define SETTINGS_OPTION_SLOT_W   80  // width of each option slot in the option row

#define HOLD_INITIAL_DELAY    400   // ms before auto-repeat begins after initial press
#define HOLD_INTERVAL_START   500   // ms between repeats at start (2/sec)
#define HOLD_INTERVAL_END     125   // ms between repeats at full speed (8/sec)
#define HOLD_RAMP_DURATION   1500   // ms to ramp from start rate to full speed

//////////////////////////////////////////////////////////////////////////////
// Static option label arrays
//////////////////////////////////////////////////////////////////////////////
static const char* s_scoreModeOptions[]    = { "Classic", "Expert" };
static const int   s_scoreModeValues[]     = { 0, 1 };

static const char* s_scrollModeOptions[]   = { "Classic", "Fixed" };
static const int   s_scrollModeValues[]    = { 0, 1 };

static const char* s_classicSpeedOptions[] = { "1x","1.5x","2x","2.5x","3x","3.5x","4x","5x","6x","7x","8x" };
static const int   s_classicSpeedValues[]  = { 10, 15, 20, 25, 30, 35, 40, 50, 60, 70, 80 };

static const char* s_judgPosOptions[]      = { "Off","Bottom","Lower","Upper","Top" };
static const int   s_judgPosValues[]       = { 0, 1, 2, 3, 4 };

static const char* s_judgTextOptions[]     = { "All","Perfect+","Great+","Good+" };
static const int   s_judgTextValues[]      = { 0, 1, 2, 3 };

static const char* s_judgMsOptions[]       = { "All","Perfect+","Great+","Good+","Never" };
static const int   s_judgMsValues[]        = { 0, 1, 2, 3, 4 };

//////////////////////////////////////////////////////////////////////////////
// SettingsMenu implementation
//////////////////////////////////////////////////////////////////////////////

void SettingsMenu::buildItemList(int player)
{
	m_itemCount = 0;
	struct PLAYER_DATA& p = sm.player[player];

	// 1. Score Mode
	m_items[m_itemCount].name             = "Score Mode";
	m_items[m_itemCount].type             = SETTINGS_LIST;
	m_items[m_itemCount].dependency       = DEP_NONE;
	m_items[m_itemCount].options          = s_scoreModeOptions;
	m_items[m_itemCount].optionValues     = s_scoreModeValues;
	m_items[m_itemCount].optionCount      = 2;
	m_items[m_itemCount].value            = &p.scoreMode;
	m_items[m_itemCount].flagToSetOnChange= NULL;
	m_itemCount++;

	// 2. Scroll Mode
	m_items[m_itemCount].name             = "Scroll Mode";
	m_items[m_itemCount].type             = SETTINGS_LIST;
	m_items[m_itemCount].dependency       = DEP_NONE;
	m_items[m_itemCount].options          = s_scrollModeOptions;
	m_items[m_itemCount].optionValues     = s_scrollModeValues;
	m_items[m_itemCount].optionCount      = 2;
	m_items[m_itemCount].value            = &p.scrollMode;
	m_items[m_itemCount].flagToSetOnChange= NULL;
	m_itemCount++;

	// 3. Classic Speed (visible when Classic scroll mode)
	m_items[m_itemCount].name             = "Classic Speed";
	m_items[m_itemCount].type             = SETTINGS_LIST;
	m_items[m_itemCount].dependency       = DEP_SCROLL_CLASSIC;
	m_items[m_itemCount].options          = s_classicSpeedOptions;
	m_items[m_itemCount].optionValues     = s_classicSpeedValues;
	m_items[m_itemCount].optionCount      = 11;
	m_items[m_itemCount].value            = &p.speedMod;
	m_items[m_itemCount].flagToSetOnChange= NULL;
	m_itemCount++;

	// 4. Fixed Scroll Speed (visible when Fixed scroll mode)
	m_items[m_itemCount].name             = "Fixed Speed";
	m_items[m_itemCount].type             = SETTINGS_RANGE;
	m_items[m_itemCount].dependency       = DEP_SCROLL_FIXED;
	m_items[m_itemCount].minVal           = 5;
	m_items[m_itemCount].maxVal           = 1000;
	m_items[m_itemCount].step             = 5;
	m_items[m_itemCount].value            = &p.fixedScrollPPS;
	m_items[m_itemCount].flagToSetOnChange= NULL;
	m_itemCount++;

	// 5. Audio Offset (wiring deferred; hasCustomAudioOffset flag set on change)
	m_items[m_itemCount].name             = "Audio Offset";
	m_items[m_itemCount].type             = SETTINGS_RANGE;
	m_items[m_itemCount].dependency       = DEP_NONE;
	m_items[m_itemCount].minVal           = -500;
	m_items[m_itemCount].maxVal           = 500;
	m_items[m_itemCount].step             = 1;
	m_items[m_itemCount].value            = &p.audioOffset;
	m_items[m_itemCount].flagToSetOnChange= &p.hasCustomAudioOffset;
	m_itemCount++;

	// 6. Visual Offset (wiring deferred)
	m_items[m_itemCount].name             = "Visual Offset";
	m_items[m_itemCount].type             = SETTINGS_RANGE;
	m_items[m_itemCount].dependency       = DEP_NONE;
	m_items[m_itemCount].minVal           = -500;
	m_items[m_itemCount].maxVal           = 500;
	m_items[m_itemCount].step             = 1;
	m_items[m_itemCount].value            = &p.visualOffset;
	m_items[m_itemCount].flagToSetOnChange= NULL;
	m_itemCount++;

	// 7. Judgment Display Position
	m_items[m_itemCount].name             = "Judgment Position";
	m_items[m_itemCount].type             = SETTINGS_LIST;
	m_items[m_itemCount].dependency       = DEP_NONE;
	m_items[m_itemCount].options          = s_judgPosOptions;
	m_items[m_itemCount].optionValues     = s_judgPosValues;
	m_items[m_itemCount].optionCount      = 5;
	m_items[m_itemCount].value            = &p.judgementPositionMode;
	m_items[m_itemCount].flagToSetOnChange= NULL;
	m_itemCount++;

	// 8. Judgment Display Text (visible when Position != Off)
	m_items[m_itemCount].name             = "Judgment Text";
	m_items[m_itemCount].type             = SETTINGS_LIST;
	m_items[m_itemCount].dependency       = DEP_JUDGMENT_ON;
	m_items[m_itemCount].options          = s_judgTextOptions;
	m_items[m_itemCount].optionValues     = s_judgTextValues;
	m_items[m_itemCount].optionCount      = 4;
	m_items[m_itemCount].value            = &p.judgementEarlyLateMode;
	m_items[m_itemCount].flagToSetOnChange= NULL;
	m_itemCount++;

	// 9. Judgment Display MS (visible when Position != Off)
	m_items[m_itemCount].name             = "Judgment MS";
	m_items[m_itemCount].type             = SETTINGS_LIST;
	m_items[m_itemCount].dependency       = DEP_JUDGMENT_ON;
	m_items[m_itemCount].options          = s_judgMsOptions;
	m_items[m_itemCount].optionValues     = s_judgMsValues;
	m_items[m_itemCount].optionCount      = 5;
	m_items[m_itemCount].value            = &p.judgementMsDisplayMode;
	m_items[m_itemCount].flagToSetOnChange= NULL;
	m_itemCount++;
}

bool SettingsMenu::isItemVisible(int index) const
{
	const SettingsItem& item = m_items[index];
	switch ( item.dependency )
	{
	case DEP_NONE:
		return true;
	case DEP_SCROLL_CLASSIC:
		return sm.player[m_player].scrollMode == 0;
	case DEP_SCROLL_FIXED:
		return sm.player[m_player].scrollMode == 1;
	case DEP_JUDGMENT_ON:
		return sm.player[m_player].judgementPositionMode != 0;
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

void SettingsMenu::open(int player)
{
	m_player       = player;
	m_isOpen       = true;
	m_isClosing    = false;
	m_slideTimer   = 0;
	m_slideOffsetX = (player == 0) ? -SETTINGS_PANEL_WIDTH : SCREEN_WIDTH;
	m_selectedItem = 0;
	m_isEditingItem= false;
	m_optionSlideOffset = 0;
	m_optionSlideTimer  = 0;
	m_optionSlideDir    = 0;
	m_holdDir           = 0;
	m_holdTime          = 0;
	m_repeatTimer       = 0;

	buildItemList(player);

	// snapshot saved values at open time
	for ( int i = 0; i < m_itemCount; i++ )
	{
		m_items[i].savedValue = *m_items[i].value;
	}
	// audio offset: if not custom, show bgmGap as the "saved" value
	// (item index 4 is Audio Offset)
	if ( !sm.player[player].hasCustomAudioOffset )
	{
		m_items[4].savedValue = gs.bgmGap;
		*m_items[4].value     = gs.bgmGap;
	}

	advanceSelectionIfHidden();
}

void SettingsMenu::close()
{
	if ( !m_isClosing )
	{
		m_isClosing  = true;
		m_slideTimer = 0;
	}
}

bool SettingsMenu::isOpen() const
{
	return m_isOpen && !m_isClosing;
}

bool SettingsMenu::isFullyClosed() const
{
	return !m_isOpen;
}

bool SettingsMenu::isEditing() const
{
	return m_isEditingItem;
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

	// LEFT+RIGHT (both held) closes menu — handled by caller, not here
	if ( bothDown ) return;

	if ( !m_isEditingItem )
	{
		// outer mode: navigate items (tap only)
		m_holdDir = 0; m_holdTime = 0; m_repeatTimer = 0;

		if ( leftDown )
		{
			// move up
			for ( int i = 1; i <= m_itemCount; i++ )
			{
				int candidate = (m_selectedItem - i + m_itemCount) % m_itemCount;
				if ( isItemVisible(candidate) )
				{
					m_selectedItem = candidate;
					break;
				}
			}
		}
		else if ( rightDown )
		{
			// move down
			for ( int i = 1; i <= m_itemCount; i++ )
			{
				int candidate = (m_selectedItem + i) % m_itemCount;
				if ( isItemVisible(candidate) )
				{
					m_selectedItem = candidate;
					break;
				}
			}
		}
		else if ( startDown )
		{
			m_isEditingItem = true;
			m_optionSlideOffset = 0;
		}
	}
	else
	{
		// inner mode: adjust value with hold-to-repeat
		SettingsItem& item = m_items[m_selectedItem];

		if ( startDown )
		{
			m_isEditingItem = false;
			m_holdDir = 0; m_holdTime = 0; m_repeatTimer = 0;
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
			if ( item.flagToSetOnChange != NULL )
			{
				*item.flagToSetOnChange = true;
			}
			m_optionSlideOffset = m_optionSlideDir * SETTINGS_OPTION_SLOT_W;
			m_optionSlideTimer  = SETTINGS_OPTION_SLIDE_MS;
		}
	}
}

void SettingsMenu::render(UTIME dt)
{
	// update slide timer
	m_slideTimer += dt;
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
		m_optionSlideOffset = m_optionSlideDir * getValueFromRange(0, SETTINGS_OPTION_SLOT_W, remainPct);
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

	int panelLeft  = m_slideOffsetX;
	int panelRight = m_slideOffsetX + SETTINGS_PANEL_WIDTH - 1;
	int panelCenterX = panelLeft + SETTINGS_PANEL_WIDTH / 2;

	// draw panel background (solid mode so the panel is fully opaque)
	solid_mode();
	rectfill(rm.m_backbuf, panelLeft, 0, panelRight, SCREEN_HEIGHT - 1, SETTINGS_BG_COLOR);

	// draw items
	int itemY = SETTINGS_START_Y;
	for ( int i = 0; i < m_itemCount; i++ )
	{
		if ( !isItemVisible(i) ) continue;

		bool isSelected = (i == m_selectedItem);

		// highlight box for selected item
		if ( isSelected )
		{
			rect(rm.m_backbuf,
				panelLeft + 1,       itemY - 1,
				panelRight - 1,      itemY + SETTINGS_ITEM_HEIGHT - 2,
				SETTINGS_HIGHLIGHT_COLOR);
		}

		// item name
		renderOutlinedColoredString(m_items[i].name, panelLeft + 4, itemY + 3, TEXT_COLOR_WHITE);

		// determine option color: green if matches saved, white if changed
		int valueColor = (*m_items[i].value == m_items[i].savedValue) ? TEXT_COLOR_GREEN : TEXT_COLOR_WHITE;

		// option row — offset by slide animation only on selected item
		int slideOff = (isSelected && m_isEditingItem) ? m_optionSlideOffset : 0;
		int optRowY  = itemY + 18;

		if ( m_items[i].type == SETTINGS_LIST )
		{
			// find current option index
			int idx = 0;
			for ( int j = 0; j < m_items[i].optionCount; j++ )
			{
				if ( m_items[i].optionValues[j] == *m_items[i].value ) { idx = j; break; }
			}

			// center option
			const char* curLabel = m_items[i].options[idx];
			int cx = panelCenterX + slideOff;
			int tw = (int)strlen(curLabel) * 10; // font is 10px wide
			renderOutlinedColoredString(curLabel, cx - tw/2, optRowY, valueColor);

			// previous option (to the left)
			if ( idx > 0 )
			{
				const char* prevLabel = m_items[i].options[idx - 1];
				int px = panelCenterX - SETTINGS_OPTION_SLOT_W + slideOff;
				int pw = (int)strlen(prevLabel) * 10;
				renderOutlinedColoredString(prevLabel, px - pw/2, optRowY, TEXT_COLOR_WHITE);
			}
			// next option (to the right)
			if ( idx < m_items[i].optionCount - 1 )
			{
				const char* nextLabel = m_items[i].options[idx + 1];
				int nx = panelCenterX + SETTINGS_OPTION_SLOT_W + slideOff;
				int nw = (int)strlen(nextLabel) * 10;
				renderOutlinedColoredString(nextLabel, nx - nw/2, optRowY, TEXT_COLOR_WHITE);
			}

			// navigation arrows when in edit mode
			if ( isSelected && m_isEditingItem )
			{
				if ( idx > 0 )
					renderOutlinedColoredString("<", panelLeft + 6, optRowY, TEXT_COLOR_WHITE);
				if ( idx < m_items[i].optionCount - 1 )
					renderOutlinedColoredString(">", panelRight - 14, optRowY, TEXT_COLOR_WHITE);
			}
		}
		else // SETTINGS_RANGE
		{
			char buf[16];
			int  dispVal = *m_items[i].value;

			// audio offset special case: show bgmGap when not yet customized
			if ( m_items[i].flagToSetOnChange != NULL && !(*m_items[i].flagToSetOnChange) )
			{
				dispVal = gs.bgmGap;
			}

			sprintf_s(buf, sizeof(buf), "%d", dispVal);
			int tw = (int)strlen(buf) * 10;
			int cx = panelCenterX + slideOff;
			renderOutlinedColoredString(buf, cx - tw/2, optRowY, valueColor);

			if ( isSelected && m_isEditingItem )
			{
				if ( dispVal > m_items[i].minVal )
					renderOutlinedColoredString("<", panelLeft + 6, optRowY, TEXT_COLOR_WHITE);
				if ( dispVal < m_items[i].maxVal )
					renderOutlinedColoredString(">", panelRight - 14, optRowY, TEXT_COLOR_WHITE);
			}
		}

		itemY += SETTINGS_ITEM_HEIGHT;
	}

	drawing_mode(DRAW_MODE_TRANS, NULL, 0, 0);
	set_alpha_blender(); // restore drawing mode expected by surrounding rendering code
}
