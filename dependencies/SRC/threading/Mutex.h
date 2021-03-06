/*
 * Sandshroud Project Ronin
 * Copyright (C) 2015-2017 Sandshroud <https://github.com/Sandshroud>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#pragma once

#ifdef __DragonFly__
#include <pthread.h>
#endif
#ifdef _WIN32_WINNT
#include <Windows.h>
#endif

class SERVER_DECL EasyMutex
{
public:
    friend class Condition;

    /** Initializes a mutex class, with InitializeCriticalSection / pthread_mutex_init
     */
    EasyMutex();

    /** Deletes the associated critical section / mutex
     */
    ~EasyMutex();

    /** Acquires this mutex. If it cannot be acquired immediately, it will block.
     */
    RONIN_INLINE void Acquire()
    {
#if PLATFORM != PLATFORM_WIN
        pthread_mutex_lock(&mutex);
#else
        EnterCriticalSection(&cs);
#endif
    }

    /** Releases this mutex. No error checking performed
     */
    RONIN_INLINE void Release()
    {
#if PLATFORM != PLATFORM_WIN
        pthread_mutex_unlock(&mutex);
#else
        LeaveCriticalSection(&cs);
#endif
    }

    /** Attempts to acquire this mutex. If it cannot be acquired (held by another thread)
     * it will return false.
     * @return false if cannot be acquired, true if it was acquired.
     */
    RONIN_INLINE bool AttemptAcquire()
    {
#if PLATFORM != PLATFORM_WIN
        return (pthread_mutex_trylock(&mutex) == 0);
#else
        return (TryEnterCriticalSection(&cs) == TRUE ? true : false);
#endif
    }

protected:
#if PLATFORM == PLATFORM_WIN
    /** Critical section used for system calls
     */
    CRITICAL_SECTION cs;

#else
    /** Static mutex attribute
     */
    static bool attr_initalized;
    static pthread_mutexattr_t attr;

    /** pthread struct used in system calls
     */
    pthread_mutex_t mutex;
#endif
};

class SERVER_DECL SmartMutex
{
public:
    friend class Condition;

    /** Initializes a mutex class, with InitializeCriticalSection / pthread_mutex_init
     */
    SmartMutex();

    /** Deletes the associated critical section / mutex
     */
    ~SmartMutex();

    /** Acquires this mutex. If it cannot be acquired immediately, it will block.
     */
    RONIN_INLINE void Acquire(bool RestrictUnlocking = false)
    {
        if(ronin_GetThreadId() == m_activeThread)
        {
            m_ThreadCalls++;
            return;
        }

#if PLATFORM != PLATFORM_WIN
        pthread_mutex_lock(&mutex);
#else
        EnterCriticalSection(&cs);
#endif
        m_ThreadCalls++;
        m_activeThread = ronin_GetThreadId();
        LockReleaseToThread = RestrictUnlocking;
    }

    /** Releases this mutex. No error checking performed
     */
    RONIN_INLINE void Release()
    {
        // If we're locked to this specific thread, and we're unlocked from another thread, error
        if(LockReleaseToThread && ronin_GetThreadId() != m_activeThread)
            ASSERT(false && "MUTEX RELEASE CALLED FROM NONACTIVE THREAD!");

        m_ThreadCalls--;
        if(m_ThreadCalls == 0)
        {
            m_activeThread = 0;
#if PLATFORM != PLATFORM_WIN
            pthread_mutex_unlock(&mutex);
#else
            LeaveCriticalSection(&cs);
#endif
        }
    }

    /** Attempts to acquire this mutex. If it cannot be acquired (held by another thread)
     * it will return false.
     * @return false if cannot be acquired, true if it was acquired.
     */
    RONIN_INLINE bool AttemptAcquire(bool RestrictUnlocking = false)
    {
        if(ronin_GetThreadId() == m_activeThread)
        {
            m_ThreadCalls++;
            return true;
        }

#if PLATFORM != PLATFORM_WIN
        if(pthread_mutex_trylock(&mutex) == 0)
#else
        if(TryEnterCriticalSection(&cs) == TRUE)
#endif
        {
            m_ThreadCalls++;
            m_activeThread = ronin_GetThreadId();
            LockReleaseToThread = RestrictUnlocking;
            return true;
        }
        return false;
    }

protected:
    bool LockReleaseToThread;
    uint32 m_activeThread;
    uint32 m_ThreadCalls;

#if PLATFORM == PLATFORM_WIN
    /** Critical section used for system calls
     */
    CRITICAL_SECTION cs;

#else
    /** Static mutex attribute
     */
    static bool attr_initalized;
    static pthread_mutexattr_t attr;

    /** pthread struct used in system calls
     */
    pthread_mutex_t mutex;
#endif
};

#define Mutex EasyMutex
