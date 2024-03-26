// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-        
#ifndef _ECN_QUEUE_H
#define _ECN_QUEUE_H
#include "datacenter/dynexp_topology.h"
#include "queue.h"
/*
 * A simple ECN queue that marks on dequeue as soon as the packet occupancy exceeds the set threshold. 
 */

#include <list>
#include "config.h"
#include "eventlist.h"
#include "network.h"
#include "loggertypes.h"

class ECNQueue : public Queue {
 public:
    ECNQueue(linkspeed_bps bitrate, mem_b maxsize, EventList &eventlist, 
		QueueLogger* logger, mem_b drop, int tor, int port, DynExpTopology *top);
   void receivePacket(Packet & pkt);
   void beginService();
   void completeService();
   mem_b queuesize();
   mem_b slice_queuesize(int slice); 

  private:
     mem_b _K;
     int _state_send;
     vector<mem_b> _queuesize;
     vector<list<Packet*>> _enqueued;
     int _dl_queue;
     DynExpTopology *_top;
     void dump_queuesize();
};

#endif