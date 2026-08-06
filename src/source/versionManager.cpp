// version.cpp implements a function for telling the compile tile version of the program
// source file created by Allen Seitz 6/6/2016

#include "../headers/common.h"
#include "../headers/versionManager.h"
#include "../headers/releaseTag.h"

void VersionManager::initialize()
{
	versionString[0] = 0;
	strcpy_s(versionString, 128, __DATE__);
	strcat_s(versionString, 128, " ");

	// __TIME__ is "HH:MM:SS" - swap in spaces to match the "Month DD YYYY HH MM SS" display format
	char timeBuf[16] = "";
	strcpy_s(timeBuf, sizeof(timeBuf), __TIME__);
	for (char* p = timeBuf; *p != 0; p++)
	{
		if (*p == ':') *p = ' ';
	}
	strcat_s(versionString, 128, timeBuf);

	if (strlen(DMX_RELEASE_TAG) == 0)
	{
		// local/dev build - tag on which configuration produced it, so builds floating around
		// during testing are distinguishable at a glance (shown both on the boot screen's "DEV
		// BUILD" line and the attract mode title screen). Never appended for an official CI build,
		// which already carries its real DMX_RELEASE_TAG for identification instead.
#if defined(DMXDEBUG)
		strcat_s(versionString, 128, "-dbg");
#elif defined(DMXDEV)
		strcat_s(versionString, 128, "-dev");
#else
		strcat_s(versionString, 128, "-prod");
#endif
	}

	currentVersionInSeconds = convert_DATE(__DATE__);
}

time_t VersionManager::convert_DATE(char const *time)
{ 
    char s_month[5] = "";
    int month, day, year;
    struct tm t = {0};
    static const char month_names[] = "JanFebMarAprMayJunJulAugSepOctNovDec";

    sscanf_s(time, "%s %d %d", s_month, 5, &day, &year);

    month = (strstr(month_names, s_month)-month_names)/3;

    currentVersionMonth = t.tm_mon = month + 1;
    currentVersionDay = t.tm_mday = day;
    currentVersionYear = t.tm_year = year - 2000;
    t.tm_isdst = -1;

    return mktime(&t);
}