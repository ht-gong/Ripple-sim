// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-
#include "pipe.h"
#include <iostream>
#include <sstream>

#include "network.h"
#include "queue.h"
#include "tcppacket.h"
#include "tcp.h"
#include "ndp.h"
#include "ndppacket.h"

Pipe::Pipe(simtime_picosec delay, EventList& eventlist)
: EventSource(eventlist,"pipe"), _delay(delay),
  _hoho_routing(HohoRouting())
{
    //stringstream ss;
    //ss << "pipe(" << delay/1000000 << "us)";
    //_nodename= ss.str();

    _bytes_delivered = 0;

}

void Pipe::receivePacket(Packet& pkt)
{
    // debug:
    // cout << "   Pipe received a packet." << endl;
    // cout << "long flow? " << pkt.get_longflow() << endl;
    unsigned seqno = 0;
    if(pkt.type() == TCP) {
        seqno = ((TcpPacket*)&pkt)->seqno();
    } else if (pkt.type() == TCPACK) {
        seqno = ((TcpAck*)&pkt)->ackno();
    }
    //cout << "Pipe receivePacket at " << eventlist().now() << " seqno " << seqno << " delay " << _delay << endl;

    if (_inflight.empty()){
        /* no packets currently inflight; need to notify the eventlist
            we've an event pending */
        eventlist().sourceIsPendingRel(*this,_delay);
    }
    _inflight.push_front(make_pair(eventlist().now() + _delay, &pkt));
}

void Pipe::doNextEvent() {
    if (_inflight.size() == 0)
        return;

    Packet *pkt = _inflight.back().second;
    _inflight.pop_back();
    sendFromPipe(pkt);

    if (!_inflight.empty()) {
        // notify the eventlist we've another event pending
        simtime_picosec nexteventtime = _inflight.back().first;
        _eventlist.sourceIsPending(*this, nexteventtime);
    }
}

uint64_t Pipe::reportBytes() {
    uint64_t temp;
    temp = _bytes_delivered;
    _bytes_delivered = 0; // reset the counter
    return temp;
}

// YX: TODO: this is routing for short flows, need to change for long flows
void Pipe::sendFromPipe(Packet *pkt) {
    unsigned seqno = 0;
    if(pkt->type() == TCP) {
        seqno = ((TcpPacket*)&pkt)->seqno();
    } else if (pkt->type() == TCPACK) {
        seqno = ((TcpAck*)&pkt)->ackno();
    }
    //cout << "Pipe sendFromPipe at " << eventlist().now() << " seqno " << seqno << endl;
    if (pkt->is_lasthop()) {
        // we'll be delivering to an NdpSink or NdpSrc based on packet type
        switch (pkt->type()) {
            case NDP:
                {
                    if (pkt->bounced() == false) {
                        if (pkt->size() > 64) // not a header
                            _bytes_delivered = _bytes_delivered + pkt->size(); // increment packet delivered

                        // send it to the sink
                        NdpSink* sink = pkt->get_ndpsink();
                        assert(sink);
                        sink->receivePacket(*pkt);
                    } else {
                        // send it to the source
                        NdpSrc* src = pkt->get_ndpsrc();
                        assert(src);
                        src->receivePacket(*pkt);
                    }

                    break;
                }
            case NDPACK:
            case NDPNACK:
            case NDPPULL:
                {
                    NdpSrc* src = pkt->get_ndpsrc();
                    assert(src);
                    src->receivePacket(*pkt);
                    break;
                }
            case TCP:
                {
                    //cout << "Getting from pipe to dst!\n";
                    TcpSink* dst = ((TcpPacket*)pkt)->get_tcpsink();
                    assert(dst);
                    //cout << "Pipe last hop for " << ((TcpPacket*)pkt)->seqno() << " is " << pkt->get_crtToR() << endl;
                    dst->receivePacket(*pkt);
                    break;
                }
            case TCPACK:
                {
                    //cout << "Getting from pipe to src!\n";
                    TcpSrc* src = ((TcpAck*)pkt)->get_tcpsrc();
                    assert(src);
                    //cout << "Pipe last hop for " << ((TcpAck*)pkt)->ackno() << " is " << pkt->get_crtToR() << endl;
                    src->receivePacket(*pkt);
                    break;
                }
        }
    } else {
        DynExpTopology* top = pkt->get_topology();
        int slice = top->time_to_slice(eventlist().now());
        unsigned seqno;
        if (pkt->type() == TCP) {
            seqno = ((TcpPacket*)pkt)->seqno();
        } else if (pkt->type() == TCPACK) {
            seqno = ((TcpAck*)pkt)->ackno();
        }
        if (pkt->get_crtToR() < 0){
            pkt->set_crtToR(pkt->get_src_ToR());
            //cout << "Pipe first hop for " << seqno << " is " << pkt->get_crtToR() << endl;
        } else {
            int nextToR = top->get_nextToR(slice, pkt->get_crtToR(), pkt->get_crtport());
            if (nextToR >= 0) {// the rotor switch is up
                pkt->set_crtToR(nextToR);
                //cout << "Pipe next hop for " << seqno << "," << pkt->get_src() << "," << pkt->get_dst() << " is " << pkt->get_crtToR() << endl;

            } else { // the rotor switch is down, "drop" the packet

                switch (pkt->type()) {
                    case NDP:
                        cout << "!!! NDP packet clipped in pipe (rotor switch down)" << 
                            "    time = " << timeAsUs(eventlist().now()) << " us"
                            "    current slice = " << slice << 
                            "    slice sent = " << pkt->get_slice_sent() << 
                            "    slice scheduled = " << pkt->get_crtslice() << 
                            "    src = " << pkt->get_src() << ", dst = " << pkt->get_dst() << endl;
                        pkt->free();
                        break;
                    case NDPACK:
                        cout << "!!! NDP packet clipped in pipe (rotor switch down)" << 
                            "    time = " << timeAsUs(eventlist().now()) << " us"
                            "    current slice = " << slice << 
                            "    slice sent = " << pkt->get_slice_sent() << 
                            "    slice scheduled = " << pkt->get_crtslice() << 
                            "    src = " << pkt->get_src() << ", dst = " << pkt->get_dst() << endl;
                        pkt->free();
                        break;
                    case NDPNACK:
                        cout << "!!! NDPNACK packet clipped in pipe (rotor switch down)" << 
                            "    time = " << timeAsUs(eventlist().now()) << " us"
                            "    current slice = " << slice << 
                            "    slice sent = " << pkt->get_slice_sent() << 
                            "    slice scheduled = " << pkt->get_crtslice() << 
                            "    src = " << pkt->get_src() << ", dst = " << pkt->get_dst() << endl;
                        pkt->free();
                        break;
                    case NDPPULL:
                        cout << "!!! NDPPULL packet clipped in pipe (rotor switch down)" << 
                            "    time = " << timeAsUs(eventlist().now()) << " us"
                            "    current slice = " << slice << 
                            "    slice sent = " << pkt->get_slice_sent() << 
                            "    slice scheduled = " << pkt->get_crtslice() << 
                            "    src = " << pkt->get_src() << ", dst = " << pkt->get_dst() << endl;
                        pkt->free();
                        break;
                    case TCPACK:
                        cout << "!!! TCPACK packet clipped in pipe (rotor switch down)" << 
                            "    time = " << timeAsUs(eventlist().now()) << " us"
                            "    current slice = " << slice << 
                            "    slice sent = " << pkt->get_slice_sent() << 
                            "    slice scheduled = " << pkt->get_crtslice() << 
                            "    src = " << pkt->get_src() << ", dst = " << pkt->get_dst() << 
                            " seq " << ((TcpAck*)pkt)->ackno() << endl;
                        pkt->free();
                        break;
                    case TCP:
                        cout << "!!! TCP packet clipped in pipe (rotor switch down)" << 
                            "time = " << timeAsUs(eventlist().now()) << " us"
                            " current slice = " << slice << 
                            " slice sent = " << pkt->get_slice_sent() << 
                            " slice scheduled = " << pkt->get_crtslice() << 
                            " src = " << pkt->get_src() << ", dst = " << pkt->get_dst() << 
                            " seq " << ((TcpPacket*)pkt)->seqno() << endl;
                        TcpPacket *tcppkt = (TcpPacket*)pkt;
                        tcppkt->get_tcpsrc()->add_to_dropped(tcppkt->seqno());
                        //cout << "    FAILED " << pkt->get_ndpsrc()->get_flowsize() << " bytes" << endl; 
                        pkt->free();
                        break;
                }
		assert(0);
                return;
            }
        }
        pkt->inc_crthop(); // increment the hop

        // get the port:
        if (pkt->get_crthop() == pkt->get_maxhops()) { // no more hops defined, need to set downlink port
            assert(0);
            pkt->set_crtport(top->get_lastport(pkt->get_dst()));
            //cout << "   Upcoming (last) port = " << pkt->get_crtport() << endl;

        } else {
            // YX: TODO: The slice to be sent is not used right now,
            // should be used for adding the packet to the calendar queue
            _hoho_routing.routing(pkt, eventlist().now());
        }
        Queue* nextqueue = top->get_queue_tor(pkt->get_crtToR(), pkt->get_crtport());
        nextqueue->receivePacket(*pkt);
    }
}


//////////////////////////////////////////////
//      Aggregate utilization monitor       //
//////////////////////////////////////////////


UtilMonitor::UtilMonitor(DynExpTopology* top, EventList &eventlist)
  : EventSource(eventlist,"utilmonitor"), _top(top)
{
    _H = _top->no_of_nodes(); // number of hosts
    _N = _top->no_of_tors(); // number of racks
    _hpr = _top->no_of_hpr(); // number of hosts per rack
    uint64_t rate = 10000000000 / 8; // bytes / second
    rate = rate * _H;
    //rate = rate / 1500; // total packets per second

    _max_agg_Bps = rate;

    // debug:
    //cout << "max bytes per second = " << rate << endl;

}

void UtilMonitor::start(simtime_picosec period) {
    _period = period;
    _max_B_in_period = _max_agg_Bps * timeAsSec(_period);

    // debug:
    //cout << "_max_pkts_in_period = " << _max_pkts_in_period << endl;

    eventlist().sourceIsPending(*this, _period);
}

void UtilMonitor::doNextEvent() {
    printAggUtil();
}

extern int max_used_queues;
#define OCC_SAMPLE 100
void UtilMonitor::printAggUtil() {

    uint64_t B_sum = 0;

    for (int tor = 0; tor < _N; tor++) {
        for (int downlink = 0; downlink < _hpr; downlink++) {
            Pipe* pipe = _top->get_pipe_tor(tor, downlink);
            B_sum = B_sum + pipe->reportBytes();
        }
    }

    // debug:
    //cout << "Packets counted = " << (int)pkt_sum << endl;
    //cout << "Max packets = " << _max_pkts_in_period << endl;

    double util = (double)B_sum / (double)_max_B_in_period;

    cout << "Util " << fixed << util << " " << timeAsMs(eventlist().now()) << " MaxPkts: " << _top->_max_total_packets << endl;

    cout << "Used max: " << max_used_queues << endl;

    //Calendarqueue occupancy debugging
    /*
    if((int)timeAsMs(eventlist().now()) % OCC_SAMPLE == 0){
        cout << "Occupancy " <<  timeAsMs(eventlist().now());
        for (int tor = 0; tor < _N; tor++) {
            cout  << " ToR:" << tor;
            for (int uplink = _hpr; uplink < _hpr*2; uplink++) {
                Queue* q = _top->get_queue_tor(tor, uplink);
                CompositeQueue *cq = dynamic_cast<CompositeQueue*>(q);
                //'Port:port:size'
                cout  << " Port:" << uplink << ":" << cq->_max_tot_qsize;
                for(int slice = 0; slice < _top->get_nsuperslice() * 2; slice++){
                    //'Slicequeue:slice:size'
                    cout << " Slicequeue:" << slice << ":" << cq->_max_slice_qsize[slice];
                }
            }
        }
        cout << endl;
    }
    */
    

    if (eventlist().now() + _period < eventlist().getEndtime())
        eventlist().sourceIsPendingRel(*this, _period);

}
