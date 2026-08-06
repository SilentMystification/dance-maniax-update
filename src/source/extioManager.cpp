// extioManager.cpp implements programming the serial port for DMX
// source file created by Allen Seitz 11/14/2013
// NOTE: this implements a lights board build on an Arduino Mega, other boards are supported in other source files

#include "../headers/extioManager.h"
#include "../headers/inputManager.h"
#include "../headers/lightsManager.h"

#define ARDUINO_WAIT_TIME 4000
#define ARDUINO_TIMEOUT 8000

// how many ports to try, starting at \\.\COM0 - wide enough to survive Windows' habit of bumping a
// device's assigned COM number every time it's physically replugged, so the real IO board doesn't
// fall permanently out of range on a cabinet that's had cables swapped a lot over its lifetime
#define MAX_COM_PORT_INDEX 32

// CreateFile() on a COM port has no OS-level timeout and can block forever on some ports/drivers
// (observed on real cabinets: something sitting on a low COM number - e.g. a Bluetooth virtual COM
// port - that never completes its connection handshake). Bounding it here, rather than giving up
// with no time limit, is what keeps a bad port from hanging the whole game at boot.
#define COM_OPEN_TIMEOUT_MS 1500

extern InputManager im;
extern LightsManager lm;
extern bool usePhoenixIO;

namespace
{
	// state for a port open running on a throwaway worker thread (see comOpenThreadProc). Owned by
	// whichever side is still interested: the extioManager while pendingOpen points at it, or the
	// worker thread itself once "abandoned" is set (having timed out from the main thread's point of
	// view) - the two never touch it at the same time past that handoff.
	struct ComOpenAttempt
	{
		char port[32];
		bool usePhoenix;
		HANDLE threadHandle;
		HANDLE resultHandle; // only valid once "done" is set
		volatile LONG done;
		volatile LONG abandoned;
	};

	DWORD WINAPI comOpenThreadProc(LPVOID param)
	{
		ComOpenAttempt* attempt = (ComOpenAttempt*)param;

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

		attempt->resultHandle = h;

		if ( InterlockedCompareExchange(&attempt->abandoned, 0, 0) != 0 )
		{
			// the main thread stopped waiting on this a while ago - we own cleanup now
			if ( h != INVALID_HANDLE_VALUE ) CloseHandle(h);
			CloseHandle(attempt->threadHandle);
			delete attempt;
			return 0;
		}

		InterlockedExchange(&attempt->done, 1);
		return 0;
	}
}

extioManager::extioManager()
{
	hSerial = INVALID_HANDLE_VALUE;
	comPort = -1;
	isConnected = false;
	isTalking = false;
	powerOnTime = 0;
	connectionState = 0;
	timeSinceLastLampUpdate = 0;
	pendingOpen = NULL;
	pendingOpenElapsed = 0;
}

void extioManager::initialize()
{
	for ( int i = 0; i < PACKET_SIZE; i++ )
	{
		inputBuffer[i] = 0;
	}

//#ifdef DMXDEBUG
//	comPort = TRUSTED_COM_PORT - 1;
//#endif
}

void extioManager::startAsyncOpen(const char* port)
{
	ComOpenAttempt* attempt = new ComOpenAttempt();
	strcpy_s(attempt->port, sizeof(attempt->port), port);
	attempt->usePhoenix = usePhoenixIO;
	attempt->resultHandle = INVALID_HANDLE_VALUE;
	attempt->done = 0;
	attempt->abandoned = 0;
	attempt->threadHandle = CreateThread(NULL, 0, comOpenThreadProc, attempt, 0, NULL);

	pendingOpen = attempt;
	pendingOpenElapsed = 0;
}

bool extioManager::updateInitialize(UTIME dt)
{
	if ( !isConnected )
	{
		if ( pendingOpen == NULL )
		{
			comPort++;
			if ( comPort >= MAX_COM_PORT_INDEX )
			{
				al_trace("Can't find the IO board on any com port.\r\n");
				comPort = -1;
				connectionState = 0;
				return false;
			}

			char port[32];
			sprintf_s(port, "\\\\.\\COM%d", comPort);
			startAsyncOpen(port);
			return true;
		}

		// an open is running on the worker thread - wait for it, but only up to COM_OPEN_TIMEOUT_MS,
		// so a port that never completes its open can't hang the whole game
		ComOpenAttempt* attempt = (ComOpenAttempt*)pendingOpen;
		pendingOpenElapsed += dt;

		if ( InterlockedCompareExchange(&attempt->done, 0, 0) != 0 )
		{
			HANDLE result = attempt->resultHandle;
			CloseHandle(attempt->threadHandle);
			delete attempt;
			pendingOpen = NULL;

			if ( result != INVALID_HANDLE_VALUE )
			{
				al_trace(usePhoenixIO ? "Using Phoenix IO at 115200\r\n" : "Using Standard IO at 9600\r\n");
				hSerial = result;
				isConnected = true;
				powerOnTime = 0;
				connectionState = 1;
			}
			// else: this port isn't it - fall through and try the next one next frame
		}
		else if ( pendingOpenElapsed >= COM_OPEN_TIMEOUT_MS )
		{
			al_trace("Timed out opening port %d - abandoning it and trying the next port.\r\n", comPort);
			InterlockedExchange(&attempt->abandoned, 1);
			pendingOpen = NULL; // the worker thread owns the attempt's cleanup now, whenever (if ever) it returns
		}

		return true;
	}
	else
	{
		powerOnTime += dt;

		if ( connectionState == 1 )
		{
			if ( powerOnTime >= ARDUINO_WAIT_TIME )
			{
				WriteData("DMX", PACKET_SIZE);
				connectionState = 2;
			}
		}
		else if ( connectionState == 2 )
		{
			if ( isPacketReady() )
			{
				ReadData(inputBuffer, PACKET_SIZE);
				if ( inputBuffer[0] == 'O' && inputBuffer[1] == 'K' && inputBuffer[2] == '!' )
				{
					al_trace("Handshake accepted! Found the IO board on port %d.\r\n", comPort);
					isTalking = true;
					connectionState = 3;
					WriteData("I", 1); // begin the input request loop
				}
				else
				{
					// abort - try another port
					powerOnTime = 0;
					isConnected = false;
					connectionState = 0;
					al_trace("The handshake was incorrect on port %d. Checking other ports.\r\n", comPort);
				}
			}
			else if ( powerOnTime >= ARDUINO_TIMEOUT )
			{
				// abort - try another port
				powerOnTime = 0;
				isConnected = false;
				connectionState = 0;
				al_trace("IO board timeout on port %d. Checking other ports.\r\n", comPort);
			}
		}
		else if ( connectionState == 3 )
		{
			al_trace("Do not call updateInitialize() after success!\r\n");
		}
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