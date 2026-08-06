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
	// Every candidate port is tried IN PARALLEL, one dedicated thread each, spun up together on the
	// first call - not one port after another. Whichever one answers the handshake first wins; the
	// rest are abandoned in place (see extioManager.cpp for how that's done safely). This function
	// itself never calls a single blocking Win32 API - it only ever polls a plain atomic status flag -
	// so it is UNCONDITIONALLY safe to call every frame no matter what any port's driver does,
	// including if one never returns at all. It gives up and returns false on its own if nothing has
	// reported back within BOOT_WAIT_TIMEOUT_MS, regardless of whether any thread is still running -
	// some other, unrelated device sitting on a COM port (built into the cabinet's PC, a Bluetooth
	// virtual port, whatever) can never hang boot, and can no longer delay finding the real board
	// past its own individual timeout either, since it's no longer sitting in front of it in a queue.

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
	int comPort; // the port that won the race and got adopted, for logging/display only

	void* scanContext;      // ScanContext*, shared (reference-counted) with every per-port thread -
	                         // see extioManager.cpp
	bool scanStarted;        // true once the per-port threads have been kicked off
	UTIME bootWaitElapsed;   // how long THIS object has been waiting on them - independent of how
	                         // long any individual thread has actually been running

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
