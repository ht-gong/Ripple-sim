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
   Routing(RoutingAlgorithm routalg, int64_t cutoff) {
    _routing_algorithm = routalg;
    _cutoff = cutoff;
   };
   simtime_picosec routing_from_ToR(Packet* pkt, simtime_picosec t, simtime_picosec init_time);
   simtime_picosec reroute(Packet* pkt, simtime_picosec t, simtime_picosec finish_push);
   int get_pkt_priority(TcpSrc* tcp_src);
   simtime_picosec routing_from_PQ(Packet* pkt, simtime_picosec t);
   simtime_picosec routing_from_ToR_VLB(Packet* pkt, simtime_picosec t, simtime_picosec init_time);
   simtime_picosec routing_from_ToR_Expander(Packet* pkt, simtime_picosec t, simtime_picosec init_time);
   simtime_picosec routing_from_ToR_OptiRoute(Packet* pkt, simtime_picosec t, simtime_picosec init_time);

   RoutingAlgorithm get_routing_algorithm() {return _routing_algorithm;}
 private:
   RoutingAlgorithm _routing_algorithm;
   int get_path_index(Packet* pkt, int slice);
   int64_t _cutoff = -1;
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