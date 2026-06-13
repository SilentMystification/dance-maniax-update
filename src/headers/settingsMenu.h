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
	void open(int player);
	void close();
	bool isOpen() const;
	bool isFullyClosed() const;
	bool isEditing() const;
	void handleInput(UTIME dt);
	void render(UTIME dt);

private:
	void buildItemList(int player);
	bool isItemVisible(int index) const;
	void advanceSelectionIfHidden();

	int          m_player;
	SettingsItem m_items[12];
	int          m_itemCount;
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
};

#endif
