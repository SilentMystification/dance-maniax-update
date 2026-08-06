// videoManager.cpp implements a class which implements the background videos
// source file created by Allen Seitz 6/27/2012

#include "../headers/videoManager.h"

#include <apeg.h>

extern UTIME getTimeMs();

static void resolveStepFilename(const struct MOVIE_SEQ_STEP* script, int step, char* out, size_t outSize)
{
	strcpy_s(out, outSize, "DATA/video/");
	const char* name = (script[step].filename[0] == '*') ? script[step-1].filename : script[step].filename;
	strcat_s(out, outSize, name);
	strcat_s(out, outSize, ".ogg");
}

static APEG_STREAM* openVideoStream(const char* filename, void** bufOut)
{
	*bufOut = NULL;
	UTIME t0 = getTimeMs();

	FILE* fp = NULL;
	if ( fopen_s(&fp, filename, "rb") != 0 )
	{
		al_trace("Movie %s is missing.\r\n", filename);
		return NULL;
	}
	fseek(fp, 0, SEEK_END);
	long fsize = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	void* buf = malloc(fsize);
	fread(buf, fsize, 1, fp);
	fclose(fp);

	UTIME t1 = getTimeMs();
	al_trace("VideoManager: read %s (%ld bytes) in %lu ms on the main thread.\r\n", filename, fsize, t1 - t0);
	if ( t1 - t0 > 15 )
	{
		al_trace("VideoManager: *** SLOW FILE READ *** %s took %lu ms - check AV/Defender scanning or disk contention on this file.\r\n", filename, t1 - t0);
	}

	if ( ((char*)buf)[0] != 'O' || ((char*)buf)[1] != 'g' || ((char*)buf)[2] != 'g' || ((char*)buf)[3] != 'S' )
	{
		al_trace("Video stream did not seem to contain Ogg data.\r\n");
		free(buf);
		return NULL;
	}
	APEG_STREAM* stream = apeg_open_memory_stream(buf, fsize);
	if ( stream == NULL )
	{
		free(buf);
		return NULL;
	}
	*bufOut = buf;
	return stream;
}

static void closeVideoStream(APEG_STREAM*& stream, void*& buf)
{
	if ( stream != NULL )
	{
		apeg_reset_stream(stream);
		apeg_close_stream(stream);
		stream = NULL;
	}
	if ( buf != NULL )
	{
		free(buf);
		buf = NULL;
	}
}

static void advanceToFirstFrame(APEG_STREAM* stream)
{
	UTIME t0 = getTimeMs();
	for ( int i = 0; i < 8; i++ )
	{
		apeg_advance_stream(stream, true);
		if ( stream->frame_updated > 0 && stream->bitmap != NULL )
			break;
	}
	UTIME elapsed = getTimeMs() - t0;
	if ( elapsed > 15 )
	{
		al_trace("VideoManager: *** SLOW DECODE *** advanceToFirstFrame took %lu ms.\r\n", elapsed);
	}
}

void VideoManager::stop()
{
	closeVideoStream(nextCmov, nextVideoBuffer);
	nextPreloadedStep = -1;
	isStopped = true;
}

void VideoManager::reset()
{
	stop();
	currentTime = currentStep = decodeAccum = 0;
	frameMs = 33;
}

VideoManager::VideoManager()
{
	haxNoVideos = fileExists("novideo");
}

void VideoManager::initialize()
{
	reset();
	apeg_ignore_audio(true);

	frameData = create_bitmap_ex(32, 320, 192);
	clear_to_color(frameData, 0);
	cmov = NULL; videoBuffer = NULL;
}

void VideoManager::update(UTIME dt)
{
	if ( isStopped || haxNoVideos )
	{
		return;
	}
	currentTime += dt;

	// check for a script step advance
	if ( (script[currentStep+1].timing != -1) && currentTime/10 >= script[currentStep+1].timing/3 ) // because there are 300 ticks per second
	{
		currentStep++;
		loadVideoAtCurrentStep();
	}
	// check for the movie ending and loop the whole thing after 5 seconds
	if ( script[currentStep+1].timing == -1 && currentStep > 1 && (currentTime/10 + 1500 >= script[currentStep].timing/3) )
	{
		currentStep = 1;
		currentTime = 0;
		loadVideoAtCurrentStep();
	}

	if ( cmov != NULL )
	{
		decodeAccum += dt;
		if ( decodeAccum >= frameMs )
		{
			decodeAccum -= frameMs;
			if ( apeg_advance_stream(cmov, true) != APEG_OK)
			{
				al_trace("Video problem! Breakpoint!\r\n"); // doesn't really matter if it fails
			}
			if( cmov->frame_updated > 0 && cmov->bitmap != NULL )
			{
				//stretch_blit(cmov->bitmap, frameData, 0, 0, cmov->w, cmov->h, 0, 0, 320, 192);
				blit(cmov->bitmap, frameData, 0, 0, 0, 0, 320, 192);
			}
		}
	}
}

void VideoManager::renderToSurface(BITMAP* surface, int x, int y)
{
	//renderWhiteNumber(currentTime, 100, 84);
	if ( frameData == NULL )
	{
		return;
	}
	blit(frameData, surface, 0, 0, x, y, 320, 192);

	//renderWhiteNumber(script[currentStep].beta, 100, 52);
	//renderWhiteNumber(numFrames, 120, 52);
	//renderWhiteNumber(script[currentStep].delta, 200, 52);
	//renderWhiteNumber(script[currentStep].timing/300, 100, 64);
	//renderWhiteString(script[currentStep].filename, 200, 64);
	//renderWhiteNumber(currentTime, 100, 84);
}

void VideoManager::renderToSurfaceStretched(BITMAP* surface, int x, int y, int width, int height)
{
	if ( frameData == NULL )
	{
		return;
	}
	stretch_blit(frameData, surface, 0, 0, 320, 192, x, y, width, height);
}

void VideoManager::loadScript(const char* filename)
{
	reset();
	FILE* fp = NULL;

	if ( fopen_s(&fp, filename, "rb") != 0 )
	{
		globalError(MISSING_VIDEO_SCRIPT, filename);
		return;
	}

	// load script steps until the file is exhausted
	while ( fread(&script[currentStep], sizeof(struct MOVIE_SEQ_STEP), 1, fp) > 0 )
	{
		currentStep++;
	}
	script[currentStep].clear(); // so that I'll know later when the script has ended

	// in the original game step 1 was always one of the four J_ movies
	// these were stored on the onboard flash for speed
	// they would play for the first 1-5 seconds while the CD-ROM streamed the mp3
	// the play length was listed as 0, since the videos on disc would resume as soon as the CD-ROM was ready
	// I could omit the J_ step entirely, since it is no longer needed to mask load times anywmore
	// but those videos are part of the game, so I show them for 1500ms, then switch to the real script
	if ( currentStep > 1 && script[2].timing == 0 )
	{
		script[1].timing = 0; // should be overwriting 1500
		script[2].timing = 1500; // should be overwriting 0
	}

	currentStep = 1;
	isStopped = false;

	fclose(fp);
	loadVideoAtCurrentStep();
}

void VideoManager::loadVideoAtCurrentStep()
{
	UTIME t0 = getTimeMs();
	bool usedPreload = ( nextCmov != NULL && nextPreloadedStep == currentStep );

	if ( usedPreload )
	{
		// swap preloaded stream in — no file I/O, first frame already decoded
		closeVideoStream(cmov, videoBuffer);
		cmov = nextCmov;         videoBuffer = nextVideoBuffer;
		nextCmov = NULL;         nextVideoBuffer = NULL;   nextPreloadedStep = -1;
	}
	else
	{
		char filename[256];
		resolveStepFilename(script, currentStep, filename, sizeof(filename));
		closeVideoStream(cmov, videoBuffer);
		cmov = openVideoStream(filename, &videoBuffer);

		if ( cmov == NULL )
		{
			clear_to_color(frameData, makecol(255, 255, 255));
			textprintf_centre(frameData, font, 160, 90, makecol(0, 0, 0), "%s", script[currentStep].filename);
			return;
		}
		advanceToFirstFrame(cmov);
	}

	int displayRate = get_refresh_rate();
	int displayMs  = (displayRate > 0) ? (1000 / displayRate) : 33;
	frameMs = (cmov->frame_rate > 0.0) ? (int)(1000.0 / cmov->frame_rate) : displayMs;

	if ( cmov->frame_updated > 0 && cmov->bitmap != NULL )
		blit(cmov->bitmap, frameData, 0, 0, 0, 0, 320, 192);

	UTIME swapMs = getTimeMs() - t0;
	al_trace("VideoManager: swap-in for step %d at song time %d ms took %lu ms (%s).\r\n",
		currentStep, currentTime, swapMs, usedPreload ? "used preload, should be instant" : "BLOCKING LOAD, no preload was ready");

	UTIME t1 = getTimeMs();
	preloadNextStep();
	UTIME preloadMs = getTimeMs() - t1;
	al_trace("VideoManager: preloadNextStep() for the step after %d took %lu ms on the main thread.\r\n", currentStep, preloadMs);
	if ( swapMs + preloadMs > 15 )
	{
		al_trace("VideoManager: *** this update() call blocked the main game thread for %lu ms total ***\r\n", swapMs + preloadMs);
	}
}

void VideoManager::preloadNextStep()
{
	closeVideoStream(nextCmov, nextVideoBuffer);
	nextPreloadedStep = -1;

	// preload the next sequential step, or step 1 if the script is about to loop
	int step = (script[currentStep + 1].timing == -1) ? 1 : currentStep + 1;
	if ( step >= 100 || script[step].timing == -1 )
		return;

	char filename[256];
	resolveStepFilename(script, step, filename, sizeof(filename));
	nextCmov = openVideoStream(filename, &nextVideoBuffer);
	if ( nextCmov == NULL )
		return;

	advanceToFirstFrame(nextCmov);
	nextPreloadedStep = step;
}