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
 * Contributions: Eugene Kalishenko <ydginster@gmail.com> (Open Source and Linux Laboratory http://dev.osll.ru/)
 */

#ifndef NODEUPDATEMANAGER_H
#define NODEUPDATEMANAGER_H

#include "main/common.h"
#include "animnode.h"

#include <QString>

namespace netanim {

typedef struct {
    qreal updateTime;
    AnimNode * animNode;
    uint8_t r;
    uint8_t g;
    uint8_t b;
    bool visible;
    QString description;
    bool skipColor;
    AnimNodeEnergySource energySource;
} NodeUpdate_t;

class NodeUpdateManager {

public:
    void addNodeUpdate (qreal updateTime, AnimNode *, uint8_t r, uint8_t g, uint8_t b,
                        QString description, bool visible, bool skipColor, const AnimNodeEnergySource& energySource);
    bool updateNodes(qreal currentTime);
    void systemReset();
    static NodeUpdateManager * getInstance();
    qreal getNextUpdateTime();
private:
    NodeUpdateManager();
    typedef std::vector <NodeUpdate_t> NodeUpdateVector_t;
    NodeUpdateVector_t m_nodeUpdates;
    NodeUpdateVector_t::const_iterator m_currentIterator;
    qreal m_lastTime;
    qreal m_nextUpdateTime;

};

} // namespace netanim


#endif // NODEUPDATEMANAGER_H
