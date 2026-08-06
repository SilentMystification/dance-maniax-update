// downloadManager.h implements a class to find and download software updates
// source file created by Allen Seitz 11/02/2015
// downloads themselves use WinHTTP (not the old urlmon/URLDownloadToFile) so they can request
// modern TLS explicitly and so a stalled connection can be detected and aborted rather than
// hanging forever - see downloadManager.cpp for details.

#ifndef _DOWNLOADMANAGER_H
#define _DOWNLOADMANAGER_H

#pragma comment(lib, "winhttp.lib")

#include "../headers/common.h"

#define MANIFEST_FILENAME "manifest.txt"
#define MANIFEST_BETA_FILENAME "manifest_beta.txt"
#define UPDATE_CONFIG_FILENAME "update_config.txt"

// downloadFile() blocks the calling thread until the transfer finishes, fails, or is cancelled,
// pumping the screen periodically so the UI doesn't freeze during a long transfer (mirrors the
// pattern the old urlmon progress callback used)
extern void alternateMainUpdateLoop();
extern RenderingManager rm;

// true only while the automatic startup update path owns an in-progress check/apply - when set,
// failures here must degrade silently instead of calling globalError() (see main.cpp for why)
extern bool automaticUpdateActive;

class DownloadManager
{
public:

	DownloadManager();

	void resetState();
	// precondition: do not call this if a download is in progress! it can leak file handles
	// postcondition: ready to call downloadFile() again

	std::string doWeNeedAnyUpdates();
	// precondition: manifest.txt has been downloaded
	// postcondition: returns a file to download if manifest.txt lists a file which we don't have, or an empty string otherwise

	void downloadFile(std::string url, std::string filename, bool urlIsAbsolute = false);
	// precondition: works best if no other downloads are in progress
	// if urlIsAbsolute is true, "url" is used as-is (not prefixed with serverUrl) - used for
	// fully-qualified URLs such as a GitHub release asset's browser_download_url
	// postcondition: blocks until the download completes, fails, or is cancelled. There is
	// deliberately NO cap on total transfer time - a slow-but-working download is allowed to take
	// as long as it needs. Each individual read IS bounded (see the stall timeout in the .cpp), so
	// a connection that goes completely silent is detected and aborted rather than hanging forever.
	// check isDownloadComplete() / didDownloadFail() afterward to see which happened.

	std::string getServerUrl() { return serverUrl; }
	// postcondition: returns the data-server base URL read from UPDATE_CONFIG_FILENAME, or "" if missing

	std::string getCurrentDownloadFilename();
	// precondition: there is a download currently in progress because of downloadFile()
	// postcondition: returns the currentFilename, or an empty string

	int getCurrentDownloadProgress();
	// postcondition: returns [0-100]. Always 0 if the server never reported a Content-Length -
	// use isDownloadComplete() to know when it's actually done, not a 100% reading.

	void cancelCurrentDownload();
	// precondition: there is a download currently in progress because of downloadFile()
	// postcondition: aborts the in-progress transfer at the next opportunity and deletes the
	// partial file

	bool isDownloadComplete();
	// postcondition: returns true only once the most recently started download finished successfully

	bool didDownloadFail();
	// postcondition: returns true if the most recently started download ended in a definite
	// failure (network/protocol error, a stalled connection, or cancellation) rather than
	// completing successfully. Callers MUST check this - isDownloadComplete() never becomes true
	// on failure, so code that only polls isDownloadComplete() would otherwise wait forever.

protected:
	std::string serverUrl;

	bool isDownloadInProgress;
	bool userCancelledDownload;
	bool downloadSucceeded;
	bool downloadFailed;
	std::string currentUrl;
	std::string currentFilename;
	unsigned __int64 currentBytesDownloaded;
	unsigned __int64 currentTotalBytes; // 0 if the server never reported a Content-Length

	bool performDownload(const std::string& url, const std::string& filename);
	// low-level WinHTTP GET-to-file with progress pumping and stall detection; returns true only
	// on a fully successful transfer. Deletes a partial file on any failure or cancellation.
};

#endif
