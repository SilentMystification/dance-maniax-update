// extioManager.h implements programming the serial port for DMX
// source file created by Allen Seitz 11/14/2013

#ifndef _EXTIOMANAGER_H_
#define _EXTIOMANAGER_H_

#include "../headers/common.h"
#include <vector>

class extioManager
{
public:
	extioManager::extioManager();

	void initialize();
	// precondition: called only once before the visible boot sequence begins
	// postcondition: the list of COM ports to try is populated (from the ports Windows currently
	//                reports as present - see HARDWARE\DEVICEMAP\SERIALCOMM in extioManager.cpp -
	//                falling back to a brute-force COM1-COM32 scan only if that comes back empty)

	bool updateInitialize(UTIME dt);
	// precondition: called continuously during the boot loop, until the IO is ready
	// postcondition: hopefully, eventualy, isReady() will return true
	// returns: false when the software should give up on finding an IO board, true when it should continue to wait
	//
	// The entire port hunt (open, configure, handshake) for every candidate port runs on ONE
	// dedicated background thread, spun up on the first call. This function itself never calls a
	// single blocking Win32 API - it only ever polls a plain atomic status flag - so it is
	// UNCONDITIONALLY safe to call every frame no matter what any port's driver does, including if
	// it never returns at all, and including any effect one bad port's open might have on trying to
	// open a DIFFERENT port afterward (e.g. a shared driver-level lock serializing opens across
	// ports - this only ever affects the background thread, never this call). This function gives up
	// and returns false on its own if the background thread hasn't reported back within
	// BOOT_WAIT_TIMEOUT_MS, regardless of whether that thread is still running - some other,
	// unrelated device sitting on a COM port (built into the cabinet's PC, a Bluetooth virtual port,
	// whatever) can never hang boot.

	bool isReady();
	// returns: true when the initialization is complete and the hardware is fully usable

	void update(UTIME dt);
	// precondition: isReady() (checked) or else this call will do nothing
	// postcondition: updates the IO board, if necessary

	void updateLamps();
	// precondition: called only by the LightsManager whenever a lamp change happens, and not more than 20 times per second
	// postcondition: a packet has been sent to the IO board

	// TODO: functions for coin counter and lockout coil

private:
	std::vector<int> comPortsToTry;

	bool isConnected;
	bool isTalking;
	UTIME powerOnTime;
	int comPort; // the port the scan thread found and handed off, for logging/display only

	void* scanThreadHandle; // HANDLE, or NULL before the scan starts / after it's been reaped
	void* scanContext;      // ScanContext*, owned by whichever side (this object, or the thread
	                         // itself once detached past BOOT_WAIT_TIMEOUT_MS) is still interested -
	                         // see extioManager.cpp
	UTIME bootWaitElapsed;   // how long THIS object has been waiting on the scan thread - independent
	                         // of how long the thread itself has actually been running

	void setBaudRate(DWORD rate);
	// precondition: hSerial is a valid open handle
	// postcondition: serial port reconfigured to the new baud rate without closing the handle

	//////////////////////////////////////////////////////////////////////////
	// LOW LEVEL SERIAL FUNCTIONS
	//////////////////////////////////////////////////////////////////////////
private:
	static const int PACKET_SIZE = 3;
	unsigned char inputBuffer[PACKET_SIZE];
	UTIME timeSinceLastLampUpdate;

	void* hSerial;
	COMSTAT status;
	DWORD errors;

	int ReadData(unsigned char *buffer, unsigned int nbChar);
	// precondition: isReady() and buffer can hold nbChar bytes
	// postcondition: reads up to nbChar bytes
	// returns: number of bytes actually read, or -1 if the port was empty

	bool WriteData(char *buffer, unsigned int nbChar);
	// precondition: isReady() and buffer contains at least nbChar bytes
	// postcondition: writes nbChar bytes to the port
	// returns: true on success or false on error

	bool isPacketReady();
	// precondition: isReady()
	// returns: true if there are 8 bytes or more waiting to be read
};

#endif
