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
// #define DEBUG

extern uint32_t delay_host2ToR; // nanoseconds, host-to-tor link
extern uint32_t delay_ToR2ToR; // nanoseconds, tor-to-tor link

simtime_picosec Routing::routingFromPQ(Packet* pkt, simtime_picosec t) {
    DynExpTopology* top = pkt->get_topology();
    pkt->set_lasthop(false);
    pkt->set_crthop(-1);
    pkt->set_crtToR(-1);
    
    if (pkt->get_src_ToR() == top->get_firstToR(pkt->get_dst())) {
        // the packet is being sent within the same rack

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

        if(pkt->get_longflow()) {
            pkt->set_maxhops(2);
        }
        else {
            pkt->set_maxhops(top->get_no_hops(pkt->get_src_ToR(),
                top->get_firstToR(pkt->get_dst()), slice, path_index));
        }
    }
}

simtime_picosec Routing::routing(Packet* pkt, simtime_picosec t, simtime_picosec init_time) {
    unsigned seqno = 0;
    if(pkt->type() == TCP) {
        seqno = ((TcpPacket*)pkt)->seqno();
    } else if (pkt->type() == TCPACK) {
        seqno = ((TcpAck*)pkt)->ackno();
    } else if (pkt->type() == NDP) {
        seqno = ((NdpPacket*)pkt)->seqno();
    }
    DynExpTopology* top = pkt->get_topology();
    int cur_slice = top->time_to_slice(t);

    if(pkt->get_crtToR() == top->get_firstToR(pkt->get_dst())){
        pkt->set_crtport(top->get_lastport(pkt->get_dst()));
        pkt->set_crtslice(0);
        return 0;
    }

    // using VLB for long flows
    if(pkt->get_longflow()) {
        // cannot use reconfig slices because packets will be stuck in that calendarq slice of only 10us
        if(cur_slice % 2 == 1) {
            return routing(pkt, top->get_slice_start_time(top->time_to_absolute_slice(t) + 1), init_time);
        }
        assert(pkt->get_crthop() == 1 || pkt->get_crthop() == 0);
        pair<int, int> route = top->get_direct_routing(pkt->get_crtToR(), 
                                                        top->get_firstToR(pkt->get_dst()), cur_slice);
        if(pkt->get_crthop() == 1 || route.second == cur_slice) {
            pkt->set_crtport(route.first);
            pkt->set_crtslice(route.second);

        } else {
            int randnum = top->get_rpath_indices(pkt->get_src(), pkt->get_dst(), top->time_to_slice(init_time));
            pkt->set_crtport(top->no_of_hpr() + randnum);
            pkt->set_crtslice(cur_slice);

        }
        Queue* cur_q = top->get_queue_tor(pkt->get_crtToR(), pkt->get_crtport());	
        simtime_picosec finish_push;	
        // to get accurate queueing delay, need to add from when the queue began servicing	
        if(cur_q->get_is_servicing() && top->time_to_slice(init_time) == cur_slice) { // only needed when init_slice == cur_slice	
            finish_push = cur_q->get_last_service_time() + cur_q->get_queueing_delay(cur_slice) +	
                cur_q->drainTime(pkt);	
        } else {	
            finish_push = t + cur_q->get_queueing_delay(pkt->get_crtslice()) +	
                cur_q->drainTime(pkt);	
        }	
        if(top->time_to_slice(finish_push) != pkt->get_crtslice()) { // if we cannot send out packet in original planned slice, route again
            // Does not follow VLB faithfully, but other options are drop packet/make it stay in calendarq -> which cause RTOs
            return routing(pkt, top->get_slice_start_time(top->time_to_absolute_slice(t) + 1), init_time);	
        }
        assert(pkt->get_crtslice() % 2 == 0);
        return 0;
    }

    if(pkt->get_crthop() == pkt->get_maxhops()) {
        cout<<"Packet exceeded max hops, routing to downlink\n";
        pkt->set_crtport(top->get_lastport(pkt->get_dst()));
        pkt->set_crtslice(0);
        return 1;
    }

    // next port assuming topology does not change
	int cur_slice_port = top->get_port(pkt->get_src_ToR(), top->get_firstToR(pkt->get_dst()),
                                pkt->get_slice_sent(), pkt->get_path_index(), pkt->get_crthop());

    // if this is the last ToR, need to get downlink port
        //FD uplink never seems to get changed so I just put this here
    Queue* cur_q = top->get_queue_tor(pkt->get_crtToR(), cur_slice_port);
    
    simtime_picosec finish_push;
    // to get accurate queueing delay, need to add from when the queue began servicing
    if(cur_q->get_is_servicing() && top->time_to_slice(init_time) == cur_slice) { // only needed when init_slice == cur_slice
        finish_push = cur_q->get_last_service_time() + cur_q->get_queueing_delay(cur_slice) +
            cur_q->drainTime(pkt);
    } else {
        finish_push = t + cur_q->get_queueing_delay(cur_slice) +
            cur_q->drainTime(pkt);
    }

    int finish_push_slice = top->time_to_slice(finish_push);
    simtime_picosec finish_time = finish_push + timeFromNs(delay_ToR2ToR);
    int finish_slice = top->time_to_slice(finish_time);
    
    #ifdef DEBUG
    cout <<seqno << "Routing crtToR: " << pkt->get_crtToR() << "Srctor:" <<pkt->get_src_ToR() << " dstToR: " << top->get_firstToR(pkt->get_dst()) << " slice: " << cur_slice 
            << " hop: " << pkt->get_crthop() << " port: "<<cur_slice_port<< " Now: " << t << " Last comp: "<< cur_q->get_last_service_time() << " Finish push:" << finish_push_slice << " " << finish_push << " Finish slice:"<<finish_slice<< " " << finish_time << endl;
    #endif

    if (finish_push_slice == cur_slice) {
        pkt->set_crtslice(cur_slice);
        pkt->set_crtport(cur_slice_port);
        return finish_time;
    } else {
        int next_absolute_slice = top->time_to_absolute_slice(t) + 1;
        simtime_picosec next_time = top->get_slice_start_time(next_absolute_slice);
        return routing(pkt, next_time, init_time);
    }
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
    //cout << "SETTING TO " << crt << endl;
    if(_queue->_sending_pkt == NULL && _queue->slice_queuesize(crt) > 0){
        // cout << "queue slice " << crt << " alarm beginService" << endl;
        _queue->beginService();
    }
    int next_absolute_slice = _top->time_to_absolute_slice(t) + 1;
    simtime_picosec next_time = _top->get_slice_start_time(next_absolute_slice);
    assert(next_time > t);
    eventlist().sourceIsPending(*this, next_time); 
}