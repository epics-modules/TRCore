/* This file is part of the Transient Recorder Framework.
 * It is subject to the license terms in the LICENSE.txt file found in the
 * top-level directory of this distribution and at
 * https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html. No part
 * of the Transient Recorder Framework, including this file, may be copied,
 * modified, propagated, or distributed except according to the terms
 * contained in the LICENSE.txt file.
 */

#include <stddef.h>

#include <algorithm>

#include <epicsThread.h>
#include <epicsGuard.h>
#include <epicsAssert.h>

#include "TRWorkerThread.h"

TRWorkerThread::TRWorkerThread (std::string const &thread_name)
: m_stop(false),
  m_thread(*this, thread_name.c_str(),
           epicsThreadGetStackSize(epicsThreadStackMedium),
           epicsThreadPriorityLow)
{
}

TRWorkerThread::~TRWorkerThread ()
{
    stop();
}

void TRWorkerThread::start ()
{
    m_thread.start();
}

void TRWorkerThread::stop ()
{
    // Set the stop flag.
    {
        epicsGuard<epicsMutex> lock(m_mutex);
        m_stop = true;
    }
    
    // Send a signal to the thread.
    m_event.signal();
    
    // Wait until the thread terminates.
    m_thread.exitWait();
}

void TRWorkerThread::run ()
{
    epicsGuard<epicsMutex> lock(m_mutex);
    
    while (true) {
        // If we are supposed to stop, then do so.
        if (m_stop) {
            break;
        }
        
        if (m_queue.empty()) {
            // Clear the event to avoid a redundant wakeup immediately.
            m_event.tryWait();
            
            // Wait on the event with the lock released.
            {
                epicsGuardRelease<epicsMutex> unlock(lock);
                m_event.wait();
            }
            
            continue;
        }

        // Take and remove a task from the front of the queue.
        TRWorkerThreadTask *task = m_queue.front();
        m_queue.pop_front();
        
        // Execute the task with the lock released.
        {
            epicsGuardRelease<epicsMutex> unlock(lock);
            task->m_runnable->runWorkerThreadTask(task->m_id);
        }
    }
}

TRWorkerThreadTask::TRWorkerThreadTask ()
: m_worker(NULL),
  m_runnable(NULL),
  m_id(0)
{
}


TRWorkerThreadTask::TRWorkerThreadTask (TRWorkerThread *worker, TRWorkerThreadRunnable *runnable, int id)
: m_worker(worker),
  m_runnable(runnable),
  m_id(id)
{
    assert(worker != NULL);
    assert(runnable != NULL);
}

TRWorkerThreadTask::~TRWorkerThreadTask ()
{
    if (m_worker != NULL) {
        cancel();
    }
}

void TRWorkerThreadTask::init (TRWorkerThread *worker, TRWorkerThreadRunnable *runnable, int id)
{
    assert(m_worker == NULL);
    assert(worker != NULL);
    assert(runnable != NULL);
    
    m_worker = worker;
    m_runnable = runnable;
    m_id = id;
}

bool TRWorkerThreadTask::start ()
{
    assert(m_worker != NULL);
    
    {
        epicsGuard<epicsMutex> lock(m_worker->m_mutex);
        
        // Reject request if already in queue.
        if (std::find(m_worker->m_queue.begin(), m_worker->m_queue.end(), this) != m_worker->m_queue.end()) {
            return false;
        }
        
        // Add to queue.
        m_worker->m_queue.push_back(this);
    }
    
    // Send a signal to the thread.
    m_worker->m_event.signal();
    
    return true;
}

bool TRWorkerThreadTask::cancel ()
{
    assert(m_worker != NULL);
    
    epicsGuard<epicsMutex> lock(m_worker->m_mutex);
    
    // Remove it from the queue if it is queued.
    std::deque<TRWorkerThreadTask *>::iterator it =
        std::find(m_worker->m_queue.begin(), m_worker->m_queue.end(), this);
    if (it != m_worker->m_queue.end()) {
        m_worker->m_queue.erase(it);
        return true;
    } else {
        return false;
    }
}
