// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-
#include "pipe.h"
#include <iostream>
#include <sstream>

#include "queue.h"
#include "ndp.h"
#include "ndppacket.h"

Pipe::Pipe(simtime_picosec delay, EventList& eventlist)
: EventSource(eventlist,"pipe"), _delay(delay),
  _hop_delay_forward(HopDelayForward(eventlist))
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

    //pkt.flow().logTraffic(pkt,*this,TrafficLogger::PKT_ARRIVE);
    //cout << "Pipe: Packet received at " << timeAsUs(eventlist().now()) << " us" << endl;
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
    //pkt->flow().logTraffic(*pkt, *this,TrafficLogger::PKT_DEPART);

    // tell the packet to move itself on to the next hop
    //pkt->sendOn();
    //pkt->sendFromPipe();
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
        if (pkt->is_lasthop()) {
            // debug:
            //if (pkt->been_bounced() == true && pkt->bounced() == false) {
            //    cout << "! Pipe sees a last-hop previously-bounced packet" << endl;
            //    cout << "    src = " << pkt->get_src() << endl;
            //}

            // debug:
            //cout << "Pipe - packet on its last hop" << endl;

            // we'll be delivering to an NdpSink or NdpSrc based on packet type
            switch (pkt->type()) {
            case NDP:
            {
                if (pkt->bounced() == false) {

                    //NdpPacket* ndp_pkt = dynamic_cast<NdpPacket*>(pkt);
                    //if (!ndp_pkt->retransmitted())
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
            }
        } else {
            DynExpTopology* top = pkt->get_topology();
            int slice = top->time_to_slice(eventlist().now());

            if (pkt->get_crtToR() < 0)
                pkt->set_crtToR(pkt->get_src_ToR());

            else {
                int nextToR = top->get_nextToR(slice, pkt->get_crtToR(), pkt->get_crtport());
                if (nextToR >= 0) {// the rotor switch is up
                    pkt->set_crtToR(nextToR);

                } else { // the rotor switch is down, "drop" the packet

                    switch (pkt->type()) {
                    case NDP:
                        cout << "!!! NDP packet clipped in pipe (rotor switch down)" << endl;
                        cout << "    time = " << timeAsUs(eventlist().now()) << " us";
                        cout << "    current slice = " << slice << endl;
                        cout << "    slice sent = " << pkt->get_slice_sent() << endl;
                        cout << "    slice scheduled = " << pkt->get_crtslice() << endl;
                        cout << "    src = " << pkt->get_src() << ", dst = " << pkt->get_dst() << endl;
                        cout << "    FAILED " << pkt->get_ndpsrc()->get_flowsize() << " bytes" << endl; 
                        pkt->free();
                        break;
                    case NDPACK:
                        cout << "!!! NDP ACK clipped in pipe (rotor switch down)" << endl;
                        cout << "    time = " << timeAsUs(eventlist().now()) << " us";
                        cout << "    current slice = " << slice << endl;
                        cout << "    slice sent = " << pkt->get_slice_sent() << endl;
                        cout << "    slice scheduled = " << pkt->get_crtslice() << endl;
                        cout << "    src = " << pkt->get_src() << ", dst = " << pkt->get_dst() << endl;
                        cout << "    FAILED " << pkt->get_ndpsrc()->get_flowsize() << " bytes" << endl; 
                        pkt->free();
                        break;
                    case NDPNACK:
                        cout << "!!! NDP NACK clipped in pipe (rotor switch down)" << endl;
                        cout << "    time = " << timeAsUs(eventlist().now()) << " us";
                        cout << "    current slice = " << slice << endl;
                        cout << "    slice sent = " << pkt->get_slice_sent() << endl;
                        cout << "    slice scheduled = " << pkt->get_crtslice() << endl;
                        cout << "    src = " << pkt->get_src() << ", dst = " << pkt->get_dst() << endl;
                        cout << "    FAILED " << pkt->get_ndpsrc()->get_flowsize() << " bytes" << endl; 
                        pkt->free();
                        break;
                    case NDPPULL:
                        cout << "!!! NDP PULL clipped in pipe (rotor switch down)" << endl;
                        cout << "    time = " << timeAsUs(eventlist().now()) << " us";
                        cout << "    current slice = " << slice << endl;
                        cout << "    slice sent = " << pkt->get_slice_sent() << endl;
                        cout << "    slice scheduled = " << pkt->get_crtslice() << endl;
                        cout << "    src = " << pkt->get_src() << ", dst = " << pkt->get_dst() << endl;
                        cout << "    FAILED " << pkt->get_ndpsrc()->get_flowsize() << " bytes" << endl; 
                        pkt->free();
                        break;
                    }
                    // debug:
                    //cout << " Packet got clipped! src = " << pkt->get_src() <<
                    //    ", dst = " << pkt->get_dst() << ", slice sent = " << pkt->get_slice_sent() <<
                    //    ", current hop = " << pkt->get_crthop() << ". Time clipped = " <<
                    //    timeAsUs(eventlist().now()) << " us (current slice " << slice << ")" << endl;
                    //cout << "   superslice = " << superslice << ", reltime = " << reltime << endl;

                    return;
                }
            }
            //cout << "Pipe: the packet is delivered at " << timeAsUs(eventlist().now()) << " us" << endl;
            //cout << "   The curret slice is " << currentslice << endl;
            //cout << "   Upcoming ToR is " << pkt->get_crtToR() << endl;

            pkt->inc_crthop(); // increment the hop

            // debug:
            //RlbPacket *p = (RlbPacket*)(pkt);
            //if (p->seqno() == 1) {
            //    cout << "Pipe: current hop = " << pkt->get_crthop() << ", max hops = " << pkt->get_maxhops() << endl;
            //}

            // get the port:
            if (pkt->get_crthop() == pkt->get_maxhops()) { // no more hops defined, need to set downlink port
                assert(0);
                pkt->set_crtport(top->get_lastport(pkt->get_dst()));
                //cout << "   Upcoming (last) port = " << pkt->get_crtport() << endl;

            } else {
                // YX: TODO: The slice to be sent is not used right now,
                // should be used for adding the packet to the calendar queue
                _hop_delay_forward.routing(pkt, eventlist().now());
            }

            // debug:
            //cout << "pipe delivered to ToR " << pkt->get_crtToR() << ", port " << pkt->get_crtport() << endl;

            Queue* nextqueue = top->get_queue_tor(pkt->get_crtToR(), pkt->get_crtport());
            nextqueue->receivePacket(*pkt);
        }
    //}
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
#define OCC_SAMPLE 1000
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

    cout << "Util " << fixed << util << " " << timeAsMs(eventlist().now()) << endl;

    cout << "Used max: " << max_used_queues << endl;

    //Calendarqueue occupancy debugging
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
    

    if (eventlist().now() + _period < eventlist().getEndtime())
        eventlist().sourceIsPendingRel(*this, _period);

}
