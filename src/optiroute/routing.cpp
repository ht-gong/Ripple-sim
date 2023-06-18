// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-
#include "routing.h"
#include <math.h>

#include <climits>
#include <iostream>
#include <sstream>
#include <algorithm>

#include "config.h"
#include "dynexp_topology.h"
#include "network.h"
#include "tcppacket.h"
#include "ndp.h"

extern uint32_t delay_host2ToR; // nanoseconds, host-to-tor link
extern uint32_t delay_ToR2ToR; // nanoseconds, tor-to-tor link

// Routing out of Host into Tor
simtime_picosec Routing::routingFromPQ(Packet* pkt, simtime_picosec t) {
    DynExpTopology* top = pkt->get_topology();
    if (pkt->get_src_ToR() == top->get_firstToR(pkt->get_dst())) {
        // the packet is being sent within the same rack
        pkt->set_lasthop(false);
        pkt->set_crthop(-1);
        pkt->set_hop_index(-1);
        pkt->set_crtToR(-1);
        pkt->set_maxhops(0); // want to select a downlink port immediately
    } else {
        // the packet is being sent between racks

        // we will choose the path based on the current slice
        int slice = top->time_to_slice(t);
        // get the number of available paths for this packet during this slice
        int npaths = top->get_no_paths(pkt->get_src_ToR(),
            top->get_firstToR(pkt->get_dst()), slice);

        if (npaths == 0)
            cout << "Error: there were no paths for slice " << slice  << " src " << pkt->get_src_ToR() <<
                " dst " << top->get_firstToR(pkt->get_dst()) << endl;
        assert(npaths > 0);

        // randomly choose a path for the packet
        // !!! todo: add other options like permutation, etc...
        // int path_index = random() % npaths;
        int path_index = 0;

        pkt->set_slice_sent(slice); // "timestamp" the packet
        pkt->set_fabricts(t);
        pkt->set_path_index(path_index); // set which path the packet will take
        pkt->set_crtslice(slice);

        // set some initial packet parameters used for label switching
        // *this could also be done in NDP before the packet is sent to the NIC
        pkt->set_lasthop(false);
        pkt->set_crthop(-1);
        pkt->set_hop_index(-1);
        pkt->set_crtToR(-1);
        pkt->set_maxhops(top->get_no_hops(pkt->get_src_ToR(),
            top->get_firstToR(pkt->get_dst()), slice, path_index));
    }
}

simtime_picosec Routing::routing(Packet* pkt, simtime_picosec t) {
    unsigned seqno = 0;
    if(pkt->type() == TCP) {
        seqno = ((TcpPacket*)pkt)->seqno();
    } else if (pkt->type() == TCPACK) {
        seqno = ((TcpAck*)pkt)->ackno();
    } else if (pkt->type() == NDP) {
        seqno = ((NdpPacket*)pkt)->seqno();
    }

	DynExpTopology* top = pkt->get_topology();
    // if this is the last ToR, need to get downlink port
    if(pkt->get_crtToR() == top->get_firstToR(pkt->get_dst())){
        pkt->set_crtport(top->get_lastport(pkt->get_dst()));
        pkt->set_crtslice(0);
        return 0;
    // calculate time from the start of sent_slice, not current slice
    }

	int cur_slice = top->time_to_slice(t);
    // next port assuming topology does not change
	int cur_slice_port = top->get_port(pkt->get_src_ToR(), top->get_firstToR(pkt->get_dst()),
                                cur_slice, pkt->get_path_index(), pkt->get_hop_index());
    
    // cout <<seqno << " Routing crtToR: " << pkt->get_crtToR() << "Srctor:" <<pkt->get_src_ToR() << " dstToR: " << top->get_firstToR(pkt->get_dst()) << " slice: " << cur_slice 
    //     << "hop: " << pkt->get_hop_index() << "port: "<<cur_slice_port<< endl;
    if(!cur_slice_port) {
        cout <<seqno << "Routing Failed crtToR: " << pkt->get_crtToR() << "Srctor:" <<pkt->get_src_ToR() << " dstToR: " << top->get_firstToR(pkt->get_dst()) << " slice: " << cur_slice 
        << "hop: " << pkt->get_hop_index() << "port: "<<cur_slice_port<< endl;
        assert(0);
    }

    Queue* cur_q = top->get_queue_tor(pkt->get_crtToR(), cur_slice_port);
    
    simtime_picosec finish_push = t + cur_q->get_queueing_delay(cur_slice) +
            cur_q->drainTime(pkt) /*229760*/;
    int finish_push_slice = top->time_to_slice(finish_push); // plus the link delay

    // simtime_picosec finish_push_nxt = t + nxt_q->get_queueing_delay(cur_slice) +
    //         nxt_q->drainTime(pkt) /*229760*/;
    // int finish_push_nxt_slice = top->time_to_slice(finish_push_nxt);

    // //calculate delay considering the queue occupancy
    simtime_picosec finish_time = t + cur_q->get_queueing_delay(cur_slice) +
            cur_q->drainTime(pkt) /*229760*/ + timeFromNs(delay_ToR2ToR);
    int finish_slice = top->time_to_slice(finish_time);

    // simtime_picosec finish_time_nxt = t + nxt_q->get_queueing_delay(top->absolute_slice_to_slice(cur_slice)) +
    //         nxt_q->drainTime(pkt) /*229760*/ + timeFromNs(delay_ToR2ToR);
    // int finish_nxt_slice = top->time_to_slice(finish_time_nxt);

    // cout <<seqno << "Routing crtToR: " << pkt->get_crtToR() << "Srctor:" <<pkt->get_src_ToR() << " dstToR: " << top->get_firstToR(pkt->get_dst()) << " slice: " << cur_slice 
    //         << "hop: " << pkt->get_hop_index() << "port: "<<cur_slice_port<< " " << t <<"Finish push:" << finish_push_slice << " " << finish_push << "Finish slice:"<<finish_slice<< " " << finish_time << endl;
    if(top->is_reconfig(finish_push)) {
        finish_push_slice = top->absolute_slice_to_slice(finish_push_slice + 1);
    }

    if (finish_push_slice == cur_slice) {
        pkt->set_crtslice(cur_slice);
        pkt->set_crtport(cur_slice_port);
        if(finish_slice != cur_slice && top->get_nextToR(cur_slice, pkt->get_crtToR(), pkt->get_crtport()) != top->get_firstToR(pkt->get_dst())) {
            pkt->set_src_ToR(top->get_nextToR(cur_slice, pkt->get_crtToR(), pkt->get_crtport()));
            pkt->set_hop_index(-1);
            pkt->set_maxhops(top->get_no_hops(pkt->get_src_ToR(),
                                top->get_firstToR(pkt->get_dst()), finish_slice, pkt->get_path_index()));
        }
        
        return finish_time;
    } 
    
    return reroute(pkt, t, finish_time);
}

simtime_picosec Routing::reroute(Packet* pkt, simtime_picosec t, simtime_picosec finish_push) {
    DynExpTopology * top = pkt->get_topology();
    int nxt_slice = top->time_to_absolute_slice(t) + 1;
    pkt->set_src_ToR(pkt->get_crtToR());
    pkt->set_hop_index(0);
    pkt->set_maxhops(top->get_no_hops(pkt->get_src_ToR(),
            top->get_firstToR(pkt->get_dst()), top->absolute_slice_to_slice(nxt_slice), pkt->get_path_index()));


    // cout << "Reroute crtToR: " << pkt->get_crtToR() << " dstToR: " << top->get_firstToR(pkt->get_dst()) << " slice: " << pkt->get_crtslice()<< " at " <<t << "finishing at" << finish_push << endl;
    return routing(pkt, top->get_slice_start_time(nxt_slice));
}

QueueAlarm::QueueAlarm(EventList &eventlist, int port, Queue* q, DynExpTopology* top)
    : EventSource(eventlist, "QueueAlarm"), _queue(q), _top(top)
{
    //nic or downlink queue uses no alarm
    if (!top || top->is_downlink(port)){
        return;
    }
    simtime_picosec t = eventlist.now();
    int next_absolute_slice = top->time_to_absolute_slice(t) + 1;
    simtime_picosec next_time = top->get_slice_start_time(next_absolute_slice);
    assert(next_time > t);
    eventlist.sourceIsPending(*this, next_time); 
}

void QueueAlarm::doNextEvent(){
    simtime_picosec t = eventlist().now();
    int crt = _top->time_to_slice(t);
#ifdef CALENDAR_CLEAR
    //wrap around modulo
    int prev = ((crt-1)+_top->get_nsuperslice()*2)%(_top->get_nsuperslice()*2);
    //clear previous slice calendar queue
    _queue->clearSliceQueue(prev);
#endif

    if(_queue->slice_queuesize(_queue->_crt_tx_slice) > 0) {
        cout << "Packets " << _queue->slice_queuesize(_queue->_crt_tx_slice)/1436 << 
            " stuck in queue tor " << _queue->_tor << " port " << _queue->_port << " slice " << _queue->_crt_tx_slice << endl;
    }
    
    assert(_queue->_crt_tx_slice != crt);
    _queue->_crt_tx_slice = crt;
    // cout << "SETTING TO " << crt << endl;
    if(_queue->_sending_pkt == NULL && _queue->slice_queuesize(crt) > 0){
        // cout << "queue slice " << crt << " " << _queue->_tor << " alarm beginService quesize: " <<_queue->queuesize() << endl;
        _queue->beginService();
    }
    int next_absolute_slice = _top->time_to_absolute_slice(t) + 1;
    simtime_picosec next_time = _top->get_slice_start_time(next_absolute_slice);
    assert(next_time > t);
    eventlist().sourceIsPending(*this, next_time); 
}