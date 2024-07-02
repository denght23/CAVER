/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: John Abraham <john.abraham@gatech.edu>
 */

#ifndef TIMEVALUE_H
#define TIMEVALUE_H

#include "debug/xdebug.h"

#include <map>
#include <QtGlobal>

namespace netanim {

template <class T>
class TimeValue {
public:
    TimeValue();
    TimeValue(const TimeValue & other);
    TimeValue <T> & operator=(const TimeValue <T> & rhs);
    typedef std::map<qreal, T> TimeValue_t;
    void add(qreal t, T value);
    void systemReset();
    void setCurrentTime(qreal t);
    T getCurrent();

private:
    TimeValue_t m_timeValues;
    typename TimeValue<T>::TimeValue_t::const_iterator m_currentIterator;
};

template <class T>
TimeValue<T>::TimeValue()
{

}

template <class T>
TimeValue<T>::TimeValue(const TimeValue & other)
{
    for(typename TimeValue<T>::TimeValue_t::const_iterator i = other.m_timeValues.begin();
        i != other.m_timeValues.end();
        ++i)
    {
        m_timeValues[i->first] = i->second;
    }
    if(!m_timeValues.empty())
    {
        m_currentIterator = m_timeValues.begin();
    }
}


template <class T>
TimeValue <T> &
TimeValue<T>::operator= (const TimeValue <T> & other)
{
    for(typename TimeValue<T>::TimeValue_t::const_iterator i = other.m_timeValues.begin();
        i != other.m_timeValues.end();
        ++i)
    {
        m_timeValues[i->first] = i->second;
    }
    if(!m_timeValues.empty())
    {
        m_currentIterator = m_timeValues.begin();
    }
    return *this;
}


template <class T>
void
TimeValue<T>::add(qreal t, T value)
{
    bool wasEmpty = m_timeValues.empty();
    m_timeValues[t] = value;
    if (wasEmpty)
    {
        m_currentIterator = m_timeValues.begin();
    }
}

template <class T>
void
TimeValue<T>::systemReset()
{
    m_timeValues.clear();
}


template <class T>
T
TimeValue<T>::getCurrent()
{
    if (m_currentIterator == m_timeValues.end())
    {
        return T(m_timeValues.rbegin()->second);
    }
    return m_currentIterator->second;
}

template <class T>
void
TimeValue<T>::setCurrentTime(qreal t)
{
    if (m_timeValues.empty())
        return;

    if ((!t) || (t < m_currentIterator->first))
    {
        m_currentIterator = m_timeValues.begin();
        return;
    }

    for(typename TimeValue<T>::TimeValue_t::const_iterator i = m_currentIterator;
        i != m_timeValues.end();
        ++i)
    {
        if (i->first > t)
        {
            --m_currentIterator;
            return;
        }
        else if (i->first == t)
        {
            return;
        }
        else
        {
            ++m_currentIterator;
        }
    }

}

}
#endif // TIMEVALUE_H
