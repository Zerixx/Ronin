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

#include "Mutex.h"
#include <deque>

template<class TYPE>
class LockedQueue
{
public:
    ~LockedQueue()
    {

    }

    RONIN_INLINE void add(const TYPE& element)
    {
        mutex.Acquire();
        queue.push_back(element);
        mutex.Release();
    }

    RONIN_INLINE TYPE next()
    {
        mutex.Acquire();
        assert(queue.size() > 0);
        TYPE t = queue.front();
        queue.pop_front();
        mutex.Release();
        return t;
    }

    RONIN_INLINE size_t size()
    {
        mutex.Acquire();
        size_t c = queue.size();
        mutex.Release();
        return c;
    }

    RONIN_INLINE bool empty()
    {   // return true only if sequence is empty
        mutex.Acquire();
        bool isEmpty = queue.empty();
        mutex.Release();
        return isEmpty;
    }

    RONIN_INLINE TYPE get_first_element()
    {
        mutex.Acquire();
        TYPE t; 
        if(queue.size() == 0)
            t = reinterpret_cast<TYPE>(0);
        else
            t = queue.front();
        mutex.Release();
        return t;           
    }

    RONIN_INLINE void pop()
    {
        mutex.Acquire();
        ASSERT(queue.size() > 0);
        queue.pop_front();
        mutex.Release();
    }

    RONIN_INLINE void clear()
    {
        mutex.Acquire();
        queue.clear();
        mutex.Release();
    }

protected:
    std::deque<TYPE> queue;
    Mutex mutex;
};
