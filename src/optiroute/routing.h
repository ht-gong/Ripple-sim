// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-
#ifndef ROUTING_H
#define ROUTING_H

#include <list>
#include "queue.h"
#include "config.h"
#include "eventlist.h"
#include "network.h"
#include "loggertypes.h"

// A util class
class Routing {
 public:
   Routing() {}
   simtime_picosec routing(Packet* pkt, simtime_picosec t);
};

// Set events to activate next calendar queue
class QueueAlarm : public EventSource {
    public:
        QueueAlarm(EventList &eventlist, int port, Queue* q, DynExpTopology* top);
        void doNextEvent();
    private:
        Queue* _queue; 
        DynExpTopology* _top;
};
#endif