// logManager.cpp implements a non-blocking replacement for Allegro's default al_trace() file writer
//
// PROBLEM: al_trace() is called directly (not through Allegro's own no-op-in-release TRACE() macro)
// from 90+ places throughout this codebase, on whatever thread happens to call it - almost always
// the main/render thread. With no custom handler registered, Allegro's default behavior is to
// synchronously open and append to "allegro.log" on every single call (see al_trace's own docs).
// That is unbounded, unthrottled, synchronous file I/O sitting directly on the thread that also has
// to keep rendering frames and accepting coins - the exact same shape of bug as the COM port hang
// this logging was originally added to help diagnose: an OS call with no timeout, on a thread that
// can't afford to stop. A locked log file (an editor, a log viewer, an AV scanner), a failing or
// very slow disk, or a network/removable working directory could all make that write take an
// unbounded amount of time.
//
// FIX: register a custom trace handler (register_trace_handler(), see allegro/debug.h) that never
// touches the disk itself. It only pushes the message onto a small in-memory ring buffer, protected
// by one CRITICAL_SECTION, and returns immediately. A single dedicated background thread drains that
// buffer to disk on its own schedule, however slowly, without anyone waiting on it.
//
// DEADLOCK SAFETY - the whole design rests on three properties, all worth stating explicitly:
//   1. There is exactly ONE lock in this entire file. A deadlock requires a cycle of two or more
//      threads each waiting on a lock the other holds - that is structurally impossible with a
//      single lock, since no thread can ever be waiting on a second lock while holding the first.
//   2. The lock is NEVER held across a blocking call. Every acquire/release pair here wraps only
//      plain memory operations (ring buffer bookkeeping) - the actual file write happens after the
//      lock is released (see drainQueueToFile()). So even if the disk write itself stalls for a long
//      time, it only delays the *next* drain cycle - it can never make a producer (anything calling
//      al_trace) wait, because producers only ever need the lock for the length of a memcpy.
//   3. Windows CRITICAL_SECTIONs are recursive (the owning thread can re-enter one it already holds),
//      so even an unexpected reentrant call into the handler from the same thread can't self-deadlock.
// Net effect: worst case under real contention or a stuck disk is a full ring buffer (oldest lines
// silently dropped to make room for new ones) - never a wait with no upper bound.

#include "../headers/common.h"
#include "../headers/logManager.h"

namespace
{
	const int LOG_QUEUE_CAPACITY = 512;
	const int LOG_LINE_MAX_LEN = 512;

	// how often the background thread wakes to drain the queue - not latency-critical (this is a
	// debug log, not gameplay), short enough that lines show up promptly for anyone tailing the file
	const DWORD LOG_DRAIN_INTERVAL_MS = 50;

	struct LogQueue
	{
		char lines[LOG_QUEUE_CAPACITY][LOG_LINE_MAX_LEN];
		int head;  // index of the oldest pending line
		int count; // number of pending lines, 0..LOG_QUEUE_CAPACITY
		CRITICAL_SECTION lock;
	};

	LogQueue g_logQueue;
	HANDLE g_logThreadHandle = NULL;

	// precondition: g_logQueue.lock has been initialized
	// postcondition: msg has been queued for the background thread to write; on a full queue, the
	//                oldest pending line is silently dropped to make room - this function never
	//                blocks on anything but the (effectively instantaneous, uncontended) lock itself
	void pushLogLine(const char* msg)
	{
		EnterCriticalSection(&g_logQueue.lock);

		int writePos = (g_logQueue.head + g_logQueue.count) % LOG_QUEUE_CAPACITY;
		strncpy_s(g_logQueue.lines[writePos], LOG_LINE_MAX_LEN, msg, _TRUNCATE);

		if ( g_logQueue.count < LOG_QUEUE_CAPACITY )
		{
			g_logQueue.count++;
		}
		else
		{
			// already full - writePos above landed on the oldest slot (head), overwriting it in
			// place with this newest message, so just advance head to match
			g_logQueue.head = (g_logQueue.head + 1) % LOG_QUEUE_CAPACITY;
		}

		LeaveCriticalSection(&g_logQueue.lock);
	}

	// precondition: logFile is open for appending
	// postcondition: every line queued so far has been written out. The lock is only ever held to
	//                pop a single line, never across the fprintf() below - see the file-level
	//                comment for why that ordering is what actually guarantees this can't deadlock
	//                or stall a producer, no matter how slow the write turns out to be.
	void drainQueueToFile(FILE* logFile)
	{
		bool wroteAnything = false;

		for ( ;; )
		{
			char line[LOG_LINE_MAX_LEN];

			EnterCriticalSection(&g_logQueue.lock);
			if ( g_logQueue.count == 0 )
			{
				LeaveCriticalSection(&g_logQueue.lock);
				break;
			}
			strcpy_s(line, LOG_LINE_MAX_LEN, g_logQueue.lines[g_logQueue.head]);
			g_logQueue.head = (g_logQueue.head + 1) % LOG_QUEUE_CAPACITY;
			g_logQueue.count--;
			LeaveCriticalSection(&g_logQueue.lock);

			fputs(line, logFile);
			wroteAnything = true;
		}

		if ( wroteAnything )
		{
			fflush(logFile); // durability against a crash, at the cost of a bit more disk activity -
			                  // acceptable since this is all off the main thread anyway
		}
	}

	DWORD WINAPI logWriterThreadProc(LPVOID)
	{
		FILE* logFile = NULL;
		if ( fopen_s(&logFile, "allegro.log", "a") != 0 || logFile == NULL )
		{
			// can't log to disk at all - drop everything silently rather than retry or hang;
			// nothing else in the game depends on this file existing
			return 0;
		}

		for ( ;; )
		{
			Sleep(LOG_DRAIN_INTERVAL_MS);
			drainQueueToFile(logFile);
		}
	}

	// This is the function registered with Allegro via register_trace_handler() - al_trace() calls
	// this instead of its own default file writer, from whatever thread called al_trace() in the
	// first place. Must never do anything slower than the pushLogLine() call itself.
	// __cdecl explicitly, matching AL_METHOD's expansion for this platform (see debug.h / almsvc.h)
	// regardless of this project's own default calling convention.
	int __cdecl asyncTraceHandler(const char* msg)
	{
		pushLogLine(msg);
		return 0;
	}
}

void initializeAsyncLogging()
{
	InitializeCriticalSection(&g_logQueue.lock);
	g_logQueue.head = 0;
	g_logQueue.count = 0;

	// start the writer thread, then register the handler - in that order, so nothing can possibly
	// call into asyncTraceHandler() before the queue it relies on is fully set up
	g_logThreadHandle = CreateThread(NULL, 0, logWriterThreadProc, NULL, 0, NULL);
	register_trace_handler(asyncTraceHandler);
}
