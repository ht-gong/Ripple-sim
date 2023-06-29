// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-        
#include "pipe.h"
#include <cstdint>
#include <iostream>
#include <sstream>
#include <set>
#include <algorithm>

#include "queue.h"
#include "tcp.h"
#include "ndp.h"
//#include "rlb.h"
#include "rlbmodule.h"
#include "ndppacket.h"
#include "rlbpacket.h" // added for debugging

Pipe::Pipe(simtime_picosec delay, EventList& eventlist)
: EventSource(eventlist,"pipe"), _delay(delay), _routing(Routing())
{
    //stringstream ss;
    //ss << "pipe(" << delay/1000000 << "us)";
    //_nodename= ss.str();

    _bytes_delivered = 0;

}

void Pipe::receivePacket(Packet& pkt)
{
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

void Pipe::sendFromPipe(Packet *pkt) {
    //cout << "sendFromPipe" << endl;
    if (pkt->is_lasthop()) {
        //cout << "LAST HOP\n";
        // we'll be delivering to an NdpSink or NdpSrc based on packet type
        // ! OR an RlbModule
        switch (pkt->type()) {
            case RLB:
                {
                    // check if it's really the last hop for this packet
                    // otherwise, it's getting indirected, and doesn't count.
                    if (pkt->get_dst() == pkt->get_real_dst()) {

                        _bytes_delivered = _bytes_delivered + pkt->size(); // increment packet delivered

                        // debug:
                        //cout << "!!! incremented packets delivered !!!" << endl;

                    }

                    DynExpTopology* top = pkt->get_topology();
                    RlbModule * mod = top->get_rlb_module(pkt->get_dst());
                    assert(mod);
                    mod->receivePacket(*pkt, 0);
                    break;
                }
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
            case TCP:
                {
                    // send it to the sink
                    _bytes_delivered += pkt->size();
                    TcpSink *sink = pkt->get_tcpsink();
                    assert(sink);
                    sink->receivePacket(*pkt);
                    break;
                }
            case TCPACK:
                {
                    TcpSrc* src = pkt->get_tcpsrc();
                    assert(src);
                    src->receivePacket(*pkt);
                    break;
                }
            case SAMPLE:
                {
                    RTTSampler* sampler = ((SamplePacket*)pkt)->get_sampler();
                    assert(sampler);
                    sampler->receivePacket(*pkt);
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
        // we'll be delivering to a ToR queue
        DynExpTopology* top = pkt->get_topology();

        if (pkt->get_crtToR() < 0)
            pkt->set_crtToR(pkt->get_src_ToR());

        else {
            // this was assuming the topology didn't change:
            //pkt->set_crtToR(top->get_nextToR(pkt->get_slice_sent(), pkt->get_crtToR(), pkt->get_crtport()));

            // under a changing topology, we have to use the current slice:
            // we compute the current slice at the end of the packet transmission
            // !!! as a future optimization, we could have the "wrong" host NACK any packets
            // that go through at the wrong time
            int slice = top->time_to_slice(eventlist().now());

            int nextToR = top->get_nextToR(slice, pkt->get_crtToR(), pkt->get_crtport());
            if (nextToR >= 0) {// the rotor switch is up
                pkt->set_crtToR(nextToR);

            } else { // the rotor switch is down, "drop" the packet

                switch (pkt->type()) {
                    case RLB:
                        {
                            // for now, let's just return the packet rather than implementing the RLB NACK mechanism
                            RlbPacket *p = (RlbPacket*)(pkt);
                            RlbModule* module = top->get_rlb_module(p->get_src()); // returns pointer to Rlb module that sent the packet
                            module->receivePacket(*p, 1); // 1 means to put it at the front of the queue
                            break;
                        }
                    case NDP:
                        cout << "!!! NDP packet clipped in pipe (rotor switch down)" << endl;
                        cout << "    time = " << timeAsUs(eventlist().now()) << " us";
                        cout << "    current slice = " << slice << endl;
                        cout << "    slice sent = " << pkt->get_slice_sent() << endl;
                        cout << "    src = " << pkt->get_src() << ", dst = " << pkt->get_dst() << endl;
                        pkt->free();
                        break;
                    case NDPACK:
                        cout << "!!! NDP ACK clipped in pipe (rotor switch down)" << endl;
                        cout << "    time = " << timeAsUs(eventlist().now()) << " us";
                        cout << "    current slice = " << slice << endl;
                        cout << "    slice sent = " << pkt->get_slice_sent() << endl;
                        cout << "    src = " << pkt->get_src() << ", dst = " << pkt->get_dst() << endl;
                        pkt->free();
                        break;
                    case NDPNACK:
                        cout << "!!! NDP NACK clipped in pipe (rotor switch down)" << endl;
                        cout << "    time = " << timeAsUs(eventlist().now()) << " us";
                        cout << "    current slice = " << slice << endl;
                        cout << "    slice sent = " << pkt->get_slice_sent() << endl;
                        cout << "    src = " << pkt->get_src() << ", dst = " << pkt->get_dst() << endl;
                        pkt->free();
                        break;
                    case NDPPULL:
                        cout << "!!! NDP PULL clipped in pipe (rotor switch down)" << endl;
                        cout << "    time = " << timeAsUs(eventlist().now()) << " us";
                        cout << "    current slice = " << slice << endl;
                        cout << "    slice sent = " << pkt->get_slice_sent() << endl;
                        cout << "    src = " << pkt->get_src() << ", dst = " << pkt->get_dst() << endl;
                        pkt->free();
                        break;
                    case TCP:
                        cout << "!!! TCP packet clipped in pipe (rotor switch down)" << endl;
                        cout << "    time = " << timeAsUs(eventlist().now()) << " us";
                        cout << "    current slice = " << slice << endl;
                        cout << "    slice sent = " << pkt->get_slice_sent() << endl;
                        cout << "    src = " << pkt->get_src() << ", dst = " << pkt->get_dst() << endl;
                        TcpPacket *tcppkt = (TcpPacket*)pkt;
                        tcppkt->get_tcpsrc()->add_to_dropped(tcppkt->seqno());
                        pkt->free();
                        break;
                }
                return;
            }
        }
        pkt->inc_crthop(); // increment the hop

        _routing.routing(pkt, eventlist().now());
        
        Queue* nextqueue = top->get_queue_tor(pkt->get_crtToR(), pkt->get_crtport());
        assert(nextqueue);
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

UtilMonitor::UtilMonitor(DynExpTopology* top, EventList &eventlist, map<uint64_t, TcpSrc*> flow_map)
  : EventSource(eventlist,"utilmonitor"), _top(top), _flow_map(flow_map)
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

/*
    for (int tor = 0; tor < _top->no_of_tors(); tor++) {
        for (int uplink = 0; uplink < _top->no_of_hpr()*2; uplink++) {
            Queue* q = _top->get_queue_tor(tor, uplink);
            q->reportMaxqueuesize();
        }
    }
*/
    /*
    cout << "QueueReport" << endl;
    for (int tor = 0; tor < _top->no_of_tors(); tor++) {
        for (int uplink = _top->no_of_hpr(); uplink < _top->no_of_hpr()*2; uplink++) {
            Queue* q = _top->get_queue_tor(tor, uplink);
            q->reportMaxqueuesize_perslice();
        }
    }
    */
    /*
    map<uint64_t, float> ideal_share = findIdealShare();
    map<uint64_t, float>::iterator is_it;
    for(is_it = ideal_share.begin(); is_it != ideal_share.end(); ++is_it) {
        uint64_t id = is_it->first;
        float share = is_it->second;
        uint64_t tp = 100000*share;
        TcpSrc *tcpsrc = _flow_map[id];
        if(tcpsrc) {
            tcpsrc->cmpIdealCwnd(tp);
            tcpsrc->reportTP();
        }
    }
    */
    if (eventlist().now() + _period < eventlist().getEndtime())
        eventlist().sourceIsPendingRel(*this, _period);
}

map<uint64_t, float> 
UtilMonitor::findIdealShare() {
    map<uint64_t, float> ideal_shares;
    //maps flow id to set of sets of collisions (i.e. flows it shares a queue with)
    map<uint64_t, set<set<uint64_t>>> collisions_per_flow;
    //fill collisions_per_flow
    for (int tor = 0; tor < _top->no_of_tors(); tor++) {
        for (int link = 0; link < _top->no_of_hpr()*2; link++) {
            Queue* q = _top->get_queue_tor(tor, link);
            set<uint64_t> flows = q->getFlows();
            for(uint64_t id : flows) {
                collisions_per_flow[id].insert(flows);
            }
        }
    }
    //count and then sort flows by their max number of collisions
    vector<pair<int,uint64_t>> max_collisions;
    map<uint64_t, set<set<uint64_t>>>::iterator cpf_it;
    for(cpf_it = collisions_per_flow.begin(); cpf_it != collisions_per_flow.end(); ++cpf_it) {
        uint64_t id = cpf_it->first;
        int max = 0;
        set<set<uint64_t>> collisions = cpf_it->second;
        set<set<uint64_t>>::iterator it;
        for(it = collisions.begin(); it != collisions.end(); ++it) {
            max = it->size() > max ? it->size() : max;
        }
        assert(max > 0);
        max_collisions.push_back({max,id});
    }
    if (max_collisions.size() <= 0) return ideal_shares;
    sort(max_collisions.begin(), max_collisions.end(), greater<>()); 
    //iterating through the flows id in descending order of collisions
    //1) no flows in a set have a rate -> all of their rates is 1/sizeof(set)
    //2) some flows in a set have a rate -> the remaining ones get (1-set_rates)/sizeof(remaining)
    //this is correct because the list is sorted and so if no flow has decided a rate, none of the flows
    //in that set of collisions can have a worse rate than any of the others (easily proven by contradiction)
    //and so they share equally.
    vector<pair<int,uint64_t>>::iterator mc_it;
    for(mc_it = max_collisions.begin(); mc_it != max_collisions.end(); ++mc_it) {
        float min_rate = 1;
        uint64_t id = mc_it->second;
        set<set<uint64_t>> collisions = collisions_per_flow[id];
        set<set<uint64_t>>::iterator it;
        for(it = collisions.begin(); it != collisions.end(); ++it) {
            float crt_share = 0;
            set<uint64_t> flows = (*it);
            int remaining = flows.size();
            set<uint64_t>::iterator it;
            for(it = flows.begin(); it != flows.end(); ++it) {
                if(ideal_shares[*it] > 0) remaining -= 1;
                crt_share += ideal_shares[*it];
            }
            assert(crt_share < 1);
            float rate = (1-crt_share)/remaining;
            min_rate = rate < min_rate ? rate : min_rate;
        }
        ideal_shares[id] = min_rate;
    }
    /*
    map<uint64_t, float>::iterator it; 
    for(it = ideal_shares.begin(); it != ideal_shares.end(); ++it) {
        cout << it->first << "," << it->second << endl;
    }
    */
    return ideal_shares;
}
