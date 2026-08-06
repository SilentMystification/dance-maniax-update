// updateChecker.h implements a class for bounded, timeout-safe checks for new exe/data updates
// unlike downloadManager.h (which uses URLDownloadToFile and has no timeout knob), this class
// uses WinHTTP specifically so that an automatic startup-time check can never hang an unattended
// cabinet's boot when the machine has no internet connection.

#ifndef _UPDATECHECKER_H
#define _UPDATECHECKER_H

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "wininet.lib")

#include "../headers/common.h"
#include "../headers/downloadManager.h" // for UPDATE_CONFIG_FILENAME
#include "../headers/releaseTag.h"

// release tags are shaped "DMX-XX-YYYYMMDDVV" - XX is one of these channel codes
#define DMX_CHANNEL_STABLE 0
#define DMX_CHANNEL_BETA 1
#define DMX_CHANNEL_ALPHA 2

class UpdateChecker
{
public:

	UpdateChecker();
	// postcondition: githubRepo is populated from line 2 of UPDATE_CONFIG_FILENAME, or empty if unreadable

	static bool hasNetworkConnection();
	// precondition: none
	// postcondition: returns instantly (a local system query, no network round-trip) - true if this
	//                machine has any active network connection at all. Meant as a cheap first gate
	//                before isHostReachable()/checkForExeUpdate() so a cabinet with no network/no IP
	//                skips immediately instead of waiting out a connect timeout for nothing.

	bool isHostReachable(const std::string& url, int timeoutMs);
	// precondition: none
	// postcondition: returns true only if a TCP+TLS connection could be made and an HTTP response
	//                was received within timeoutMs; returns false (never hangs, never throws) otherwise

	bool checkForExeUpdate(int channel, std::string& outTag, std::string& outAssetUrl, int timeoutMs);
	// precondition: githubRepo is non-empty; channel is one of the DMX_CHANNEL_* constants
	// postcondition: returns true only if a release tagged for the requested channel was found AND its
	//                date+sequence is strictly newer than getCurrentExeTag() AND both the tag and
	//                the DMX.exe asset URL were parsed successfully. Only ever considers tags matching
	//                the requested channel - a channel switch is allowed (an operator picking a
	//                different channel naturally starts comparing against that channel's releases),
	//                but a downgrade never is, regardless of channel.
	//                returns false safely on any network failure, timeout, or parse failure.

	std::string getGithubRepo() { return githubRepo; }

protected:
	std::string githubRepo; // "owner/repo"

	bool httpGetBounded(const std::string& host, const std::string& path, bool isHttps, int timeoutMs, std::string& outBody);
	// low-level WinHTTP GET, used by both isHostReachable (body discarded) and checkForExeUpdate.
	// bounds the response body to a fixed maximum (64KB) to avoid unbounded memory growth.
	// every WinHTTP*() return value is checked; all handles are closed on every exit path.

	bool parseUrl(const std::string& url, bool& outIsHttps, std::string& outHost, std::string& outPath);

	std::string extractJsonStringField(const std::string& json, const std::string& key, size_t searchFromOffset = 0);
	// naive substring scan for "\"key\":\"value\"" - see design note in updateChecker.cpp

	std::string extractDmxExeDownloadUrl(const std::string& json);
	// finds "\"name\":\"DMX.exe\"" then scans forward (bounded window) for the sibling
	// "\"browser_download_url\":\"...\""

	std::string findBestTagForChannel(const std::string& releasesJsonArray, int channel, std::string& outAssetUrl);
	// scans a GitHub "list releases" JSON array response for every release whose tag matches the
	// requested channel's "DMX-XX-" prefix, and returns the one with the greatest date+sequence
	// (i.e. the newest release actually published on that channel). Returns "" if none matched.
};

std::string getCurrentExeTag();
// postcondition: returns DMX_RELEASE_TAG - the tag this exe was CI-built as, burned in at compile
// time (see releaseTag.h) - or "" for a local/dev build that was never an official release

#endif
