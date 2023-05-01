// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-
#include "hoho_utils.h"
#include <math.h>

#include <climits>
#include <iostream>
#include <sstream>
#include <algorithm>

#include "config.h"
#include "dynexp_topology.h"
#include "tcppacket.h"
#include "ndp.h"

extern uint32_t delay_host2ToR; // nanoseconds, host-to-tor link
extern uint32_t delay_ToR2ToR; // nanoseconds, tor-to-tor link

simtime_picosec HohoRouting::routing(Packet* pkt, simtime_picosec t) {
    unsigned seqno = 0;
    if(pkt->type() == TCP) {
        seqno = ((TcpPacket*)pkt)->seqno();
    } else if (pkt->type() == TCPACK) {
        seqno = ((TcpAck*)pkt)->ackno();
    }
	DynExpTopology* top = pkt->get_topology();
	int slice = top->time_to_slice(t);

	pair<int, int> route;
	if (pkt->get_longflow()) {
		// YX: get_crtToR() should give you the first ToR only, please test
        //cout << "direct routing between " << pkt->get_crtToR() << " & " << top->get_firstToR(pkt->get_dst()) << endl;
        assert(0);
		route = top->get_direct_routing(pkt->get_crtToR(), top->get_firstToR(pkt->get_dst()), slice);
	} else {
		route = top->get_routing(pkt->get_crtToR(), top->get_firstToR(pkt->get_dst()), slice);
	}
    //cout << "Routing crtToR: " << pkt->get_crtToR() << " firstToR: " << top->get_firstToR(pkt->get_dst()) << endl;
    
	int uplink = route.first;
	int sent_slice = route.second;

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
    } else if (sent_slice != slice) {
        int next_absolute_slice = top->time_to_absolute_slice(t) + 1;
        simtime_picosec next_time = top->get_slice_start_time(next_absolute_slice);
        return routing(pkt, next_time);
    } else {
        //FD uplink never seems to get changed so I just put this here
	    pkt->set_crtport(uplink);
    }
    pkt->set_crtslice(sent_slice);

    Queue* q = top->get_queue_tor(pkt->get_crtToR(), uplink);

    //calculate delay considering the queue occupancy
    int finish_slice = top->time_to_slice(t + q->get_queueing_delay(sent_slice) +
            q->drainTime(pkt) /*229760*/ + timeFromNs(delay_ToR2ToR)); // plus the link delay

    if (finish_slice == sent_slice) {
        /*
        cout << "Routing finish_slice==slice slice=" << slice << " t=" << 
            (t + q->get_queueing_delay(sent_slice) + q->drainTime(pkt)) + timeFromNs(delay_ToR2ToR) << endl;
        cout << "qdelay: " << q->get_queueing_delay(sent_slice) << " draintime: " 
            << q->drainTime(pkt) << " slice " << slice << " sent_slice: " <<  sent_slice << " seqno: " << seqno << endl;
        */
        return (t + q->get_queueing_delay(sent_slice) + q->drainTime(pkt));
    } else {
        /*
        cout << "Rerouting: " <<  (q->get_queueing_delay(sent_slice) + q->drainTime(pkt) + timeFromNs(delay_ToR2ToR)) <<
            " delay t="  << (t + q->get_queueing_delay(sent_slice) + q->drainTime(pkt) + timeFromNs(delay_ToR2ToR)) << endl;
        cout << "qdelay: " << q->get_queueing_delay(sent_slice) << " draintime: " 
            << q->drainTime(pkt) << " slice " << slice << " sent_slice: " << sent_slice << " seqno: " << seqno << endl;
        */
        int next_absolute_slice = top->time_to_absolute_slice(t) + 1;
        simtime_picosec next_time = top->get_slice_start_time(next_absolute_slice);
        return routing(pkt, next_time);
    }
}
#if 0
		// May wrap around the cycle
		int slice_delta = (finish_slice - slice + top->get_nsuperslice() * 2)
			% (top->get_nsuperslice() * 2);

        if (slice_delta == 1) {
            //cout << "Routing slice_delta == 1" << endl;
            if (top->get_nextToR(slice, pkt->get_crtToR(), uplink) ==
                    top->get_nextToR(finish_slice, pkt->get_crtToR(), uplink)) {
                //cout << "Routing same ToR" << endl;
                int next_slice = (pkt->get_crtslice()+1)%(top->get_nsuperslice()*2);
                pkt->set_crtslice(next_slice);
                return (t + q->get_queueing_delay(slice) + q->drainTime(pkt));
            }
            // Miss this slice, have to wait till the next slice
            int next_absolute_slice = top->time_to_absolute_slice(t) + 1;
            simtime_picosec next_time = top->get_slice_start_time(next_absolute_slice);
            return routing(pkt, next_time);
        }

        /*
        assert(slice_delta == 2); // Cannot be more than 2!

        int middle_slice = (slice + 1) % (top->get_nsuperslice() * 2);

		int next_tor1 = top->get_nextToR(slice, pkt->get_crtToR(), uplink);
		int next_tor2 = top->get_nextToR(middle_slice, pkt->get_crtToR(), uplink);
		int next_tor3 = top->get_nextToR(finish_slice, pkt->get_crtToR(), uplink);

		if ((next_tor1 == next_tor2) && (next_tor2 == next_tor3)) {
			return (t + q->drainTime(pkt));
		}
        */
		// Miss this slice, have to wait till the next slice
		int next_absolute_slice = top->time_to_absolute_slice(t) + 1;
		simtime_picosec next_time = top->get_slice_start_time(next_absolute_slice);
		return routing(pkt, next_time);
	} else {
        /*
        cout << "Routing sent_slice!=slice hop " << pkt->get_crthop() << " crtToR " << 
            pkt->get_crtToR() << " port " << pkt->get_crtport() << " seqno " << seqno << endl;
            */
		// Send at the beginning of the sent_slice
		int wait_slice = (sent_slice - slice + top->get_nsuperslice() * 2)
			% (top->get_nsuperslice() * 2);
		int sent_absolute_slice = top->time_to_absolute_slice(t) + wait_slice;
        return (top->get_slice_start_time(sent_absolute_slice) +
            q->drainTime(pkt));
	}
}
#endif

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
    if(_queue->_tor == 94 && _queue->_port == 10) {
        cout << "slice change at " << eventlist().now() << endl;
    }
    assert(_queue->_crt_tx_slice != crt);
    _queue->_crt_tx_slice = crt;
    //cout << "SETTING TO " << crt << endl;
    if(_queue->_sending_pkt == NULL && _queue->slice_queuesize(crt) > 0){
        //cout << "queue slice " << crt << " alarm beginService" << endl;
        _queue->beginService();
    }
    int next_absolute_slice = _top->time_to_absolute_slice(t) + 1;
    simtime_picosec next_time = _top->get_slice_start_time(next_absolute_slice);
    assert(next_time > t);
    eventlist().sourceIsPending(*this, next_time); 
}
