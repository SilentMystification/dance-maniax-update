// downloadManager.cpp implements a class to find and download software updates
// source file created by Allen Seitz 11/02/2015

#include "../headers/downloadManager.h"
#include <winhttp.h>
#include <vector>
#include <errno.h>

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

	bool parseUrl(const std::string& url, bool& outIsHttps, std::string& outHost, std::string& outPath)
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
			return false;
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
}

DownloadManager::DownloadManager()
{
	serverUrl = "";
	char buffer[512] = "";
	FILE* fp = NULL;
	fopen_s(&fp, UPDATE_CONFIG_FILENAME, "rt");
	if (fp != NULL)
	{
		fgets(buffer, 512, fp);
		fclose(fp);

		size_t len = strlen(buffer);
		while (len > 0 && (buffer[len - 1] == '\n' || buffer[len - 1] == '\r'))
		{
			buffer[--len] = 0;
		}
		serverUrl = buffer;
	}
	resetState();
}

void DownloadManager::resetState()
{
	isDownloadInProgress = false;
	userCancelledDownload = false;
	downloadSucceeded = false;
	downloadFailed = false;
	currentUrl = "";
	currentFilename = "";
	currentBytesDownloaded = 0;
	currentTotalBytes = 0;
}

std::string DownloadManager::doWeNeedAnyUpdates()
{
	std::string retval = "";
	FILE* fp = NULL;
	if ( fopen_s(&fp, MANIFEST_FILENAME, "rt") != 0 )
	{
		if ( !automaticUpdateActive )
		{
			globalError(UPDATE_MISSING_MANIFEST, "please restart");
		}
		return "";
	}

	while (true)
	{
		char checkFile[256] = "", updateFile[256] = "", blankLine[256] = "";

		// read
		fgets(checkFile, 256, fp);
		fgets(updateFile, 256, fp);
		fgets(blankLine, 256, fp);
		if ( strlen(checkFile) > 0 )
		{
			checkFile[strlen(checkFile)-1] = 0; // chop off the trailing '\n'
			updateFile[strlen(updateFile)-1] = 0; // chop off the trailing '\n', it won't be null
		}

		if ( strlen(checkFile) == 0 || strcmp(checkFile, "NULL") == 0 )
		{
			break; // should reach this eventually
		}

		if ( !fileExists(checkFile) )
		{
			retval = updateFile; // found one!
			break;
		}
	}

	fclose(fp);
	return retval;
}

bool DownloadManager::performDownload(const std::string& url, const std::string& filename)
{
	bool isHttps = false;
	std::string host, path;
	if (!parseUrl(url, isHttps, host, path))
	{
		return false;
	}

	bool success = false;
	FILE* outFile = NULL;

	HINTERNET hSession = WinHttpOpen(L"DMX-Remake-Updater/1.0",
		WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
		WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	if (hSession == NULL)
	{
		al_trace("DOWNLOAD:WinHttpOpen failed:GetLastError=%lu:host=%s\n", GetLastError(), host.c_str());
		return false;
	}

	// request the widest reasonable TLS range so this can negotiate with whatever this machine's
	// Windows version actually supports (XP tops out at TLS 1.0 regardless; Win7+ needs TLS 1.2
	// explicitly requested like this to use it at all)
	DWORD secureProtocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_1 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1;
	WinHttpSetOption(hSession, WINHTTP_OPTION_SECURE_PROTOCOLS, &secureProtocols, sizeof(secureProtocols));

	// resolve/connect/send are bounded fairly tightly since the server was just confirmed reachable
	// moments earlier. The RECEIVE timeout is the actual stall detector: it bounds how long we'll
	// wait for the NEXT chunk of data, not the total transfer - a slow-but-still-flowing download
	// keeps resetting it every time a chunk arrives and can take as long overall as it needs, but a
	// connection that goes completely silent trips it instead of hanging forever.
	const int connectTimeoutMs = 15000;
	const int stallTimeoutMs = 30000;
	WinHttpSetTimeouts(hSession, connectTimeoutMs, connectTimeoutMs, stallTimeoutMs, stallTimeoutMs);

	HINTERNET hConnect = NULL;
	HINTERNET hRequest = NULL;

	do
	{
		hConnect = WinHttpConnect(hSession, toWide(host).c_str(),
			isHttps ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT, 0);
		if (hConnect == NULL)
		{
			al_trace("DOWNLOAD:WinHttpConnect failed:GetLastError=%lu:host=%s\n", GetLastError(), host.c_str());
			break;
		}

		DWORD requestFlags = WINHTTP_FLAG_REFRESH | (isHttps ? WINHTTP_FLAG_SECURE : 0);
		hRequest = WinHttpOpenRequest(hConnect, L"GET", toWide(path).c_str(),
			NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, requestFlags);
		if (hRequest == NULL)
		{
			al_trace("DOWNLOAD:WinHttpOpenRequest failed:GetLastError=%lu:path=%s\n", GetLastError(), path.c_str());
			break;
		}

		const wchar_t* headers = L"User-Agent: DMX-Remake-Updater/1.0\r\nCache-Control: no-cache\r\nPragma: no-cache\r\n";
		if (!WinHttpSendRequest(hRequest, headers, (DWORD)-1, WINHTTP_NO_REQUEST_DATA, 0, 0, 0))
		{
			al_trace("DOWNLOAD:WinHttpSendRequest failed:GetLastError=%lu:url=%s\n", GetLastError(), url.c_str());
			break;
		}
		if (!WinHttpReceiveResponse(hRequest, NULL))
		{
			al_trace("DOWNLOAD:WinHttpReceiveResponse failed:GetLastError=%lu:url=%s\n", GetLastError(), url.c_str());
			break;
		}

		// a non-2xx status (404, 403, etc.) is still a "successful" transport-level exchange as far
		// as WinHTTP is concerned - without this check we'd happily save an error page as the exe
		DWORD statusCode = 0;
		DWORD statusCodeSize = sizeof(statusCode);
		WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
			WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusCodeSize, WINHTTP_NO_HEADER_INDEX);
		if (statusCode < 200 || statusCode >= 300)
		{
			al_trace("DOWNLOAD:unexpected HTTP status %lu:url=%s\n", statusCode, url.c_str());
			break;
		}

		// Content-Length lets us show real progress %, but its absence isn't fatal - EOF still ends the loop
		DWORD contentLength = 0;
		DWORD headerSize = sizeof(contentLength);
		if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
			WINHTTP_HEADER_NAME_BY_INDEX, &contentLength, &headerSize, WINHTTP_NO_HEADER_INDEX))
		{
			currentTotalBytes = contentLength;
		}

		if (fopen_s(&outFile, filename.c_str(), "wb") != 0 || outFile == NULL)
		{
			char errBuf[128] = "";
			strerror_s(errBuf, sizeof(errBuf), errno);
			al_trace("DOWNLOAD:unable to open \"%s\" for writing:errno=%d:%s\n", filename.c_str(), errno, errBuf);
			break;
		}

		std::vector<char> buffer(65536);
		while (true)
		{
			if (userCancelledDownload)
			{
				al_trace("DOWNLOAD:cancelled by user:url=%s\n", url.c_str());
				break;
			}

			DWORD bytesAvailable = 0;
			if (!WinHttpQueryDataAvailable(hRequest, &bytesAvailable))
			{
				// includes a timed-out/stalled read - ERROR_WINHTTP_TIMEOUT (12002) means the stall
				// detector tripped; anything else is a genuine transport error
				al_trace("DOWNLOAD:WinHttpQueryDataAvailable failed:GetLastError=%lu:bytesReceived=%I64u:url=%s\n",
					GetLastError(), currentBytesDownloaded, url.c_str());
				break;
			}
			if (bytesAvailable == 0)
			{
				success = true; // EOF - transfer complete
				break;
			}

			DWORD toRead = bytesAvailable < (DWORD)buffer.size() ? bytesAvailable : (DWORD)buffer.size();
			DWORD bytesRead = 0;
			if (!WinHttpReadData(hRequest, buffer.data(), toRead, &bytesRead) || bytesRead == 0)
			{
				al_trace("DOWNLOAD:WinHttpReadData failed:GetLastError=%lu:bytesReceived=%I64u:url=%s\n",
					GetLastError(), currentBytesDownloaded, url.c_str());
				break;
			}

			fwrite(buffer.data(), 1, bytesRead, outFile);
			currentBytesDownloaded += bytesRead;

			// keep the screen responsive during this blocking call, same as the old urlmon callback did
			alternateMainUpdateLoop();
			rm.flip();
		}
	} while (false);

	if (outFile != NULL) fclose(outFile);
	if (hRequest != NULL) WinHttpCloseHandle(hRequest);
	if (hConnect != NULL) WinHttpCloseHandle(hConnect);
	WinHttpCloseHandle(hSession);

	if (!success)
	{
		remove(filename.c_str()); // don't leave a partial/corrupt file behind
	}

	return success;
}

void DownloadManager::downloadFile(std::string url, std::string filename, bool urlIsAbsolute)
{
	if ( isDownloadInProgress )
	{
		return;
	}

	if (!urlIsAbsolute && serverUrl.length() == 0)
	{
		if ( !automaticUpdateActive )
		{
			globalError(UPDATE_MISSING_SERVERURL, "check update_config.txt and restart");
		}
		return;
	}

	isDownloadInProgress = true;
	userCancelledDownload = false;
	downloadSucceeded = false;
	downloadFailed = false;
	currentFilename = filename;
	currentBytesDownloaded = 0;
	currentTotalBytes = 0;

	std::string fullUrl = urlIsAbsolute ? url : (serverUrl + url);

	// thanks, no thanks, no cache please
	char arbitraryNumber[64] = "";
	_itoa_s((int)time(0), arbitraryNumber, 64, 10);
	currentUrl = fullUrl + "?CacheBuster=" + arbitraryNumber;

	bool ok = performDownload(currentUrl, filename);
	downloadSucceeded = ok;
	downloadFailed = !ok;
}

std::string DownloadManager::getCurrentDownloadFilename()
{
	return currentFilename;
}

int DownloadManager::getCurrentDownloadProgress()
{
	if (currentTotalBytes == 0)
	{
		return 0;
	}
	return (int)((currentBytesDownloaded * 100) / currentTotalBytes);
}

void DownloadManager::cancelCurrentDownload()
{
	userCancelledDownload = true;
}

bool DownloadManager::isDownloadComplete()
{
	return downloadSucceeded;
}

bool DownloadManager::didDownloadFail()
{
	return downloadFailed;
}
