/* This file is part of the Transient Recorder Framework.
 * It is subject to the license terms in the LICENSE.txt file found in the
 * top-level directory of this distribution and at
 * https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html. No part
 * of the Transient Recorder Framework, including this file, may be copied,
 * modified, propagated, or distributed except according to the terms
 * contained in the LICENSE.txt file.
 */

/**
 * @file
 * 
 * Defines the TRWorkerThread and associated classes, which implement a simple worker thread.
 */

#ifndef TRANSREC_WORKER_THREAD_H
#define TRANSREC_WORKER_THREAD_H

#include <deque>
#include <string>

#include <epicsMutex.h>
#include <epicsEvent.h>
#include <epicsThread.h>

#include "TRNonCopyable.h"

/**
 * This class is used to execute worker thread tasks.
 * It has a virtual function that is implemented to execute
 * the task.
 */
class TRWorkerThreadRunnable
{
public:
    /**
     * Called on the helper thread request to execute a request.
     * 
     * @param id identifier allowing one class to handle multiple
     *           request types
     */
    virtual void runWorkerThreadTask (int id) = 0;
};

class TRWorkerThreadTask;

/**
 * A simple worker thread with a queue of tasks.
 * 
 * Implements a worker thread to which arbitrary tasks can
 * be queued for execution.
 */
class TRWorkerThread :
    private TRNonCopyable,
    private epicsThreadRunable
{
    friend class TRWorkerThreadTask;
    
public:
    /**
     * Constructor for the worker thread
     * 
     * After construction, start should be called to start
     * operation.
     * 
     * @param thread_name The name for the thread.
     */
    TRWorkerThread (std::string const &thread_name);
    
    /**
     * Destructor for the worker thread.
     * 
     * It does stop(), nevertheless it is a good idea to call
     * stop() explicitly before destruction, due to possible issues
     * with virtual functions.
     */
    ~TRWorkerThread ();
    
    /**
     * Start the worker thread.
     * 
     * This should be called once after construction
     * and must not be called again.
     */
    void start ();
    
    /**
     * Send a signal to the worker thread and wait for it stop.
     */
    void stop ();
    
private:
    bool m_stop;
    epicsThread m_thread;
    epicsMutex m_mutex;
    epicsEvent m_event;
    std::deque<TRWorkerThreadTask *> m_queue;
    
private:
    void run ();
};

/**
 * Represents a task submitted to the worker thread.
 * 
 * The Transient Recorder framework does not create a worker thread itself but
 * provides this class is provided at the convenience of drivers
 * which need to perform synchronous operations beyond what can be
 * achieved using only TRBaseDriver.
 */
class TRWorkerThreadTask :
    private TRNonCopyable
{
    friend class TRWorkerThread;
    
public:
    /**
     * Default constructor.
     * 
     * If this is used, then other functions must not be called before
     * TRWorkerThreadTask::init is called.
     */
    TRWorkerThreadTask ();
    
    /**
     * Constructs the task object.
     * 
     * To queue the task for execution, call start.
     * 
     * @param worker pointer to the TRWorkerThread
     * @param runnable the runnable defining the code to run
     * @param id identifier allowing disambiguation, passed to runnable
     */
    TRWorkerThreadTask (TRWorkerThread *worker, TRWorkerThreadRunnable *runnable, int id);
    
    /**
     * Complete initization when the default constructor was used.
     * 
     * @param worker pointer to the TRWorkerThread
     * @param runnable the runnable defining the code to run
     * @param id identifier allowing disambiguation, passed to runnable
     */
    void init (TRWorkerThread *worker, TRWorkerThreadRunnable *runnable, int id);
    
    /**
     * Destructs the task object.
     * 
     * Destruction will dequeu the task if it is queued
     * (the same as calling cancel).
     * 
     * WARNING: This does not wait for the task to complete
     * if is currently being executed. It is the responsibility
     * of the user to address that issue. Also note the possibility
     * that the worker thread has dequeued the task but not yet
     * called its runWorkerThreadTask function.
     */
    ~TRWorkerThreadTask ();
    
    /**
     * Queues the task for execution.
     * 
     * This will only queue the task if it is not already queued.
     * Note however that this means the task may be queued when it
     * is being executed.
     * 
     * @return True if queued, false if it was already queued.
     */
    bool start ();
    
    /**
     * Dequeues the task.
     * 
     * @return True if dequeued, false if it was not queued.
     */
    bool cancel ();
    
private:
    TRWorkerThread *m_worker;
    TRWorkerThreadRunnable *m_runnable;
    int m_id;
};

#endif
