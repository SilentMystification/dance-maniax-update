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

// How long the BOOT LOOP (the main/render thread) waits on the parallel port scan before giving up
// and moving on regardless. Every candidate port is tried at once (see beginParallelScan()), so this
// mainly just needs to cover the real board's own answer time: ~4-5s (ARDUINO_WAIT_TIME plus reply
// time), no matter how many OTHER ports are being tried alongside it. If nothing has reported back by
// this point, boot proceeds to SKIP; any still-running per-port threads are left alone and simply
// never listened to again (see updateInitialize()). 8s comfortably covers PORT_OPEN_TIMEOUT_MS (2s,
// in the unlikely case the real board's own port is itself slow to open) + ARDUINO_WAIT_TIME (4s) +
// reply time - and unlike the old sequential design, other bad ports running alongside it don't add
// to that at all, since they're no longer in a queue in front of it.
#define BOOT_WAIT_TIMEOUT_MS 8000

// Bounds CreateFile()+GetCommState()+SetCommState() for a SINGLE port (see openPortBounded()) - NOT
// the main thread, and not any OTHER port's thread either, since every candidate port gets its own
// thread now (see beginParallelScan()). This just keeps one port's own thread from sitting stuck
// inside CreateFile() forever if that port's driver never returns - it has no effect on any other
// port, which was already racing independently regardless.
#define PORT_OPEN_TIMEOUT_MS 2000

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

	// Shared by every per-port thread (one per candidate port, all started together - see
	// beginParallelScan()) plus extioManager itself, all racing to try their own port. Reference-
	// counted (refCount starts at portCount + 1, one per thread plus one held by extioManager) rather
	// than owned by any single side, because with several threads touching the same ctx concurrently
	// there is no single moment where any one of them can safely say "everyone else is definitely
	// done with this too" - whichever side's decrement happens to bring refCount to 0 is, by
	// construction, provably the last one still holding a reference, and frees it right then. A
	// thread whose own port is genuinely stuck forever (never returns from CreateFile) never reaches
	// its decrement, so refCount for that ctx never reaches 0 and it leaks - deliberately: the
	// alternative is freeing it while that thread might still dereference it later, which is worse.
	struct ScanContext
	{
		bool usePhoenix;
		volatile LONG state;          // ScanState
		HANDLE resultHandle;          // valid only once state == SCAN_FOUND
		int resultComPort;
		volatile LONG portsRemaining; // countdown of per-port threads that haven't finished their OWN
		                               // attempt yet, independent of refCount - see its use below
		volatile LONG refCount;       // see comment above
	};

	// precondition: called only by a thread that holds a reference (i.e. hasn't released its own
	//               refCount decrement yet)
	// postcondition: this call's reference is released; if it was the last one, ctx is freed - the
	//                caller must not touch ctx again after this returns, under any circumstances
	void releaseScanContextRef(ScanContext* ctx)
	{
		if ( InterlockedDecrement(&ctx->refCount) == 0 )
		{
			delete ctx;
		}
	}

	enum PortOpenState
	{
		PORT_OPEN_PENDING = 0,          // still in flight, neither side has claimed it yet
		PORT_OPEN_CLAIMED_BY_SCANNER,   // the scan thread gave up waiting and claimed cleanup duty
		PORT_OPEN_CLAIMED_BY_WORKER,    // the worker finished (possibly late) and left a result
	};

	// State for a single port's open, running on its own short-lived worker thread (see
	// portOpenWorkerProc). Same atomic-claim handoff as ScanContext and for the same reason: exactly
	// one of the scan thread (on timeout) or the worker (on finishing) ever cleans this up, decided
	// by an atomic PENDING -> CLAIMED_BY_* transition, so there's no window where both or neither do.
	struct PortOpenAttempt
	{
		char port[32];
		bool usePhoenix;
		HANDLE threadHandle;
		HANDLE resultHandle; // only meaningful to whichever side wins CLAIMED_BY_WORKER
		volatile LONG state;
	};

	DWORD WINAPI portOpenWorkerProc(LPVOID param)
	{
		PortOpenAttempt* attempt = (PortOpenAttempt*)param;

		HANDLE h = CreateFileA(attempt->port,
			GENERIC_READ | GENERIC_WRITE,
			0,
			NULL,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			NULL);

		if ( h != INVALID_HANDLE_VALUE )
		{
			DCB dcbSerialParams = { 0 };
			dcbSerialParams.DCBlength = sizeof(DCB);
			bool ok = GetCommState(h, &dcbSerialParams) != FALSE;
			if ( ok )
			{
				dcbSerialParams.BaudRate = attempt->usePhoenix ? CBR_115200 : CBR_9600;
				dcbSerialParams.ByteSize = 8;
				dcbSerialParams.StopBits = ONESTOPBIT;
				dcbSerialParams.Parity = NOPARITY;
				ok = SetCommState(h, &dcbSerialParams) != FALSE;
			}
			if ( !ok )
			{
				CloseHandle(h);
				h = INVALID_HANDLE_VALUE;
			}
		}

		LONG prev = InterlockedCompareExchange(&attempt->state, PORT_OPEN_CLAIMED_BY_WORKER, PORT_OPEN_PENDING);
		if ( prev == PORT_OPEN_PENDING )
		{
			// we got here first - leave the result for the scan thread to collect
			attempt->resultHandle = h;
		}
		else
		{
			// the scan thread already claimed this as abandoned - it's not looking at this struct
			// anymore, so we own cleaning up everything ourselves
			if ( h != INVALID_HANDLE_VALUE ) CloseHandle(h);
			CloseHandle(attempt->threadHandle);
			delete attempt;
		}

		return 0;
	}

	// Called only from the scan thread. Opens and configures a single port, bounded to
	// PORT_OPEN_TIMEOUT_MS regardless of how long the underlying driver actually takes - returns
	// INVALID_HANDLE_VALUE on failure OR timeout, either way leaving the scan thread free to move on
	// to the next port immediately. A timed-out attempt is handed off to portOpenWorkerProc to clean
	// up whenever (if ever) it finishes, never waited on further.
	HANDLE openPortBounded(const char* port, bool usePhoenix)
	{
		PortOpenAttempt* attempt = new PortOpenAttempt();
		strcpy_s(attempt->port, sizeof(attempt->port), port);
		attempt->usePhoenix = usePhoenix;
		attempt->resultHandle = INVALID_HANDLE_VALUE;
		attempt->state = PORT_OPEN_PENDING;
		attempt->threadHandle = CreateThread(NULL, 0, portOpenWorkerProc, attempt, 0, NULL);

		DWORD waitResult = WaitForSingleObject(attempt->threadHandle, PORT_OPEN_TIMEOUT_MS);

		if ( waitResult == WAIT_OBJECT_0 )
		{
			// the worker thread has already exited - waiting on its handle until it's signaled is
			// itself a synchronization point, so reading resultHandle here needs no further atomics
			HANDLE result = attempt->resultHandle;
			CloseHandle(attempt->threadHandle);
			delete attempt;
			return result;
		}

		// timed out - hand off cleanup duty via the same atomic claim the worker checks, so whichever
		// side finishes "second" (in the rare case the worker completes right as we time out) safely
		// detects it and cleans up, with no window where both or neither do
		LONG prev = InterlockedCompareExchange(&attempt->state, PORT_OPEN_CLAIMED_BY_SCANNER, PORT_OPEN_PENDING);
		if ( prev == PORT_OPEN_PENDING )
		{
			// we claimed it first - the worker will see CLAIMED_BY_SCANNER when (if ever) it finishes
			// and clean up after itself. Nothing more for us to do; we never touch this struct again.
			return INVALID_HANDLE_VALUE;
		}

		// the worker actually finished right as we were giving up on it - it left a result waiting
		// for us, so it's our job to discard it cleanly (we're not adopting it - the scan is moving
		// on to the next port) rather than leak the handle
		HANDLE result = attempt->resultHandle;
		CloseHandle(attempt->threadHandle);
		delete attempt;
		if ( result != INVALID_HANDLE_VALUE )
		{
			CloseHandle(result);
		}
		return INVALID_HANDLE_VALUE;
	}

	// One of these per candidate port, all created together (see beginParallelScan()) - each thread
	// owns exactly one port for its entire lifetime, so it can only ever be delayed by that ONE
	// port's own driver, never by any other port's.
	struct PortScanThreadParam
	{
		ScanContext* ctx; // a reference this thread owns - must release exactly once, at the end
		int port;
		HANDLE selfHandle; // set by beginParallelScan() after CreateThread() returns, before resuming -
		                    // lets this thread close its own thread handle itself, so nothing external
		                    // needs to track or reap a per-port thread handle at all
	};

	// Tries exactly one port: open, configure, wait for the board to reset, handshake. Every
	// candidate port gets one of these running concurrently (see beginParallelScan()) instead of
	// working through the list one at a time, so a single unresponsive port only ever costs its own
	// timeout, in parallel with everyone else, rather than sitting in front of the real board in a
	// queue. Whichever thread's handshake succeeds first wins the race to publish SCAN_FOUND; every
	// other thread (already in flight or still to finish) notices it lost - either by losing the
	// atomic claim below, or simply never getting this far - and just cleans up quietly.
	DWORD WINAPI portScanThreadProc(LPVOID param)
	{
		PortScanThreadParam* p = (PortScanThreadParam*)param;
		ScanContext* ctx = p->ctx;
		int port = p->port;
		HANDLE selfHandle = p->selfHandle;
		delete p;

		char portName[32];
		sprintf_s(portName, "\\\\.\\COM%d", port);

		HANDLE h = openPortBounded(portName, ctx->usePhoenix);
		bool foundBoard = false;

		if ( h != INVALID_HANDLE_VALUE )
		{
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
				// resultHandle/resultComPort MUST be written before the state transition below -
				// extioManager only ever reads them after observing state == SCAN_FOUND via the same
				// kind of interlocked peek, which is what makes this write visible in time
				LONG prevState = InterlockedCompareExchange(&ctx->state, SCAN_FOUND, SCAN_RUNNING);
				if ( prevState == SCAN_RUNNING )
				{
					ctx->resultHandle = h;
					ctx->resultComPort = port;
					foundBoard = true;
				}
				// else: another port's thread already won the race a moment earlier - fall through
				// and close our own handle below instead of adopting it
			}

			if ( !foundBoard )
			{
				CloseHandle(h);
			}
		}

		// whether or not this port panned out, report in: if every other port has also finished and
		// none of them found the board either, this is the thread that declares the scan given up
		if ( InterlockedDecrement(&ctx->portsRemaining) == 0 )
		{
			InterlockedCompareExchange(&ctx->state, SCAN_GAVE_UP, SCAN_RUNNING); // no-op if already FOUND
		}

		releaseScanContextRef(ctx); // must be the last thing this thread does with ctx
		CloseHandle(selfHandle);
		return 0;
	}

	// Starts one thread per candidate port, all racing concurrently, and returns immediately without
	// waiting on any of them - extioManager polls ctx->state on its own schedule from
	// updateInitialize(). ctx is heap-allocated and reference-counted (see ScanContext) since it's
	// about to be shared by every one of these threads plus the caller.
	ScanContext* beginParallelScan(const std::vector<int>& ports, bool usePhoenix)
	{
		ScanContext* ctx = new ScanContext();
		ctx->usePhoenix = usePhoenix;
		ctx->state = ports.empty() ? SCAN_GAVE_UP : SCAN_RUNNING; // nothing to scan - degenerate, but
		                                                          // should never happen (initialize()
		                                                          // always falls back to a numeric
		                                                          // range if the registry gives none)
		ctx->resultHandle = INVALID_HANDLE_VALUE;
		ctx->resultComPort = -1;
		ctx->portsRemaining = (LONG)ports.size();
		ctx->refCount = (LONG)ports.size() + 1; // one per thread, plus the caller's own reference

		for ( size_t i = 0; i < ports.size(); i++ )
		{
			PortScanThreadParam* p = new PortScanThreadParam();
			p->ctx = ctx;
			p->port = ports[i];

			// CREATE_SUSPENDED so the thread can be handed its own HANDLE (for it to self-close on
			// exit) before it starts running - avoids extioManager needing to track a per-port thread
			// handle at all
			HANDLE threadHandle = CreateThread(NULL, 0, portScanThreadProc, p, CREATE_SUSPENDED, NULL);
			p->selfHandle = threadHandle;
			ResumeThread(threadHandle);
		}

		return ctx;
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
	scanContext = NULL;
	scanStarted = false;
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
	if ( !scanStarted )
	{
		// first call - fire off one thread per candidate port, all racing in parallel
		scanContext = beginParallelScan(comPortsToTry, usePhoenixIO);
		scanStarted = true;
		bootWaitElapsed = 0;
		return true;
	}

	if ( scanContext == NULL )
	{
		// already gave up and released our reference in a previous call - nothing left to poll
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

		releaseScanContextRef(ctx); // extioManager's own reference - may or may not be the last one
		scanContext = NULL;
		return true;
	}

	if ( state == SCAN_GAVE_UP )
	{
#ifdef DMX_LOGGING
		al_trace("Can't find the IO board on any known com port.\r\n");
#endif

		releaseScanContextRef(ctx);
		scanContext = NULL;
		return false;
	}

	// still running - bounded independently of whatever any individual port thread is actually doing
	if ( bootWaitElapsed >= BOOT_WAIT_TIMEOUT_MS )
	{
#ifdef DMX_LOGGING
		al_trace("Giving up waiting on the IO board scan after %lums - no board found (any still-running port threads are left alone in the background in case one of them is just being slow, but boot is no longer waiting on them).\r\n",
			(unsigned long)bootWaitElapsed);
#endif

		releaseScanContextRef(ctx); // just releases OUR reference - safe even if per-port threads are
		                            // still running and holding their own; whichever of them is truly
		                            // last to finish frees ctx itself, we just stop watching it here
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
