// logManager.h makes al_trace() logging non-blocking, no matter how many places in the codebase
// call it directly (94+ call sites, unguarded, none of them expecting to ever block) - see
// logManager.cpp for the full design and why file I/O on the caller's own thread was a real risk
// (same class of bug as the COM port hang: an unbounded synchronous OS call sitting on a thread that
// also has to keep rendering and accepting coins).

#ifndef _LOGMANAGER_H_
#define _LOGMANAGER_H_

void initializeAsyncLogging();
// precondition: called exactly once, from the main thread, after allegro_init() and before any
//               other code calls al_trace() (so nothing can log before the queue exists to catch it)
// postcondition: a custom Allegro trace handler is registered (see register_trace_handler() in
//                allegro/debug.h) that queues every al_trace() message instead of writing it
//                directly, and a dedicated background thread is running that drains the queue to
//                "allegro.log" on its own schedule. From this point on, al_trace() - called from
//                ANY thread, anywhere in the codebase, unchanged - can never block its caller on
//                file I/O: the handler only ever touches an in-memory ring buffer under a critical
//                section that is never held across a blocking call, so there is nothing for it to
//                deadlock against.

#endif
