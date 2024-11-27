/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2023 NUS
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
 * Authors: Chahwan Song <songch@comp.nus.edu.sg>
 */


#pragma once

#include <arpa/inet.h>

#include <map>
#include <queue>
#include <unordered_map>
#include <vector>

#include "ns3/address.h"
#include "ns3/callback.h"
#include "ns3/event-id.h"
#include "ns3/net-device.h"
#include "ns3/object.h"
#include "ns3/packet.h"
#include "ns3/ptr.h"
#include "ns3/settings.h"
#include "ns3/simulator.h"
#include "ns3/tag.h"

namespace ns3 {
    class HulaHeader : public Header {
       public:
        HulaHeader (uint32_t torID, uint32_t minUtil);
        HulaHeader ();
        virtual ~HulaHeader ();

        //Setters
        //不知道为什么要写setter和getter，但是别人都写了
        void SetTorID (uint32_t torID);
        void SetMinUtil (uint8_t minUtil);

        //Getters
        uint32_t GetTorID () const;
        uint8_t GetMinUtil () const;

        static TypeId GetTypeId (void);
        virtual TypeId GetInstanceTypeId (void) const;
        virtual void Print (std::ostream &os) const;
        virtual uint32_t GetSerializedSize (void) const;
        virtual void Serialize (Buffer::Iterator start) const;
        virtual uint32_t Deserialize (Buffer::Iterator start);

       private:
        union {
            struct {
                uint32_t torID:24;
                uint8_t  minUtil:8;
            } data;
            uint32_t u32view;
        } payload;
        //uint32_t torID;
        //uint8_t minUtil;
    };
}  // namespace ns3
