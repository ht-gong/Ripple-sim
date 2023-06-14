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
	int slice = top->time_to_slice(t);

    // next port assuming topology does not change
	int nextPort = top->get_port(pkt->get_src_ToR(), top->get_firstToR(pkt->get_dst()),
                        slice, pkt->get_path_index(), pkt->get_hop_index());
    // cout << "Routing crtToR: " << pkt->get_crtToR() << " dstToR: " << top->get_firstToR(pkt->get_dst()) << " slice: " << slice << endl;

    /*
    //track max distance from current queue, short flows only
    if(!pkt->get_longflow() && (pkt->get_crtToR() != top->get_firstToR(pkt->get_dst()))){
        int diff = (((sent_slice-slice) + top->get_nsuperslice()*2) % (top->get_nsuperslice()*2))+1;
        //cout << slice << " " << sent_slice << " " << diff << endl;
        max_used_queues = diff > max_used_queues ? diff : max_used_queues;
    }
    */

    // if this is the last ToR, need to get downlink port
    if(pkt->get_crtToR() == top->get_firstToR(pkt->get_dst())){
        pkt->set_crtport(top->get_lastport(pkt->get_dst()));
        pkt->set_crtslice(0);
        return 0;
    // calculate time from the start of sent_slice, not current slice
    } else {
        //FD uplink never seems to get changed so I just put this here
	    pkt->set_crtport(nextPort);
    }

    Queue* q = top->get_queue_tor(pkt->get_crtToR(), nextPort);
    
    simtime_picosec finish_time = t + q->get_queueing_delay(slice) +
            q->drainTime(pkt) /*229760*/ + timeFromNs(delay_ToR2ToR);
    //calculate delay considering the queue occupancy
    int finish_slice = top->time_to_slice(finish_time); // plus the link delay
    if(top->is_reconfig(finish_time)) {
        finish_slice++;
    }

    if (finish_slice == slice) {
        /*
        if(pkt->get_src() == 489 && pkt->get_dst() == 0){
        cout << "Routing finish_slice==slice slice=" << slice << " t=" << 
            (t + q->get_queueing_delay(sent_slice) + q->drainTime(pkt)) + timeFromNs(delay_ToR2ToR) << endl;
        cout << "qdelay: " << q->get_queueing_delay(sent_slice) << " draintime: " 
            << q->drainTime(pkt) << " slice " << slice << " sent_slice: " <<  sent_slice << " seqno: " << seqno << endl;
        }
        */
        pkt->set_crtslice(slice);
        return (t + q->get_queueing_delay(slice) + q->drainTime(pkt));
    } else {
        /*
        if(pkt->get_src() == 489 && pkt->get_dst() == 0){
        cout << "Rerouting: " <<  (q->get_queueing_delay(sent_slice) + q->drainTime(pkt) + timeFromNs(delay_ToR2ToR)) <<
            " delay t="  << (t + q->get_queueing_delay(sent_slice) + q->drainTime(pkt) + timeFromNs(delay_ToR2ToR)) << endl;
        cout << "qdelay: " << q->get_queueing_delay(sent_slice) << " draintime: " 
            << q->drainTime(pkt) << " slice " << slice << " sent_slice: " << sent_slice << " seqno: " << seqno << endl;
        }
        */
        return reroute(pkt, t);
    }
}

simtime_picosec Routing::reroute(Packet* pkt, simtime_picosec t) {
    DynExpTopology * top = pkt->get_topology();
    int next_absolute_slice = top->time_to_absolute_slice(t) + 1;
    simtime_picosec next_time = top->get_slice_start_time(next_absolute_slice);
    pkt->set_src_ToR(pkt->get_crtToR());
    pkt->set_hop_index(0);
    pkt->set_maxhops(top->get_no_hops(pkt->get_src_ToR(),
            top->get_firstToR(pkt->get_dst()), top->time_to_slice(next_time), pkt->get_path_index()));
    return routing(pkt, next_time);
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
        // cout << "queue slice " << crt << " alarm beginService" << endl;
        _queue->beginService();
    }
    int next_absolute_slice = _top->time_to_absolute_slice(t) + 1;
    simtime_picosec next_time = _top->get_slice_start_time(next_absolute_slice);
    assert(next_time > t);
    eventlist().sourceIsPending(*this, next_time); 
}