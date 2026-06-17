// common.cpp implements the functions described in common.h
// source file created

#include "common.h"
#include <stdio.h>
#include <string.h>
#include "../headers/gameStateManager.h" // for error mode, is all
#include "../lib/allegro-4.4.2-msvc-10.0/include/loadpng.h"	// extra functions for PNG support
#include "../headers/analyticsManager.h"

extern RenderingManager rm;
extern GameStateManager gs;
extern AnalyticsManager am;

extern int NUM_SONGS;
extern bool vsyncEnabled;
extern int* songIDs;
extern std::string* songTitles;
extern std::string* songArtists;
extern std::string* movieScripts;

void RenderingManager::Initialize(bool installMode, int windowWidth, int windowHeight, bool widescreenPillars)
{
	useAlphaLanes = true;

	screenWidth = windowWidth;
	screenHeight = windowHeight;
	pillarboxMode = widescreenPillars;

	// set up double buffering
	m_backbuf1 = create_system_bitmap(SCREEN_WIDTH, SCREEN_HEIGHT); // source material is still 640x480, so use the old constants
	m_backbuf2 = create_system_bitmap(SCREEN_WIDTH, SCREEN_HEIGHT);
	clear_to_color(m_backbuf1, 0);
	clear_to_color(m_backbuf2, 0);

	m_temp64 = create_bitmap(64, 64);
	currentPage = 1;
	m_backbuf = rm.m_backbuf1;

	// load other stuff - these can be NULL during the install process!
	if ( !installMode )
	{
		m_whiteFont = loadImage("DATA/etc/white_font.bmp");

		for ( int i = 0; i < 16; i++ )
			m_colorFont[i] = loadImage("DATA/etc/white_font.bmp");
		// tint each copy: indices 0-3 match boldFont palette; magenta uses (255,0,220) to avoid Allegro's mask color (255,0,255)
		replaceColor(m_colorFont[TEXT_COLOR_GREEN],   makecol(255,255,255), makecol(170,255,170));
		replaceColor(m_colorFont[TEXT_COLOR_RED],     makecol(255,255,255), makecol(255,170,170));
		replaceColor(m_colorFont[TEXT_COLOR_BLUE],    makecol(255,255,255), makecol(170,170,255));
		replaceColor(m_colorFont[TEXT_COLOR_CYAN],    makecol(255,255,255), makecol(0,255,255));
		replaceColor(m_colorFont[TEXT_COLOR_MAGENTA], makecol(255,255,255), makecol(255,0,220));
		replaceColor(m_colorFont[TEXT_COLOR_YELLOW],  makecol(255,255,255), makecol(255,255,0));
		replaceColor(m_colorFont[TEXT_COLOR_BLACK],   makecol(255,255,255), makecol(0,0,0));
		replaceColor(m_colorFont[TEXT_COLOR_GOLD],    makecol(255,255,255), makecol(212,175,55));
		replaceColor(m_colorFont[TEXT_COLOR_FRED],    makecol(255,255,255), makecol(255,0,0));
		replaceColor(m_colorFont[TEXT_COLOR_FGREEN],  makecol(255,255,255), makecol(0,255,0));
		replaceColor(m_colorFont[TEXT_COLOR_FBLUE],   makecol(255,255,255), makecol(0,0,255));
		replaceColor(m_colorFont[TEXT_COLOR_HRED],    makecol(255,255,255), makecol(255,85,85));
		replaceColor(m_colorFont[TEXT_COLOR_HGREEN],  makecol(255,255,255), makecol(85,255,85));
		replaceColor(m_colorFont[TEXT_COLOR_HBLUE],   makecol(255,255,255), makecol(85,85,255));
		replaceColor(m_colorFont[TEXT_COLOR_GREY],    makecol(255,255,255), makecol(192,192,192));
		for ( int i = 0; i < 16; i++ )
			outlineBitmap(m_colorFont[i]);

		for ( int i = 0; i < 16; i++ )
			m_colorFontNoOutline[i] = loadImage("DATA/etc/white_font.bmp");
		replaceColor(m_colorFontNoOutline[TEXT_COLOR_GREEN],   makecol(255,255,255), makecol(170,255,170));
		replaceColor(m_colorFontNoOutline[TEXT_COLOR_RED],     makecol(255,255,255), makecol(255,170,170));
		replaceColor(m_colorFontNoOutline[TEXT_COLOR_BLUE],    makecol(255,255,255), makecol(170,170,255));
		replaceColor(m_colorFontNoOutline[TEXT_COLOR_CYAN],    makecol(255,255,255), makecol(0,255,255));
		replaceColor(m_colorFontNoOutline[TEXT_COLOR_MAGENTA], makecol(255,255,255), makecol(255,0,220));
		replaceColor(m_colorFontNoOutline[TEXT_COLOR_YELLOW],  makecol(255,255,255), makecol(255,255,0));
		replaceColor(m_colorFontNoOutline[TEXT_COLOR_BLACK],   makecol(255,255,255), makecol(0,0,0));
		replaceColor(m_colorFontNoOutline[TEXT_COLOR_GOLD],    makecol(255,255,255), makecol(212,175,55));
		replaceColor(m_colorFontNoOutline[TEXT_COLOR_FRED],    makecol(255,255,255), makecol(255,0,0));
		replaceColor(m_colorFontNoOutline[TEXT_COLOR_FGREEN],  makecol(255,255,255), makecol(0,255,0));
		replaceColor(m_colorFontNoOutline[TEXT_COLOR_FBLUE],   makecol(255,255,255), makecol(0,0,255));
		replaceColor(m_colorFontNoOutline[TEXT_COLOR_HRED],    makecol(255,255,255), makecol(255,85,85));
		replaceColor(m_colorFontNoOutline[TEXT_COLOR_HGREEN],  makecol(255,255,255), makecol(85,255,85));
		replaceColor(m_colorFontNoOutline[TEXT_COLOR_HBLUE],   makecol(255,255,255), makecol(85,85,255));
		replaceColor(m_colorFontNoOutline[TEXT_COLOR_GREY],    makecol(255,255,255), makecol(192,192,192));

		m_textFont[0] = loadImage("DATA/etc/text_font.bmp");
		m_textFont[1] = loadImage("DATA/etc/text_font.bmp");
		m_textFont[2] = loadImage("DATA/etc/text_font.bmp");
		m_textFont[3] = loadImage("DATA/etc/text_font.bmp");
		replaceColor(m_textFont[1], makecol(255,255,255), makecol(170,255,170));
		replaceColor(m_textFont[2], makecol(255,255,255), makecol(255,170,170));
		replaceColor(m_textFont[3], makecol(255,255,255), makecol(170,170,255));
		m_boldFont[0] = loadImage("DATA/etc/bold_font.bmp");
		m_boldFont[1] = loadImage("DATA/etc/bold_font.bmp");
		m_boldFont[2] = loadImage("DATA/etc/bold_font.bmp");
		m_boldFont[3] = loadImage("DATA/etc/bold_font.bmp");
		replaceColor(m_boldFont[1], makecol(248,248,248), makecol(170,255,170));
		replaceColor(m_boldFont[2], makecol(248,248,248), makecol(255,170,170));
		replaceColor(m_boldFont[3], makecol(248,248,248), makecol(170,170,255));
		m_artistFont = loadImage("DATA/etc/artist_font.bmp");
		m_scoreFont = loadImage("DATA/etc/score_font.bmp");
		m_nameFont[0] = loadImage("DATA/etc/name_font_white.bmp");
		m_nameFont[1] = loadImage("DATA/etc/name_font_green.bmp");
		m_nameFont[2] = loadImage("DATA/etc/name_font_red.bmp");
	}
}

int pickRandomInt(int n, ...)
{	
	va_list args;
	va_start(args, n);

	int retval = va_arg(args, int);
	int random = rand() % n;

	// loop a random portion of the way through the argument list
	for (int i = 0; i < random; i++)
	{
		retval = va_arg(args, int);
    }
	va_end(args);

	return retval;
}

int getValueFromRange(int min, int max, int percent)
{
	if ( max < min )
	{
		int maxrange = min - max;
		return min - (maxrange * percent / 100);
	}
	else
	{
		int minrange = max - min;
		return min + (minrange * percent / 100);
	}
}

void addLeadingZeros(char* buffer, int desiredLength)
{
	while ( (int)strlen(buffer) < desiredLength )
	{
		memmove_s(&buffer[1], desiredLength+1, buffer, strlen(buffer));
		buffer[0] = '0';
	}
}

void globalError(long errorCode, const char* errorInfo)
{
	gs.g_currentGameMode = ERRORMODE;
	gs.g_gameModeTransition = 1;
	gs.g_errorCode = errorCode;
	strcpy_s(gs.g_errorInfo, 256, errorInfo);

	am.logEvent(TRACK_CAT_ERROR, TRACK_EV_ERROR, gs.g_errorInfo, gs.g_errorCode);
}

void songIndexError(int id)
{
	if ( id < 0 )
	{
		id *= -1;
		globalError(INVALID_SONG_ID, "song id was -1");
		return;
	}

	char helpString[64] = "0000 song id";
	helpString[0] = (id / 1000 ) % 10 + '0';
	helpString[1] = (id / 100 ) % 10 + '0';
	helpString[2] = (id / 10 ) % 10 + '0';
	helpString[3] = (id / 1 ) % 10 + '0';
	globalError(INVALID_SONG_ID, helpString);
}

BITMAP* loadImage(const char* filename)
{
	BITMAP* temp = load_bitmap(filename, NULL);
	if ( temp == NULL )
	{
		allegro_message("Couldn't load %s", filename);
	}
	return temp;
}

FILE* safeLoadFile(char* filename)
{
	char checksum = calculateChecksum(filename);

	FILE* fp = NULL;
	if ( checksum != 0 || fopen_s(&fp, filename, "rbc") != 0 )
	{
		// uh-oh! file problem. is there a backup?
		char bakFilename[256] = "backup/";
		strcat_s(bakFilename, 256, filename);
		checksum = calculateChecksum(bakFilename);
		if ( checksum != 0 || fopen_s(&fp, bakFilename, "rbc") != 0 )
		{
			return NULL; // oh darn
		}
	}

	return fp;
}

void safeCloseFile(FILE* fp, char* filename)
{
	fflush(fp);
	fclose(fp);

	// calculate the checksum and append it to the original file
	FILE* oldfile = NULL;
	char checksum = calculateChecksum(filename);
	if ( fopen_s(&oldfile, filename, "a+b") != 0 ) // if an error happens while appending the checksum then the resulting file will be interpreted as corrupt later, bummer
	{
		TRACE("ERROR APPENDING CHECKSUM");
	}
	else
	{
		fprintf_s(oldfile, "%c", -checksum);
		fclose(oldfile);
		makeBackupFile(filename);
	}
}

char calculateChecksum(char* filename) // calculates the checksum for any file and should return 0 on any legit file
{
	FILE* fp = NULL;
	if ( fopen_s(&fp, filename, "rb") != 0 )
	{
		return 0; // returning non-zero here could be disasterous, but "file not found" is obviously caught elsewhere
	}

	// literally sum every byte in the file and return it
	char sum = 0, temp = 0;
	while ( fread_s(&temp, 1, 1, 1, fp) == 1 )
	{
		sum += temp;
	}

	fclose(fp);
	return sum;
}

bool fileExists(char* filename)
{
	FILE* fp = NULL;
	if ( fopen_s(&fp, filename, "rb") != 0 )
	{
		return false;
	}
	fclose(fp);
	return true;
}

void makeBackupFile(char* filename)
{
	// and make a backup
	char bakFilename[256] = "backup/";
	strcat_s(bakFilename, 256, filename);

	FILE* bakFile = NULL, *origFile = NULL;
	if ( fopen_s(&bakFile, bakFilename, "wb") != 0 || fopen_s(&origFile, filename, "rb") != 0 )
	{
		return; // well darn
	}

	// none of these files are ever very large
	char temp = 0;
	while ( fread_s(&temp, 1, sizeof(char), 1, origFile) > 0 )
	{
		fwrite(&temp, sizeof(char), 1, bakFile);
	}

	fflush(bakFile);
	fclose(bakFile);
	fclose(origFile);
}

int checkFileVersion(FILE* fp, char* expected)
{
	char version[4];
	long vnum = 0;

	fread(&version, sizeof(char), 4, fp);
	fread(&vnum, sizeof(long), 1, fp);
	if ( version[0] != expected[0] || version[1] != expected[1] || version[2] != expected[2] || version[3] != expected[3] )
	{
		return -1; // completely wrong file
	}
	return vnum;
}

void renderWhiteLetter(char letter, int x, int y)
{
	// make the letter uppercase
	if ( letter >= 'a' && letter <= 'z' )
	{
		letter = 'A' + letter - 'a';
	}

	// render an uppercase letter
	if ( letter >= 'A' && letter <= 'M' )
	{
		masked_blit(rm.m_whiteFont, rm.m_backbuf, 10*(letter-'A'), 0, x, y, 10, 12);
		return;
	}
	if ( letter >= 'N' && letter <= 'Z' )
	{
		masked_blit(rm.m_whiteFont, rm.m_backbuf, 10*(letter-'N'), 12, x, y, 10, 12);
		return;
	}

	// render a number, punctuation mark, or unknown character
	int row = 5, col = 5;
	if ( letter >= '0' && letter <= '9' )
	{
		row = 2;
		col = letter - '0';
	}
	switch (letter)
	{
	case '?':
		row = 3; col = 2; break;
	case '!':
		row = 3; col = 1; break;
	case '#':
		row = 3; col = 3; break;
	case '$':
		row = 2; col = 11; break;
	case '&':
		row = 3; col = 0; break;
	case '*':
		row = 3; col = 4; break;
	case '-':
		row = 2; col = 10; break;
	case ' ':
		row = 3; col = 5; break;
	case '.':
		row = 2; col = 12; break;
	case '(':
		row = 3; col = 9; break;
	case ')':
		row = 3; col = 10; break;
	case '+':
		row = 3; col = 11; break;
	case ',':
		row = 3; col = 12; break;
	case '~':
		row = 5; col = 3; break;
	case '%':
		row = 3; col = 7; break;
	case '"':
		row = 3; col = 6; break;
	case '\'':
		row = 3; col = 8; break;
	case '/':
		row = 4; col = 0; break;
	case ':':
		row = 4; col = 1; break;
	case ';':
		row = 4; col = 2; break;
	case '<':
		row = 4; col = 3; break;
	case '>':
		row = 4; col = 4; break;
	case '=':
		row = 4; col = 5; break;
	case 16: // shaded arrow
		row = 4; col = 6; break;
	case '[':
		row = 4; col = 7; break;
	case ']':
		row = 4; col = 8; break;
	case 153: // yen
		row = 4; col = 9; break;
	case '^':
		row = 4; col = 10; break;
	case '_':
		row = 4; col = 11; break;
	case '`':
		row = 4; col = 12; break;
	case '{':
		row = 5; col = 0; break;
	case '|':
		row = 5; col = 1; break;
	case '}':
		row = 5; col = 2; break;
	case 151: // a copyright symbol
		row = 5; col = 4; break;
	}
	masked_blit(rm.m_whiteFont, rm.m_backbuf, 10*col, 12*row, x, y, 10, 12);
}

void RenderingManager::flip()
{
	if ( vsyncEnabled )
	{
		vsync();
	}
	// center the X and Y (parameters 4 and 5 become 0 in 640x480 original mode)
	blit(m_backbuf, screen, 0, 0,  (screenWidth-SCREEN_WIDTH)/2 , (screenHeight - SCREEN_HEIGHT) / 2, rm.screenWidth, rm.screenHeight);
	
	if ( currentPage == 1 )
	{
		m_backbuf = m_backbuf2;	
		currentPage = 2;
	}
	else
	{
		m_backbuf = m_backbuf1;
		currentPage = 1;
	}
}

void RenderingManager::screenshot()
{
	// get the current time
	time_t rawtime = 0;
	struct tm timeinfo;
	rawtime = time(NULL);
	localtime_s(&timeinfo, &rawtime);

	// build a filename
	char filename[] = "dd-mm-yy hh-mm-ss.bmp";
	filename[0] = (timeinfo.tm_mday/10)%10 + '0';
	filename[1] = timeinfo.tm_mday%10 + '0';
	filename[3] = (timeinfo.tm_mon/10)%10 + '0';
	filename[4] = timeinfo.tm_mon%10 + '0';
	filename[6] = (timeinfo.tm_year/10)%10 + '0';
	filename[7] = timeinfo.tm_year%10 + '0';
	filename[9] = (timeinfo.tm_hour/10)%10 + '0';
	filename[10] = timeinfo.tm_hour%10 + '0';
	filename[12] = (timeinfo.tm_min/10)%10 + '0';
	filename[13] = timeinfo.tm_min%10 + '0';
	filename[15] = (timeinfo.tm_sec/10)%10 + '0';
	filename[16] = timeinfo.tm_sec%10 + '0';

	save_bitmap(filename, m_backbuf, NULL);
}

void RenderingManager::renderWipeAnim(int frame)
{
	if ( frame < 0 || frame > 14 )
	{
		TRACE("Don't do that.");
		return;
	}

	char filename[] = "DATA/wipe/wipe_0000.tga";
	filename[17] = (frame/10) + '0';
	filename[18] = (frame%10) + '0';

	BITMAP* temp = loadImage(filename);
	masked_stretch_blit(temp, m_backbuf, 0, 0, 320, 240, 0, 0, 640, 480);
	destroy_bitmap(temp);
}

void RenderingManager::dimScreen(int percent)
{
	drawing_mode(DRAW_MODE_TRANS, NULL, 0, 0);
	rectfill(m_backbuf, 0, 0, 640, 480, makeacol(0, 0, 0, getValueFromRange(0, 255, percent)));
	set_alpha_blender(); // the game assumes the graphics are left in this mode
}

void EffectsManager::initialize()
{
	char filename[] = "DATA/sfx/GAME_0000.wav";
	for ( int i = 0; i < TOTAL_NUM_SFX; i++ )
	{
		filename[14] = (i/1000)%10 + '0';
		filename[15] = (i/100)%10 + '0';
		filename[16] = (i/10)%10 + '0';
		filename[17] = (i % 10) + '0';
		fmod_sfx[i] = FSOUND_Sample_Load(FSOUND_FREE, filename, FSOUND_NORMAL | FSOUND_LOOP_OFF, 0, 0);
	}
}

void EffectsManager::playSample(int which, int volume)
{
	if ( which < 41 || which > 156 ) // exclude announcers
	{
		if ( fmod_sfx[which] != NULL )
		{
			int ch = FSOUND_PlaySound(FSOUND_FREE, fmod_sfx[which]);
			if ( volume != 255 )
				FSOUND_SetVolume(ch, volume);
		}
	}
}

void EffectsManager::announcerQuip(int which)
{
	if ( which >= 41 && which < 156 ) // only do announcers
	{
		if ( currentAnnouncerChannel != -1 )
		{
			FSOUND_StopSound(currentAnnouncerChannel);
		}
		if ( fmod_sfx[which] != NULL )
		{
			currentAnnouncerChannel = FSOUND_PlaySound(FSOUND_FREE, fmod_sfx[which]);
		}
	}
}

bool EffectsManager::announcerQuipChance(int which, int percent)
{
	if ( rand()%100 <= percent )
	{
		announcerQuip(which);
		return true;
	}
	return false;
}

void replaceColor(BITMAP* bmp, long col1, long col2)
{
	for ( int y = 0; y < bmp->h; y++ )
	for ( int x = 0; x < bmp->w; x++ )
	{
		if ( ((long *)bmp->line[y])[x] == col1 )
		{
			((long *)bmp->line[y])[x] = col2;
		}
	}
}

void tintGrayscaleBitmap(BITMAP* bmp, int r, int g, int b)
{
	long mask = makecol(255, 0, 255);
	for ( int y = 0; y < bmp->h; y++ )
	for ( int x = 0; x < bmp->w; x++ )
	{
		long c = ((long *)bmp->line[y])[x];
		if ( c == mask ) continue;
		int v = getr32(c); // grayscale: R == G == B
		int nr = r + (255 - r) * v / 255;
		int ng = g + (255 - g) * v / 255;
		int nb = b + (255 - b) * v / 255;
		((long *)bmp->line[y])[x] = makeacol(nr, ng, nb, 255);
	}
}

void tintFillBitmap(BITMAP* bmp, int r, int g, int b)
{
	// Maps white fill pixels to (r,g,b) and leaves black outline pixels black.
	// v=255 (white) → (r,g,b), v=0 (black) → (0,0,0)
	long mask = makecol(255, 0, 255);
	for ( int y = 0; y < bmp->h; y++ )
	for ( int x = 0; x < bmp->w; x++ )
	{
		long c = ((long *)bmp->line[y])[x];
		if ( c == mask ) continue;
		int v = getr32(c);
		((long *)bmp->line[y])[x] = makeacol(r * v / 255, g * v / 255, b * v / 255, 255);
	}
}

void outlineBitmap(BITMAP* bmp)
{
	long mask  = makecol(255, 0, 255);
	long black = makeacol(0, 0, 0, 255);

	BITMAP* src = create_bitmap(bmp->w, bmp->h);
	blit(bmp, src, 0, 0, 0, 0, bmp->w, bmp->h);

	for ( int y = 0; y < bmp->h; y++ )
	for ( int x = 0; x < bmp->w; x++ )
	{
		if ( ((long *)src->line[y])[x] != mask ) continue;
		if ( (x > 0          && ((long *)src->line[y])[x-1]  != mask) ||
		     (x < bmp->w - 1 && ((long *)src->line[y])[x+1]  != mask) ||
		     (y > 0          && ((long *)src->line[y-1])[x]   != mask) ||
		     (y < bmp->h - 1 && ((long *)src->line[y+1])[x]  != mask) )
		{
			((long *)bmp->line[y])[x] = black;
		}
	}
	destroy_bitmap(src);
}

void renderWhiteString(const char* string, int x, int y)
{
	int i = 0;
	while ( string[i] != 0 )
	{
		renderWhiteLetter(string[i], x, y);
		x += 10;
		i++;
	}
}

static void renderColoredLetterFromFont(BITMAP* font, char letter, int x, int y)
{
	if ( letter >= 'a' && letter <= 'z' )
		letter = 'A' + letter - 'a';

	if ( letter >= 'A' && letter <= 'M' )
	{
		masked_blit(font, rm.m_backbuf, 10*(letter-'A'), 0, x, y, 10, 12);
		return;
	}
	if ( letter >= 'N' && letter <= 'Z' )
	{
		masked_blit(font, rm.m_backbuf, 10*(letter-'N'), 12, x, y, 10, 12);
		return;
	}

	int row = 5, col = 5;
	if ( letter >= '0' && letter <= '9' ) { row = 2; col = letter - '0'; }
	switch (letter)
	{
	case '?': row = 3; col = 2; break;
	case '!': row = 3; col = 1; break;
	case '#': row = 3; col = 3; break;
	case '$': row = 2; col = 11; break;
	case '&': row = 3; col = 0; break;
	case '*': row = 3; col = 4; break;
	case '-': row = 2; col = 10; break;
	case ' ': row = 3; col = 5; break;
	case '.': row = 2; col = 12; break;
	case '(': row = 3; col = 9; break;
	case ')': row = 3; col = 10; break;
	case '+': row = 3; col = 11; break;
	case ',': row = 3; col = 12; break;
	case '~': row = 5; col = 3; break;
	case '%': row = 3; col = 7; break;
	case '"': row = 3; col = 6; break;
	case '\'': row = 3; col = 8; break;
	case '/': row = 4; col = 0; break;
	case ':': row = 4; col = 1; break;
	case ';': row = 4; col = 2; break;
	case '<': row = 4; col = 3; break;
	case '>': row = 4; col = 4; break;
	case '=': row = 4; col = 5; break;
	case 16:  row = 4; col = 6; break;
	case '[': row = 4; col = 7; break;
	case ']': row = 4; col = 8; break;
	case 153: row = 4; col = 9; break;
	case '^': row = 4; col = 10; break;
	case '_': row = 4; col = 11; break;
	case '`': row = 4; col = 12; break;
	case '{': row = 5; col = 0; break;
	case '|': row = 5; col = 1; break;
	case '}': row = 5; col = 2; break;
	case 151: row = 5; col = 4; break;
	}
	masked_blit(font, rm.m_backbuf, 10*col, 12*row, x, y, 10, 12);
}

void renderColoredLetter(char letter, int x, int y, int color)
{
	renderColoredLetterFromFont(rm.m_colorFontNoOutline[color], letter, x, y);
}

void renderColoredString(const char* string, int x, int y, int color)
{
	int i = 0;
	while ( string[i] != 0 )
	{
		renderColoredLetter(string[i], x, y, color);
		x += 10;
		i++;
	}
}

void renderOutlinedColoredLetter(char letter, int x, int y, int color)
{
	renderColoredLetterFromFont(rm.m_colorFont[color], letter, x, y);
}

void renderOutlinedColoredString(const char* string, int x, int y, int color)
{
	int i = 0;
	while ( string[i] != 0 )
	{
		renderOutlinedColoredLetter(string[i], x, y, color);
		x += 10;
		i++;
	}
}

void renderWhiteNumber(int number, int x, int y)
{
	char str[10] = "";
	sprintf_s(str, 10, "%d", number);
	renderWhiteString(str, x, y);
}

void renderTextString(const char* string, int x, int y, int width, int height, int color)
{
	static char puncs[27] = "!@#$%^&*()-=+[]:;'\"`~,.?/\\";
	int left = x, top = y;

	// loop through each character in the string, find it, render it, advance
	for ( int i = 0; string[i] != 0; i++ )
	{
		int row = 2, col = 11; // default to a space

		// find the glyph in the bitmap font
		if ( string[i] >= 'A' && string[i] <= 'Z' )
		{
			row = 0;
			col = string[i] - 'A';
		}
		else if ( string[i] >= 'a' && string[i] <= 'z' )
		{
			row = 1;
			col = string[i] - 'a';
		}
		else if ( string[i] >= '0' && string[i] <= '9' )
		{
			row = 2;
			col = string[i] - '0';
		}
		else
		{
			for ( int j = 0; j < 26; j++ )
			{
				if ( string[i] == puncs[j] )
				{
					row = 3;
					col = j;
					break;
				}
			}
		}

		// render it!
		masked_blit(rm.m_textFont[color], rm.m_backbuf, 10*col, 19*row, x, y, 10, 18);

		// if row is full then next row. out of rows? then end early.
		x += 10;
		if ( x-left > width )
		{
			x = left;
			y += 15;
		}
		if ( y-top > height )
		{
			break;
		}
	}
}

void debugRenderTextFontColors(int x, int y)
{
	const char* sample = "The Quick Brown Fox 0123456789 !@#$";
	renderTextString(sample, x, y,      640, 20, 0);
	renderTextString(sample, x, y + 20, 640, 20, 1);
	renderTextString(sample, x, y + 40, 640, 20, 2);
	renderTextString(sample, x, y + 60, 640, 20, 3);
}

static char boldTextWidths[] =
{
	7,  7,  10, 17, 14, 17, 15, 7,  7, 7,
	15, 14, 7,  14, 7,  10, 14, 14, 14, 14,
	14, 14, 14, 14, 14, 14, 7,  7,  13, 13,
	13, 17, 24, 14, 14, 14, 14, 10, 10, 14, // >?@ABCDEFG
	14, 9,  9,  14, 10, 20, 15, 14, 14, 14, // Q
	14, 14, 14, 14, 14, 22, 14, 13, 12, 7,  // Z[
	10, 7,  13, 14, 9,  14, 14, 14, 14, 14,
	10, 14, 14, 7,  8,  13, 8,  21, 14, 14,
	14, 14, 11, 14, 9,  14, 12, 19, 13, 13,
	10, 10, 10, 13, 13, 24, 24, 24, 24, 24,
};

void renderBoldString(const unsigned char* string, int x, int y, int maxWidth, bool fixedWidth, int color)
{
	int left = x;
	bool smallermode = false;

	// loop through each character in the string, find it, render it, advance
	for ( int i = 0; string[i] != 0; i++ )
	{
		int row = 0, col = 0; // default to a space
		int yoff = 0; // for lowercase letters and some punctuation

		// find the glyph on the sprite sheet
		unsigned char temp = string[i];
		if ( temp == 199 ) // alt-128 �
		{
			temp = 128; // heart
		}
		if ( temp >= 32 && temp <= 128 )
		{
			int ascii = temp - 32;
			row = (ascii/10) % 10;
			col = ascii % 10;
		}

		// some characters such as 'g' 'y' 'p' and 'q' hang below the line
		if ( string[i] == 'g' || string[i] == 'q' || string[i] == 'j' || string[i] == 'Q' )
		{
			yoff = +3;
		}
		if ( string[i] == 'y' || string[i] == 'p' )
		{
			yoff = +2;
		}

		if ( string[i] == '|' ) // a convention I saw used in the original DMX to note a subtitle
		{
			smallermode = !smallermode;
			continue;
		}

		// render it!
		if ( smallermode )
		{
			if ( yoff > 0 )
			{
				yoff -= 1;
			}
			masked_stretch_blit(rm.m_boldFont[color], rm.m_backbuf, 24*row, 24*col, 24, 24, x, y+yoff+6, 18, 18);
		}
		else
		{
			masked_blit(rm.m_boldFont[color], rm.m_backbuf, 24*row, 24*col, x, y+yoff, 24, 24);
		}

		// find the width of this glyph
		int width = 24;
		if ( !fixedWidth )
		{
			width = boldTextWidths[row*10 + col];
		}
		if ( smallermode )
		{
			width = (width * 3/4) + 1;
		}

		// if row is full then end early.
		x += width;
		if ( x-left >= maxWidth )
		{
			return;
		}
	}
}

void renderBoldString(const char* string, int x, int y, int maxWidth, bool fixedWidth, int color)
{
	renderBoldString((unsigned char*)string, x, y, maxWidth, fixedWidth, color);
}

int getBoldStringWidth(const char* string)
{
	int  width      = 0;
	bool smallermode = false;
	for ( int i = 0; string[i] != 0; i++ )
	{
		if ( string[i] == '|' ) { smallermode = !smallermode; continue; }
		unsigned char temp = (unsigned char)string[i];
		if ( temp == 199 ) temp = 128;
		if ( temp >= 32 && temp <= 128 )
		{
			int ascii = temp - 32;
			int row   = (ascii / 10) % 10;
			int col   = ascii % 10;
			int w     = boldTextWidths[row * 10 + col];
			if ( smallermode ) w = (w * 3 / 4) + 1;
			width += w;
		}
	}
	return width;
}

static char artistTextWidths[] =
{
	 6,  8, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
	 6, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
	10, 10,  6,  6, 11, 11, 11, 11, 11, 12, 11, 11, // ends with A B C
	11, 11, 11, 11, 11, 11, 11, 12, 11, 13, 12, 11, // D - O
	10, 11, 12, 10, 11, 12, 12, 14, 11, 11, 10,  6, // P - Z, [
	10,  6, 10, 10,  7, 11, 12, 11, 12, 11, 11, 11, // ends wuth a b c d e f g
	12,  9,  9, 10,  9, 13, 11, 10, 11, 11, 10,  9,
	10, 11, 11, 11, 10, 11,  9,  6,  6,  6, 10, 10, // begins with t - z
};

// 'artist' is the name of the typewriter font
void renderArtistString(const unsigned char* string, int x, int y, int width, int height)
{
	int left = x, top = y;

	// loop through each character in the string, find it, render it, advance
	for ( int i = 0; string[i] != 0; i++ )
	{
		int row = 0, col = 0; // default to a space
		int yoff = 0;

		// find the glyph in the bitmap font
		if ( string[i] >= 32 || string[i] <= 126 )
		{
			row = (string[i] - 32) % 12;
			col = (string[i] - 32) / 12;
		}

		// some characters such as 'g' 'y' 'p' and 'q' hang below the line
		if ( string[i] == 'g' || string[i] == 'q' || string[i] == 'j' || string[i] == 'Q' )
		{
			yoff = +3;
		}
		if ( string[i] == 'y' || string[i] == 'p' || string[i] == '(' || string[i] == ')' )
		{
			yoff = +3;
		}
		if ( string[i] == 's' )
		{
			//yoff = +1;
		}
		// render it!
		masked_blit(rm.m_artistFont, rm.m_backbuf, 16*col, 16*row, x, y+yoff, 16, 16);

		// if row is full then next row. out of rows? then end early.
		x += artistTextWidths[col*12 + row] + 1;
		if ( x-left > width )
		{
			x = left;
			y += 16;
		}
		if ( y-top > height )
		{
			break;
		}
	}
}

void renderArtistString(const char* string, int x, int y, int width, int height)
{
	renderArtistString((unsigned char*)string, x, y, width, height);
}

int getArtistStringWidth(const char* string)
{
	int width = 0;
	for ( int i = 0; string[i] != 0; i++ )
	{
		if ( string[i] >= 32 && string[i] <= 126 )
		{
			int row = (string[i] - 32) % 12;
			int col = (string[i] - 32) / 12;
			width += artistTextWidths[col * 12 + row] + 1;
		}
	}
	return width > 0 ? width - 1 : 0;
}

static char scoreTextWidths[] =
{
	 8,  8, 11, 11, 14, 16, 16, 10, 10, 10, 10, 10,
	 6, 10, 10, 10, 12, 12, 12, 12, 12, 12, 12, 12,
	12, 12,  7,  8,  9, 10,  9, 11, 15, 16, 15, 16, // ends with A B C
	16, 13, 13, 16, 16,  9, 15, 15, 14, 16, 16, 16, // D - O
	15, 16, 15, 15, 15, 16, 16, 16, 16, 16, 14,  8, // P - Z, [
	14,  8,  9, 14,  8, 11, 12, 11, 12, 11, 11, 11, // ends wuth a b c d e f g
	12,  9,  9, 10,  9, 13, 11, 10, 11, 11, 10,  9,
	10, 11, 11, 11, 10, 11,  9,  6,  6,  6, 10, 10, // begins with t - z
};

// 'score' is the name of the font with the grey/white gradient
void renderScoreString(const char* string, int x, int y, int width, int height)
{
	int left = x, top = y;

	// loop through each character in the string, find it, render it, advance
	for ( int i = 0; string[i] != 0; i++ )
	{
		int row = 0, col = 0; // default to a space
		int yoff = 0;

		// find the glyph in the bitmap font
		if ( string[i] >= 32 || string[i] <= 126 )
		{
			row = (string[i] - 32) % 12;
			col = (string[i] - 32) / 12;
		}

		/* some characters such as 'g' 'y' 'p' and 'q' hang below the line
		// untested for this font, don't use lower case letters
		if ( string[i] == 'g' || string[i] == 'q' || string[i] == 'j' || string[i] == 'Q' )
		{
			yoff = +3;
		}
		if ( string[i] == 'y' || string[i] == 'p' || string[i] == '(' || string[i] == ')' )
		{
			yoff = +3;
		}
		if ( string[i] == 's' )
		{
			//yoff = +1;
		}
		//*/

		// render it!
		masked_blit(rm.m_scoreFont, rm.m_backbuf, 32*col, 32*row, x, y+yoff, 32, 32);

		// if row is full then next row. out of rows? then end early.
		x += (scoreTextWidths[col*12 + row] + 1) * 2; // times two because this font is twice as large
		if ( x-left > width )
		{
			x = left;
			y += 32;
		}
		if ( y-top > height )
		{
			break;
		}
	}
}

void renderScoreNumber(int number, int x, int y, int requiredDigits)
{
	char str[16] = "";
	sprintf_s(str, 10, "%.*d", requiredDigits, number);
	renderScoreString(str, x, y, 256, 32);
}

void renderNameString(const char* string, int x, int y, int color)
{
	for ( int i = 0; string[i] != 0; i++ )
	{
		renderNameLetter(string[i], x, y, color);
		x += 32;
	}
}

void renderNameLetter(char letter, int x, int y, int color)
{
	int row = 5, col = 10, pos = 65; // default to a space
	static char puncs[40] = "[]()<>0123456789+-*/=@:;`',.?!\"#%&^_ |~";

	if ( letter >= 'A' && letter <= 'Z' )
	{
		pos = letter - 'A';
	}
	else if ( letter >= 'a' && letter <= 'z' )
	{
		pos = letter - 'a';
	}
	else
	{
		for ( int i = 0; i < 39; i++ )
		{
			if ( letter == puncs[i] )
			{
				pos = i + 26;
				break;
			}
		}
	}
	col = pos/6;
	row = pos%6;

	masked_blit(rm.m_nameFont[color], rm.m_backbuf, 32*col, 32*row, x, y, 32, 32);
}

int songID_to_listID(int songID)
{
	for ( int i = 0; i < NUM_SONGS; i++ )
	{
		if ( songIDs[i] == songID )
		{
			return i;
		}
	}

	return -1; // up to the caller to throw and invalid song id error if necessary
}

int listID_to_songID(int index)
{
	if ( index >= NUM_SONGS || index < 0)
	{
		return -1; // programmer error
	}
	return songIDs[index];
}

int getChartIndexFromType(int type)
{
	switch(type)
	{
	case SINGLE_MILD: return 0;
	case SINGLE_WILD: return 1;
	case SINGLE_ANOTHER: return 2;
	case DOUBLE_MILD: return 3;
	case DOUBLE_WILD: return 4;
	case DOUBLE_ANOTHER: return 5;
	}
	return -1;
}

void playSFXOnce(FSOUND_SAMPLE* sample)
{
	if ( sample != NULL )
	{
		FSOUND_PlaySound(FSOUND_FREE, sample);
	}
}

int calculateArrowColor(unsigned long timing, int timePerBeat)
{
	int t = timing % timePerBeat;
	int fraction16 = t * 16 / timePerBeat;
	return fraction16 / 4;
}