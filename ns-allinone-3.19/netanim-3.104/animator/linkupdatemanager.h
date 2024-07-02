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

#ifndef LINKUPDATEMANAGER_H
#define LINKUPDATEMANAGER_H

#include "main/common.h"
#include "animlink.h"

namespace netanim {

typedef struct {
  qreal updateTime;
  AnimLink * link;
  QString description;
} LinkDescription_t;



class LinkUpdateManager
{
public:
    static LinkUpdateManager * getInstance();
    void addLinkUpdate(qreal updateTime, AnimLink *,  QString linkDescription);
    bool updateLinks(qreal currentTime);
    void systemReset();
    qreal getNextUpdateTime();

private:

    LinkUpdateManager();
    typedef std::vector <LinkDescription_t> TimeLinkDescriptionVector_t;
    TimeLinkDescriptionVector_t m_timeLinkDescriptions;
    TimeLinkDescriptionVector_t::const_iterator m_currentIterator;
    qreal m_lastTime;
    qreal m_nextUpdateTime;


};

} // namespace netanim

#endif // LINKUPDATEMANAGER_H
