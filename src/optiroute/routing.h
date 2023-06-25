// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-
#ifndef ROUTING_H
#define ROUTING_H

#include <list>
#include "config.h"
#include "eventlist.h"
#include "network.h"
#include "loggertypes.h"

class Queue;
// A util class
class Routing {
 public:
   Routing(RoutingAlgorithm routalg) {
    _routing_algorithm = routalg;
   };
   simtime_picosec routing(Packet* pkt, simtime_picosec t);
   simtime_picosec reroute(Packet* pkt, simtime_picosec t, simtime_picosec finish_push);
   simtime_picosec routing_from_PQ(Packet* pkt, simtime_picosec t);
   simtime_picosec routing_from_ToR_VLB(Packet* pkt, simtime_picosec t, simtime_picosec init_time);
 private:
   RoutingAlgorithm _routing_algorithm;
   int get_path_index(Packet* pkt, int slice);
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