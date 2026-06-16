// settingsMenu.h — per-player settings panel, opened in the songwheel via LEFT+RIGHT+START

#ifndef _SETTINGSMENU_H_
#define _SETTINGSMENU_H_

#include "../headers/common.h"

enum SettingsItemType { SETTINGS_LIST, SETTINGS_RANGE };

enum SettingsDependency
{
	DEP_NONE,           // always visible
	DEP_SCROLL_CLASSIC, // visible only when scrollMode == 0
	DEP_SCROLL_FIXED,   // visible only when scrollMode == 1
	DEP_JUDGMENT_ON,    // visible only when judgementPositionMode != 0
	DEP_PLAY_POSITION,  // visible in singles or freestyle; not available in doubles or versus
};

struct SettingsItem
{
	const char*        name;
	SettingsItemType   type;
	SettingsDependency dependency;

	// LIST type
	const char**       options;
	const int*         optionValues;
	int                optionCount;
	int                optionSlotW;  // 0 = use global SETTINGS_OPTION_SLOT_W

	// RANGE type
	int                minVal;
	int                maxVal;
	int                step;

	// common
	int*               value;        // pointer into sm.player[p] backing field
	int                savedValue;   // snapshot at menu-open time (for green/white color)
	bool*              flagToSetOnChange; // if non-null, set to true when value changes (for hasCustomAudioOffset)
};

class SettingsMenu
{
public:
	void open(int playerData, int side);
	void close();
	void forceClose();
	void resetSettings(int playerData); // call at credit start: resets cursor and applies mode defaults to gs.player
	bool isOpen() const;
	bool isFullyClosed() const;
	bool isEditing() const;
	void handleInput(UTIME dt);
	void render(UTIME dt);

private:
	void buildItemList(int player);
	bool isItemVisible(int index) const;
	void advanceSelectionIfHidden();

	int          m_player;     // visual/input side: 0=left panel+1P buttons, 1=right panel+2P buttons
	int          m_playerData; // which sm.player[] slot holds the settings (may differ from m_player in doubles)
	SettingsItem m_items[20];
	int          m_itemCount;
	bool         m_isAdvanced;
	int          m_selectedItem;
	bool         m_isEditingItem;
	int          m_slideOffsetX;
	bool         m_isClosing;
	bool         m_isOpen;
	UTIME        m_slideTimer;
	int          m_optionSlideOffset;
	UTIME        m_optionSlideTimer;
	int          m_optionSlideDir;
	int          m_holdDir;      // -1=left, 0=none, 1=right (hold-to-repeat in edit mode)
	int          m_holdTime;     // ms current direction has been held
	int          m_repeatTimer;  // ms until next auto-repeat fires
	int          m_scrollY;      // current animated scroll pixel offset
	int          m_targetScrollY;// target scroll offset (snaps to keep selected item visible)
	int          m_selectorPanelY;       // animated selector Y in panel-space (before scroll subtraction)
	int          m_targetSelectorPanelY; // destination for selector slide animation
	bool         m_snapSelector;         // true when selector should snap instead of slide (wrap-around)
	UTIME        m_bobTimer;             // drives the triangle bob animation in edit mode
	int          m_cancelHoldTimer;      // ms L+R have been held during edit mode (2000 = cancel)
	int          m_lastNavTimer;         // ms since last navigation input (for chord detection)
	int          m_lastNavDir;           // direction of last nav: -1=up, +1=down, 0=none
	int          m_activeSlotW;          // slot width of the item currently being slide-animated
};

#endif
