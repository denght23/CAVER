/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2009 New York University
 *
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
 * Author: Adrian S.-W. Tam <adrian.sw.tam@gmail.com>
 */

#include <stdint.h>
#include <iostream>
#include "hula-header.h"
#include "ns3/buffer.h"
#include "ns3/address-utils.h"
#include "ns3/log.h"

NS_LOG_COMPONENT_DEFINE ("HulaHeader");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (HulaHeader);

    HulaHeader::HulaHeader (uint32_t torID, uint32_t minUtil) {
        payload.data.torID = torID;
        payload.data.minUtil = minUtil;
    }

    HulaHeader::HulaHeader () {
        payload.data.torID = 0;
        payload.data.minUtil = 0;
    }

    HulaHeader::~HulaHeader () {}

    void HulaHeader::SetTorID (uint32_t torID) {
        torID = torID;
    }

    uint32_t HulaHeader::GetTorID () const {
        return payload.data.torID;
    }

    void HulaHeader::SetMinUtil (uint8_t minUtil) {
        minUtil = minUtil;
    }

    uint8_t HulaHeader::GetMinUtil () const {
        return payload.data.minUtil;
    }

    TypeId HulaHeader::GetTypeId (void) {
        static TypeId tid = TypeId ("ns3::HulaHeader")
            .SetParent<Header>()
            .AddConstructor<HulaHeader>()
            ;
        return tid;
    }

    TypeId HulaHeader::GetInstanceTypeId (void) const {
        return GetTypeId();
    }

    void HulaHeader::Print (std::ostream &os) const {
        os << "probe=<" << payload.data.torID << ", " << payload.data.minUtil << ">";
    }

    uint32_t HulaHeader::GetSerializedSize (void)  const {
        return 4;
    }

    void HulaHeader::Serialize (Buffer::Iterator start)  const {
        start.WriteU32(payload.u32view);
    }

    uint32_t HulaHeader::Deserialize (Buffer::Iterator start) {
        payload.u32view = start.ReadU32();
        return GetSerializedSize ();
    }

}; // namespace ns3
