// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-        
#ifndef DCTCP_H
#define DCTCP_H

/*
 * A DCTCP source simply changes the congestion control algorithm.
 */

#include "tcp.h"

class DCTCPSrc : public TcpSrc {
 public:
     DCTCPSrc(TcpLogger* logger, TrafficLogger* pktlogger, EventList &eventlist, 
             DynExpTopology *top, int flow_src, int flow_dst, bool longflow);
    ~DCTCPSrc(){}

    // Mechanism
#ifdef TDTCP
    virtual void deflate_window(int slice);
#else
    virtual void deflate_window();
#endif
    virtual void receivePacket(Packet& pkt);
    virtual void rtx_timer_hook(simtime_picosec now,simtime_picosec period);
    virtual void startflow();
    virtual void cleanup();

 private:
#ifdef TDTCP
    vector<uint32_t> _past_cwnd;
    vector<double> _alfa;
    vector<uint32_t> _pkts_seen, _pkts_marked;
#else
    uint32_t _past_cwnd;
    double _alfa;
    uint32_t _pkts_seen, _pkts_marked;
#endif
};

#endif