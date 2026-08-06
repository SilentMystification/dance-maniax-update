// extioManager.cpp implements programming the serial port for DMX
// source file created by Allen Seitz 11/14/2013
// NOTE: this implements a lights board build on an Arduino Mega, other boards are supported in other source files

#include "../headers/extioManager.h"
#include "../headers/inputManager.h"
#include "../headers/lightsManager.h"
#include <algorithm>

#pragma comment(lib, "advapi32.lib") // for the registry reads in enumerateAvailableComPorts()

#define ARDUINO_WAIT_TIME 4000
#define ARDUINO_TIMEOUT 8000

// How long the BOOT LOOP (the main/render thread) waits on the background scan thread before giving
// up and moving on regardless. This is deliberately short: it only ever needs to cover the HAPPY
// PATH (the real board answers on the first or second port tried, which - thanks to ARDUINO_WAIT_TIME
// - takes ~4-5 seconds no matter what), because every failure mode (a bad port, a slow/hanging
// driver, several red-herring devices in a row) is now entirely the background thread's problem, not
// this one's. If the thread hasn't reported back by this point, boot proceeds to SKIP; the thread
// itself is left running and is simply never listened to again (see updateInitialize()).
#define BOOT_WAIT_TIMEOUT_MS 8000

// only used if the registry enumeration below comes back empty, as a last-resort fallback so a
// system where that lookup doesn't pan out (for whatever reason) isn't left unable to find the
// board at all. Windows COM ports are 1-indexed - there is no real COM0.
#define FALLBACK_SCAN_FIRST_PORT 1
#define FALLBACK_SCAN_LAST_PORT 32

extern InputManager im;
extern LightsManager lm;
extern bool usePhoenixIO;

namespace
{
	enum ScanState
	{
		SCAN_RUNNING = 0,
		SCAN_FOUND,
		SCAN_GAVE_UP,
	};

	// Shared between extioManager and the one background scan thread it owns. Once
	// extioManager stops waiting on it (BOOT_WAIT_TIMEOUT_MS elapsed with no result yet), it
	// abandons this struct WITHOUT freeing it - the thread may still be legitimately stuck inside a
	// bad port's driver and could write to `state`/`resultHandle` at any later, unknowable time. The
	// alternative (freeing it and letting the thread write to freed memory) is worse, so this is a
	// deliberate one-time leak in that specific case, not an oversight.
	struct ScanContext
	{
		std::vector<int> ports;
		bool usePhoenix;
		volatile LONG state; // ScanState
		HANDLE resultHandle; // valid only once state == SCAN_FOUND
		int resultComPort;
	};

	// Runs the ENTIRE port hunt - open, configure, wait for the board to reset, handshake - for
	// every candidate port, sequentially, entirely on this one dedicated thread. Deliberately never
	// hands control back to the boot loop mid-scan: if some port's driver hangs forever (or if
	// opening a later port turns out to be serialized behind a lock held by an earlier port's still-
	// blocked open - a real possibility with some legacy/shared serial drivers), this thread hangs
	// with it, but nothing else does, because nothing else ever calls a serial API directly.
	DWORD WINAPI scanThreadProc(LPVOID param)
	{
		ScanContext* ctx = (ScanContext*)param;

		for ( size_t i = 0; i < ctx->ports.size(); i++ )
		{
			int port = ctx->ports[i];
			char portName[32];
			sprintf_s(portName, "\\\\.\\COM%d", port);

			HANDLE h = CreateFileA(portName,
				GENERIC_READ | GENERIC_WRITE,
				0,
				NULL,
				OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL,
				NULL);
			if ( h == INVALID_HANDLE_VALUE )
			{
				continue;
			}

			DCB dcbSerialParams = { 0 };
			dcbSerialParams.DCBlength = sizeof(DCB);
			bool ok = GetCommState(h, &dcbSerialParams) != FALSE;
			if ( ok )
			{
				dcbSerialParams.BaudRate = ctx->usePhoenix ? CBR_115200 : CBR_9600;
				dcbSerialParams.ByteSize = 8;
				dcbSerialParams.StopBits = ONESTOPBIT;
				dcbSerialParams.Parity = NOPARITY;
				ok = SetCommState(h, &dcbSerialParams) != FALSE;
			}
			if ( !ok )
			{
				CloseHandle(h);
				continue;
			}

			// give the board a moment to actually be reset and ready to listen, same as the original
			// synchronous design - just as a Sleep() on this dedicated thread instead of a dt-driven
			// wait on the main thread, since nothing else needs this thread's time
			Sleep(ARDUINO_WAIT_TIME);

			DWORD bytesWritten = 0;
			WriteFile(h, "DMX", 3, &bytesWritten, NULL);

			UTIME waited = 0;
			char reply[3] = { 0, 0, 0 };
			bool gotReply = false;
			while ( waited < ARDUINO_TIMEOUT )
			{
				COMSTAT portStatus = { 0 };
				DWORD portErrors = 0;
				ClearCommError(h, &portErrors, &portStatus);
				if ( portStatus.cbInQue >= 3 )
				{
					DWORD bytesRead = 0;
					gotReply = ReadFile(h, reply, 3, &bytesRead, NULL) && bytesRead == 3;
					break;
				}
				Sleep(50);
				waited += 50;
			}

			if ( gotReply && reply[0] == 'O' && reply[1] == 'K' && reply[2] == '!' )
			{
				ctx->resultHandle = h;
				ctx->resultComPort = port;
				InterlockedExchange(&ctx->state, SCAN_FOUND); // must be the last thing touching ctx
				return 0; // h is intentionally left open - the main thread adopts it
			}

			CloseHandle(h);
		}

		InterlockedExchange(&ctx->state, SCAN_GAVE_UP); // must be the last thing touching ctx
		return 0;
	}

	// Real COM ports currently present on this machine, per Windows itself - not a blind numeric
	// guess. This is what keeps the scan from ever touching a port number that doesn't correspond to
	// an actual device, and is a plain synchronous registry read (no device I/O, no hang risk of its
	// own). Sorted ascending; empty if the key can't be read for some reason (caller falls back to a
	// brute-force scan in that case).
	std::vector<int> enumerateAvailableComPorts()
	{
		std::vector<int> ports;

		HKEY hKey;
		if ( RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DEVICEMAP\\SERIALCOMM", 0, KEY_READ, &hKey) != ERROR_SUCCESS )
		{
			return ports;
		}

		char valueName[256];
		char valueData[256];
		DWORD index = 0;
		for ( ;; )
		{
			DWORD valueNameSize = sizeof(valueName);
			DWORD valueDataSize = sizeof(valueData);
			DWORD type = 0;
			LONG result = RegEnumValueA(hKey, index, valueName, &valueNameSize, NULL, &type, (LPBYTE)valueData, &valueDataSize);
			if ( result == ERROR_NO_MORE_ITEMS )
			{
				break;
			}
			if ( result == ERROR_SUCCESS && type == REG_SZ )
			{
				int portNum = 0;
				if ( sscanf_s(valueData, "COM%d", &portNum) == 1 )
				{
					ports.push_back(portNum);
				}
			}
			index++;
		}

		RegCloseKey(hKey);
		std::sort(ports.begin(), ports.end());
		return ports;
	}
}

extioManager::extioManager()
{
	hSerial = INVALID_HANDLE_VALUE;
	comPort = -1;
	isConnected = false;
	isTalking = false;
	powerOnTime = 0;
	timeSinceLastLampUpdate = 0;
	scanThreadHandle = NULL;
	scanContext = NULL;
	bootWaitElapsed = 0;
}

void extioManager::initialize()
{
	for ( int i = 0; i < PACKET_SIZE; i++ )
	{
		inputBuffer[i] = 0;
	}

	comPortsToTry = enumerateAvailableComPorts();
	if ( comPortsToTry.empty() )
	{
#ifdef DMX_LOGGING
		al_trace("No ports found in HARDWARE\\DEVICEMAP\\SERIALCOMM - falling back to a brute-force COM%d-COM%d scan.\r\n",
			FALLBACK_SCAN_FIRST_PORT, FALLBACK_SCAN_LAST_PORT);
#endif
		for ( int port = FALLBACK_SCAN_FIRST_PORT; port <= FALLBACK_SCAN_LAST_PORT; port++ )
		{
			comPortsToTry.push_back(port);
		}
	}
}

bool extioManager::updateInitialize(UTIME dt)
{
	if ( scanThreadHandle == NULL && scanContext == NULL )
	{
		// first call - kick off the one background thread that owns the entire scan
		ScanContext* ctx = new ScanContext();
		ctx->ports = comPortsToTry;
		ctx->usePhoenix = usePhoenixIO;
		ctx->state = SCAN_RUNNING;
		ctx->resultHandle = INVALID_HANDLE_VALUE;
		ctx->resultComPort = -1;

		scanContext = ctx;
		scanThreadHandle = CreateThread(NULL, 0, scanThreadProc, ctx, 0, NULL);
		bootWaitElapsed = 0;
		return true;
	}

	if ( scanContext == NULL )
	{
		// already gave up and detached in a previous call - nothing left to poll
		return false;
	}

	bootWaitElapsed += dt;

	ScanContext* ctx = (ScanContext*)scanContext;
	LONG state = InterlockedCompareExchange(&ctx->state, SCAN_RUNNING, SCAN_RUNNING); // atomic peek

	if ( state == SCAN_FOUND )
	{
		hSerial = ctx->resultHandle;
		comPort = ctx->resultComPort;
		isConnected = true;
		isTalking = true;
#ifdef DMX_LOGGING
		al_trace("Handshake accepted! Found the IO board on port %d (%s).\r\n",
			comPort, usePhoenixIO ? "Phoenix IO @ 115200" : "Standard IO @ 9600");
#endif
		WriteData("I", 1); // begin the input request loop

		if ( scanThreadHandle != NULL ) CloseHandle((HANDLE)scanThreadHandle);
		scanThreadHandle = NULL;
		delete ctx;
		scanContext = NULL;
		return true;
	}

	if ( state == SCAN_GAVE_UP )
	{
#ifdef DMX_LOGGING
		al_trace("Can't find the IO board on any known com port.\r\n");
#endif

		if ( scanThreadHandle != NULL ) CloseHandle((HANDLE)scanThreadHandle);
		scanThreadHandle = NULL;
		delete ctx;
		scanContext = NULL;
		return false;
	}

	// still running - bounded independently of whatever the background thread is actually doing
	if ( bootWaitElapsed >= BOOT_WAIT_TIMEOUT_MS )
	{
#ifdef DMX_LOGGING
		al_trace("Giving up waiting on the IO board scan after %lums - no board found (the scan thread is left running in the background in case some port's driver is just being slow, but boot is no longer waiting on it).\r\n",
			(unsigned long)bootWaitElapsed);
#endif

		// deliberately do NOT close scanThreadHandle or delete ctx here - the thread may genuinely
		// still be stuck inside a bad port's driver and could touch ctx at any later time; freeing it
		// out from under that write would be a use-after-free. Just stop watching it.
		scanThreadHandle = NULL;
		scanContext = NULL;
		return false;
	}

	return true;
}

bool extioManager::isReady()
{
	return isConnected && isTalking;
}

void extioManager::update(UTIME dt)
{
	if ( isConnected )
	{
		powerOnTime += dt;
		timeSinceLastLampUpdate += dt;
	}
	if ( !isReady() )
	{
		return;
	}

	// actually do work here
	if ( isPacketReady() )
	{
		ReadData(inputBuffer, PACKET_SIZE);
		im.processInputFromExtio(inputBuffer);
		WriteData("I", 1); // continue requesting input in a loop forever
	}
}

void extioManager::updateLamps()
{
	char b0 = 0, b1 = 0, b2 = 0;

	if ( timeSinceLastLampUpdate < 50 )
	{
		//al_trace("--STIFLED LAMP UPDATE-- too soon of an update.\r\n");
		return; // don't do that
	}
	timeSinceLastLampUpdate = 0;

	// write the LEDs
	WriteData("1", 1);
	for ( int i = 0; i < 8; i++ )
	{
		b0 |= lm.getLamp(30+i) ? 1 << i : 0;
		b1 |= lm.getLamp(38+i) ? 1 << i : 0;
		b2 |= lm.getLamp(46+i) ? 1 << i : 0;
	}
	WriteData(&b0, 1);
	WriteData(&b1, 1);
	WriteData(&b2, 1);

	//al_trace("LAMP %d %d %d\n", b0, b1, b2);

	// write the other lamps
	b0 = b1 = b2 = 0;
	WriteData("2", 1);
	for ( int i = 0; i < 6; i++ )
	{
		b0 |= lm.getLamp(24+i) ? 1 << i : 0;
	}
	b1 |= lm.getLamp(spotlightA) ? 1 << 0 : 0;
	b1 |= lm.getLamp(spotlightB) ? 1 << 1 : 0;
	b1 |= lm.getLamp(spotlightC) ? 1 << 2 : 0;
	WriteData(&b0, 1);
	WriteData(&b1, 1);
}

void extioManager::setBaudRate(DWORD rate)
{
	DCB dcb = {0};
	if ( GetCommState(hSerial, &dcb) )
	{
		dcb.BaudRate = rate;
		SetCommState(hSerial, &dcb);
	}
}

//////////////////////////////////////////////////////////////////////////////
// LOW LEVEL SERIAL FUNCTIONS
//////////////////////////////////////////////////////////////////////////////

int extioManager::ReadData(unsigned char *buffer, unsigned int nbChar)
{
    //Number of bytes we'll have read
    DWORD bytesRead;
    //Number of bytes we'll really ask to read
    unsigned int toRead;

    //Use the ClearCommError function to get status info on the Serial port
    ClearCommError(this->hSerial, &this->errors, &this->status);

    //Check if there is something to read
    if(this->status.cbInQue>0)
    {
        //If there is we check if there is enough data to read the required number
        //of characters, if not we'll read only the available characters to prevent
        //locking of the application.
        if(this->status.cbInQue>nbChar)
        {
            toRead = nbChar;
        }
        else
        {
            toRead = this->status.cbInQue;
        }

        //Try to read the require number of chars, and return the number of read bytes on success
        if(ReadFile(this->hSerial, buffer, toRead, &bytesRead, NULL) && bytesRead != 0)
        {
            return bytesRead;
        }
    }

    //If nothing has been read, or that an error was detected return -1
    return -1;
}

bool extioManager::WriteData(char *buffer, unsigned int nbChar)
{
    DWORD bytesSend;

    //Try to write the buffer on the Serial port
    if(!WriteFile(this->hSerial, (void *)buffer, nbChar, &bytesSend, 0))
    {
        //In case it don't work get comm error and return false
        ClearCommError(this->hSerial, &this->errors, &this->status);

        return false;
    }
    else
        return true;
}

bool extioManager::isPacketReady()
{
    ClearCommError(this->hSerial, &this->errors, &this->status);

    return this->status.cbInQue >= PACKET_SIZE;
}
