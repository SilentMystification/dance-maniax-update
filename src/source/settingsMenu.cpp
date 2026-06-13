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
#define SETTINGS_HIGHLIGHT_COLOR makeacol(255, 170, 0, 255) // goldish-orange selection border
#define SETTINGS_PANEL_WIDTH     256
#define SETTINGS_SLIDE_MS        200
#define SETTINGS_OPTION_SLIDE_MS 80
#define SETTINGS_ITEM_HEIGHT     54
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
	m_scrollY           = 0;
	m_targetScrollY     = 0;

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

void SettingsMenu::forceClose()
{
	// revert any in-progress (unconfirmed) edit back to the last confirmed value
	if ( m_isEditingItem && m_selectedItem >= 0 && m_selectedItem < m_itemCount )
	{
		*m_items[m_selectedItem].value = m_items[m_selectedItem].savedValue;
	}
	m_isOpen    = false;
	m_isClosing = false;
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
			// commit: record the newly confirmed value so it renders green
			m_items[m_selectedItem].savedValue = *m_items[m_selectedItem].value;
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

	// ---- compute scroll target to keep selected item fully in view ----
	int selectedVisibleIdx = 0;
	int totalVisible       = 0;
	for ( int i = 0; i < m_itemCount; i++ )
	{
		if ( !isItemVisible(i) ) continue;
		if ( i < m_selectedItem ) selectedVisibleIdx++;
		totalVisible++;
	}

	int visibleHeight = SCREEN_HEIGHT - SETTINGS_START_Y;
	int selTop        = selectedVisibleIdx * SETTINGS_ITEM_HEIGHT;
	int selBottom     = selTop + SETTINGS_ITEM_HEIGHT;

	if ( selBottom > m_targetScrollY + visibleHeight ) m_targetScrollY = selBottom - visibleHeight;
	if ( selTop    < m_targetScrollY                 ) m_targetScrollY = selTop;

	int maxScroll = MAX(0, totalVisible * SETTINGS_ITEM_HEIGHT - visibleHeight);
	if ( m_targetScrollY < 0         ) m_targetScrollY = 0;
	if ( m_targetScrollY > maxScroll ) m_targetScrollY = maxScroll;

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

	// pre-pass: find screen Y of the selected item (used to draw highlight before clip rect)
	int selectedItemScreenY = -9999;
	{
		int preY = SETTINGS_START_Y - m_scrollY;
		for ( int i = 0; i < m_itemCount; i++ )
		{
			if ( !isItemVisible(i) ) continue;
			if ( i == m_selectedItem ) { selectedItemScreenY = preY; break; }
			preY += SETTINGS_ITEM_HEIGHT;
		}
	}

	// gold/orange 2px highlight box — drawn before clip rect so all 4 sides are visible,
	// inset from the 2px black border so it doesn't get overwritten
	if ( selectedItemScreenY > -9999 )
	{
		int hx1 = panelLeft  + 2, hy1 = selectedItemScreenY;
		int hx2 = panelRight - 2, hy2 = selectedItemScreenY + SETTINGS_ITEM_HEIGHT - 2;
		rect(rm.m_backbuf, hx1,     hy1,     hx2,     hy2,     SETTINGS_HIGHLIGHT_COLOR);
		rect(rm.m_backbuf, hx1 + 1, hy1 + 1, hx2 - 1, hy2 - 1, SETTINGS_HIGHLIGHT_COLOR);
	}

	// clip to panel inner area so scrolled items don't overdraw the border
	int savedCX1, savedCY1, savedCX2, savedCY2;
	get_clip_rect(rm.m_backbuf, &savedCX1, &savedCY1, &savedCX2, &savedCY2);
	set_clip_rect(rm.m_backbuf, panelLeft + 2, 0, panelRight - 2, SCREEN_HEIGHT - 1);

	// ---- draw items ----
	int visItemY = SETTINGS_START_Y - m_scrollY;
	for ( int i = 0; i < m_itemCount; i++ )
	{
		if ( !isItemVisible(i) ) continue;

		int itemY = visItemY;
		visItemY += SETTINGS_ITEM_HEIGHT;

		// skip items fully off-screen
		if ( itemY + SETTINGS_ITEM_HEIGHT <= 0 || itemY >= SCREEN_HEIGHT ) continue;

		bool isSelected    = (i == m_selectedItem);
		bool isAudioOffset = (m_items[i].flagToSetOnChange != NULL);

		// item name — name font (32px per char), centered in panel; clip rect handles overflow
		int titleX = panelLeft + SETTINGS_PANEL_WIDTH / 2 - (int)strlen(m_items[i].name) * 16;
		renderNameString(m_items[i].name, titleX, itemY + 4, 0);

		// option row sits below the 32px name glyphs (4px top + 32px glyph + 4px gap = 40)
		int slideOff = (isSelected && m_isEditingItem) ? m_optionSlideOffset : 0;
		int optRowY  = itemY + 40;

		if ( m_items[i].type == SETTINGS_LIST )
		{
			// find current option index
			int idx = 0;
			for ( int j = 0; j < m_items[i].optionCount; j++ )
			{
				if ( m_items[i].optionValues[j] == *m_items[i].value ) { idx = j; break; }
			}

			// current option: green if confirmed (savedValue), white if browsing
			int curColor = (m_items[i].optionValues[idx] == m_items[i].savedValue)
				? TEXT_COLOR_GREEN : TEXT_COLOR_WHITE;
			const char* curLabel = m_items[i].options[idx];
			int cx = panelCenterX + slideOff;
			int tw = (int)strlen(curLabel) * 10;
			renderOutlinedColoredString(curLabel, cx - tw/2, optRowY, curColor);

			// previous option: green if it's the savedValue, white otherwise
			if ( idx > 0 )
			{
				const char* prevLabel = m_items[i].options[idx - 1];
				int prevColor = (m_items[i].optionValues[idx - 1] == m_items[i].savedValue)
					? TEXT_COLOR_GREEN : TEXT_COLOR_WHITE;
				int px = panelCenterX - SETTINGS_OPTION_SLOT_W + slideOff;
				int pw = (int)strlen(prevLabel) * 10;
				renderOutlinedColoredString(prevLabel, px - pw/2, optRowY, prevColor);
			}
			// next option: green if it's the savedValue, white otherwise
			if ( idx < m_items[i].optionCount - 1 )
			{
				const char* nextLabel = m_items[i].options[idx + 1];
				int nextColor = (m_items[i].optionValues[idx + 1] == m_items[i].savedValue)
					? TEXT_COLOR_GREEN : TEXT_COLOR_WHITE;
				int nx = panelCenterX + SETTINGS_OPTION_SLOT_W + slideOff;
				int nw = (int)strlen(nextLabel) * 10;
				renderOutlinedColoredString(nextLabel, nx - nw/2, optRowY, nextColor);
			}

			// navigation arrows in edit mode
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

			const int charW = 10;
			int slotW  = maxChars * charW + 4;                  // slot width with minimal gap
			int availW = SETTINGS_PANEL_WIDTH * 70 / 100;
			int numH   = MAX(1, (availW - slotW) / (2 * slotW)); // neighbors on each side

			// scale slide animation to match computed slot width
			int rangeSlide = (isSelected && m_isEditingItem)
				? (m_optionSlideOffset * slotW / SETTINGS_OPTION_SLOT_W) : 0;
			int cx = panelCenterX + rangeSlide;

			// helper to pick color for a given range value
			#define RANGE_COLOR(v) \
				((isAudioOffset && (v) == gs.bgmGap) ? TEXT_COLOR_BLUE  : \
				 ((v) == m_items[i].savedValue        ? TEXT_COLOR_GREEN : TEXT_COLOR_WHITE))

			// center value
			{
				char buf[16];
				sprintf_s(buf, sizeof(buf), "%d", dispVal);
				renderOutlinedColoredString(buf, cx - (int)strlen(buf) * charW / 2, optRowY, RANGE_COLOR(dispVal));
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
				if ( dispVal > m_items[i].minVal )
					renderOutlinedColoredString("<", panelLeft + 6, optRowY, TEXT_COLOR_WHITE);
				if ( dispVal < m_items[i].maxVal )
					renderOutlinedColoredString(">", panelRight - 14, optRowY, TEXT_COLOR_WHITE);
			}
		}
	}

	// restore clip rect and alpha blending mode
	set_clip_rect(rm.m_backbuf, savedCX1, savedCY1, savedCX2, savedCY2);
	drawing_mode(DRAW_MODE_TRANS, NULL, 0, 0);
	set_alpha_blender();
}
