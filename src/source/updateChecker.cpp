// updateChecker.cpp implements bounded, timeout-safe update checks (see updateChecker.h)

#include "../headers/updateChecker.h"
#include <winhttp.h>
#include <wininet.h>
#include <vector>
#include <cctype>

namespace
{
	std::wstring toWide(const std::string& s)
	{
		if (s.empty()) return std::wstring();
		int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.length(), NULL, 0);
		std::wstring result(len, 0);
		MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.length(), &result[0], len);
		return result;
	}
}

namespace
{
	// accepts either a bare "owner/repo" slug or a full "https://github.com/owner/repo" URL
	// (with or without a trailing slash), and normalizes down to "owner/repo" either way
	std::string normalizeGithubRepo(std::string value)
	{
		const char* prefixes[] = { "https://github.com/", "http://github.com/" };
		for (size_t i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); i++)
		{
			size_t prefixLen = strlen(prefixes[i]);
			if (value.compare(0, prefixLen, prefixes[i]) == 0)
			{
				value = value.substr(prefixLen);
				break;
			}
		}

		while (!value.empty() && value.back() == '/')
		{
			value.pop_back();
		}

		return value;
	}
}

namespace
{
	// This project's release tags are always shaped "DMX-XX-YYYYMMDDVV" (see
	// .github/workflows/release.yml): XX is the channel (00 stable, 01 beta, 02 alpha), YYYYMMDD is
	// the build date (the same date __DATE__ embeds in the compiled binary, since CI always builds
	// same-day), and VV is a per-channel daily sequence number so a channel can release more than
	// once in a day. Comparison is purely by the 10-digit YYYYMMDDVV string - fixed-width, digits
	// only, so ordinary string comparison already gives correct numeric/chronological ordering with
	// no separate stable-vs-prerelease precedence rule needed (unlike semver).
	bool parseDmxTag(const std::string& tag, int& outChannel, std::string& outDateSeq)
	{
		if (tag.size() != 17) return false;
		if (tag.compare(0, 4, "DMX-") != 0) return false;
		if (tag[6] != '-') return false;

		std::string channelStr = tag.substr(4, 2);
		std::string dateSeq = tag.substr(7, 10);

		for (char c : channelStr) { if (!isdigit((unsigned char)c)) return false; }
		for (char c : dateSeq) { if (!isdigit((unsigned char)c)) return false; }

		outChannel = atoi(channelStr.c_str());
		outDateSeq = dateSeq;
		return true;
	}

	int monthNumberFromName(const char* name)
	{
		static const char* monthNames[12] = { "Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec" };
		for (int i = 0; i < 12; i++)
		{
			if (_stricmp(name, monthNames[i]) == 0) return i + 1;
		}
		return 0;
	}

	// A local/dev build has no CI-stamped DMX_RELEASE_TAG (and no per-day sequence number of its own
	// to slot into a channel's ordering), so day-level granularity isn't enough to place it correctly
	// during a heavy-churn day with multiple alpha/beta releases. This returns this exe's own compile
	// moment as a UTC-referenced epoch value, derived from __DATE__/__TIME__ (whatever the build
	// machine's local clock read) via mktime() - which resolves that wall-clock reading using the
	// RUNNING machine's timezone setting. That's only a true universal instant to the extent the
	// cabinet running this check shares the build machine's timezone, which holds for the common case
	// (the same operator building and deploying to their own cabinet). Returns 0 if unparseable.
	time_t getLocalCompileEpoch()
	{
		char monthStr[8] = "";
		int day = 0, year = 0, hour = 0, minute = 0, second = 0;
		if (sscanf_s(__DATE__, "%7s %d %d", monthStr, (unsigned)sizeof(monthStr), &day, &year) != 3)
		{
			return 0;
		}
		if (sscanf_s(__TIME__, "%d:%d:%d", &hour, &minute, &second) != 3)
		{
			return 0;
		}

		int month = monthNumberFromName(monthStr);
		if (month == 0) return 0;

		struct tm t = { 0 };
		t.tm_year = year - 1900;
		t.tm_mon = month - 1;
		t.tm_mday = day;
		t.tm_hour = hour;
		t.tm_min = minute;
		t.tm_sec = second;
		t.tm_isdst = -1;

		return mktime(&t);
	}

	// Parses a GitHub API UTC timestamp shaped "YYYY-MM-DDTHH:MM:SSZ" (a release's "published_at")
	// into a true UTC epoch value via _mkgmtime - unlike mktime, this applies no local timezone
	// offset, since GitHub's timestamp is already UTC. Returns 0 if unparseable.
	time_t parseGithubUtcTimestamp(const std::string& iso8601)
	{
		int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
		if (sscanf_s(iso8601.c_str(), "%d-%d-%dT%d:%d:%dZ", &year, &month, &day, &hour, &minute, &second) != 6)
		{
			return 0;
		}

		struct tm t = { 0 };
		t.tm_year = year - 1900;
		t.tm_mon = month - 1;
		t.tm_mday = day;
		t.tm_hour = hour;
		t.tm_min = minute;
		t.tm_sec = second;
		t.tm_isdst = 0;

		return _mkgmtime(&t);
	}

	// The local/dev-build comparison path: true timestamps rather than day-level tag comparison, so a
	// release actually published later than this exe was compiled counts as newer regardless of
	// same-day channel churn. If either timestamp fails to parse, the safe answer is "no" (never
	// silently accept an update we can't actually verify is newer).
	bool wasPublishedAfterThisBuild(const std::string& publishedAtUtc)
	{
		time_t releaseEpoch = parseGithubUtcTimestamp(publishedAtUtc);
		time_t compileEpoch = getLocalCompileEpoch();

		if (releaseEpoch == 0 || compileEpoch == 0)
		{
			return false;
		}

		return releaseEpoch > compileEpoch;
	}

	// returns true only if "a" is a validly-formed tag whose date+sequence is strictly greater than "b"'s.
	// "b" being absent/malformed (e.g. a local dev build with no stamped tag) is handled by the caller.
	bool isDmxTagNewer(const std::string& a, const std::string& b)
	{
		int channelA = 0, channelB = 0;
		std::string dateSeqA, dateSeqB;
		if (!parseDmxTag(a, channelA, dateSeqA)) return false;
		if (!parseDmxTag(b, channelB, dateSeqB)) return true; // b isn't a real tag - anything valid outranks it
		return dateSeqA > dateSeqB; // same-length numeric strings - lexicographic == numeric order
	}
}

UpdateChecker::UpdateChecker()
{
	githubRepo = "";
	FILE* fp = NULL;
	if (fopen_s(&fp, UPDATE_CONFIG_FILENAME, "rt") == 0 && fp != NULL)
	{
		char lineOne[512] = "";
		char lineTwo[512] = "";
		fgets(lineOne, 512, fp); // data server base URL, owned by DownloadManager
		if (fgets(lineTwo, 512, fp) != NULL)
		{
			size_t len = strlen(lineTwo);
			while (len > 0 && (lineTwo[len - 1] == '\n' || lineTwo[len - 1] == '\r'))
			{
				lineTwo[--len] = 0;
			}
			githubRepo = normalizeGithubRepo(lineTwo);
		}
		fclose(fp);
	}
}

bool UpdateChecker::hasNetworkConnection()
{
	DWORD connectionFlags = 0;
	// a local query only (no network traffic) - returns immediately either way, so this is safe
	// to call unconditionally before ever touching WinHTTP
	return InternetGetConnectedState(&connectionFlags, 0) != FALSE;
}

bool UpdateChecker::parseUrl(const std::string& url, bool& outIsHttps, std::string& outHost, std::string& outPath)
{
	std::string rest = url;
	outIsHttps = false;

	if (rest.compare(0, 8, "https://") == 0)
	{
		outIsHttps = true;
		rest = rest.substr(8);
	}
	else if (rest.compare(0, 7, "http://") == 0)
	{
		outIsHttps = false;
		rest = rest.substr(7);
	}
	else
	{
		return false; // require an explicit scheme, unlike DownloadManager's bare-host convention
	}

	size_t slashPos = rest.find('/');
	if (slashPos == std::string::npos)
	{
		outHost = rest;
		outPath = "/";
	}
	else
	{
		outHost = rest.substr(0, slashPos);
		outPath = rest.substr(slashPos);
	}

	return !outHost.empty();
}

bool UpdateChecker::httpGetBounded(const std::string& host, const std::string& path, bool isHttps, int timeoutMs, std::string& outBody)
{
	outBody = "";
	bool success = false;

	// DEFAULT_PROXY (rather than the newer AUTOMATIC_PROXY) for broad compatibility with older
	// Windows builds this cabinet software may run on
	HINTERNET hSession = WinHttpOpen(L"DMX-Remake-Updater/1.0",
		WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
		WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	if (hSession == NULL)
	{
		return false;
	}

	// ensure TLS 1.2 is available regardless of this machine's WinHTTP defaults (older Windows
	// builds may default to weaker protocols) - required to reach api.github.com
	DWORD secureProtocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_1 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1;
	WinHttpSetOption(hSession, WINHTTP_OPTION_SECURE_PROTOCOLS, &secureProtocols, sizeof(secureProtocols));

	// bound every phase so an unreachable/black-holed host fails fast instead of hanging boot
	WinHttpSetTimeouts(hSession, timeoutMs, timeoutMs, timeoutMs, timeoutMs);

	HINTERNET hConnect = NULL;
	HINTERNET hRequest = NULL;

	do
	{
		hConnect = WinHttpConnect(hSession, toWide(host).c_str(),
			isHttps ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT, 0);
		if (hConnect == NULL) break;

		// WINHTTP_FLAG_REFRESH forces revalidation with the origin server instead of a cached
		// response - the whole point of these checks is to notice something changed since last time
		DWORD requestFlags = WINHTTP_FLAG_REFRESH | (isHttps ? WINHTTP_FLAG_SECURE : 0);
		hRequest = WinHttpOpenRequest(hConnect, L"GET", toWide(path).c_str(),
			NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, requestFlags);
		if (hRequest == NULL) break;

		const wchar_t* headers = L"User-Agent: DMX-Remake-Updater/1.0\r\nCache-Control: no-cache\r\nPragma: no-cache\r\n";
		if (!WinHttpSendRequest(hRequest, headers, (DWORD)-1, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) break;
		if (!WinHttpReceiveResponse(hRequest, NULL)) break;

		// large enough for a page of ~30 releases (each with several assets) now that
		// checkForExeUpdate() fetches a list and filters client-side rather than a single object
		const size_t maxBodySize = 262144;
		DWORD bytesAvailable = 0;
		while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0)
		{
			if (outBody.length() >= maxBodySize) break;

			DWORD toRead = bytesAvailable;
			if (outBody.length() + toRead > maxBodySize)
			{
				toRead = (DWORD)(maxBodySize - outBody.length());
			}

			std::vector<char> buffer(toRead);
			DWORD bytesRead = 0;
			if (!WinHttpReadData(hRequest, buffer.data(), toRead, &bytesRead)) break;
			if (bytesRead == 0) break;

			outBody.append(buffer.data(), bytesRead);
		}

		success = true;
	} while (false);

	if (hRequest != NULL) WinHttpCloseHandle(hRequest);
	if (hConnect != NULL) WinHttpCloseHandle(hConnect);
	WinHttpCloseHandle(hSession);

	return success;
}

bool UpdateChecker::isHostReachable(const std::string& url, int timeoutMs)
{
	bool isHttps = false;
	std::string host, path;
	if (!parseUrl(url, isHttps, host, path))
	{
		return false;
	}

	std::string discardedBody;
	return httpGetBounded(host, path, isHttps, timeoutMs, discardedBody);
}

// Extraction note (no JSON library in this codebase): only two well-known fields are ever needed
// from a schema this project doesn't control anyway (tag_name, browser_download_url). A parse
// failure here is defined to collapse to "no update found" - inert, never a crash or hang - which
// is judged an acceptable tradeoff against vendoring a full JSON parser for two fields. This is
// the closest analog to the existing hand-rolled manifest.txt triplet parser already in this code.
std::string UpdateChecker::extractJsonStringField(const std::string& json, const std::string& key, size_t searchFromOffset)
{
	std::string needle = "\"" + key + "\":\"";
	size_t keyPos = json.find(needle, searchFromOffset);
	if (keyPos == std::string::npos)
	{
		return "";
	}

	size_t valueStart = keyPos + needle.length();
	size_t valueEnd = json.find('"', valueStart);
	if (valueEnd == std::string::npos)
	{
		return "";
	}

	return json.substr(valueStart, valueEnd - valueStart);
}

std::string UpdateChecker::extractDmxExeDownloadUrl(const std::string& json)
{
	std::string nameNeedle = "\"name\":\"DMX.exe\"";
	size_t namePos = json.find(nameNeedle);
	if (namePos == std::string::npos)
	{
		return "";
	}

	// GitHub's asset schema lists browser_download_url after name within the same object, so the
	// next occurrence found forward from namePos is guaranteed to belong to this same asset
	return extractJsonStringField(json, "browser_download_url", namePos);
}

std::string UpdateChecker::findBestTagForChannel(const std::string& releasesJsonArray, int channel, std::string& outAssetUrl, std::string& outPublishedAt)
{
	outAssetUrl = "";
	outPublishedAt = "";

	char prefixBuf[16] = "";
	sprintf_s(prefixBuf, "DMX-%02d-", channel);
	std::string requiredPrefix = prefixBuf;

	std::string bestTag = "";
	size_t searchPos = 0;

	while (true)
	{
		size_t tagPos = releasesJsonArray.find("\"tag_name\":\"", searchPos);
		if (tagPos == std::string::npos)
		{
			break;
		}

		std::string tag = extractJsonStringField(releasesJsonArray, "tag_name", tagPos);

		// bound this release object's own fields to before the NEXT release's tag_name, so the
		// asset lookup below can't accidentally match a different release's DMX.exe
		size_t nextTagPos = releasesJsonArray.find("\"tag_name\":\"", tagPos + 1);
		size_t objectEnd = (nextTagPos == std::string::npos) ? releasesJsonArray.length() : nextTagPos;

		if (!tag.empty() && tag.compare(0, requiredPrefix.length(), requiredPrefix) == 0)
		{
			std::string releaseSlice = releasesJsonArray.substr(tagPos, objectEnd - tagPos);
			std::string assetUrl = extractDmxExeDownloadUrl(releaseSlice);

			if (!assetUrl.empty() && (bestTag.empty() || isDmxTagNewer(tag, bestTag)))
			{
				bestTag = tag;
				outAssetUrl = assetUrl;
				outPublishedAt = extractJsonStringField(releaseSlice, "published_at");
			}
		}

		searchPos = tagPos + 1;
	}

	return bestTag;
}

bool UpdateChecker::checkForExeUpdate(int channel, std::string& outTag, std::string& outAssetUrl, int timeoutMs)
{
	outTag = "";
	outAssetUrl = "";

	if (githubRepo.empty())
	{
		return false;
	}

	// fetch a page of recent releases (across all channels) and filter client-side by tag prefix,
	// rather than relying on GitHub's own "latest"/prerelease notion - our channel scheme is our own
	std::string path = "/repos/" + githubRepo + "/releases?per_page=30";

	std::string body;
	if (!httpGetBounded("api.github.com", path, true, timeoutMs, body))
	{
		return false;
	}

	std::string assetUrl;
	std::string publishedAt;
	std::string tag = findBestTagForChannel(body, channel, assetUrl, publishedAt);
	if (tag.empty() || assetUrl.empty())
	{
		return false; // nothing published on this channel (within the fetched page), or no DMX.exe asset
	}

	if (strlen(DMX_RELEASE_TAG) > 0)
	{
		// an official CI build - the day+sequence tag comparison is exact (CI hands out sequence
		// numbers in true release order), and applies regardless of which channel the candidate or
		// the current tag belong to - this is what allows a deliberate channel switch to proceed (a
		// newer release on the newly-selected channel) while still refusing any actual downgrade.
		if (!isDmxTagNewer(tag, DMX_RELEASE_TAG))
		{
			return false;
		}
	}
	else
	{
		// a local/dev build - no tag/sequence of its own to compare against, and day-level
		// granularity isn't enough to place it correctly during a heavy-churn day with multiple
		// alpha/beta releases, so compare actual timestamps instead: only offer this release if it
		// was genuinely published after this exe was compiled. This never touches outTag - the
		// returned tag is always exactly what GitHub reports, nothing synthetic is ever displayed.
		if (!wasPublishedAfterThisBuild(publishedAt))
		{
			return false;
		}
	}

	outTag = tag;
	outAssetUrl = assetUrl;
	return true;
}

std::string getCurrentExeTag()
{
	return DMX_RELEASE_TAG;
}
