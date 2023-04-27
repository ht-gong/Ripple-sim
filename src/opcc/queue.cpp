// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-
#include <sstream>
#include <math.h>
#include <climits>
#include "datacenter/dynexp_topology.h"
#include "queue.h"
#include "tcppacket.h"
#include "ndppacket.h"
#include "tcp.h"
#include "ndp.h"
#include "queue_lossless.h"

#include "pipe.h"

Queue::Queue(linkspeed_bps bitrate, mem_b maxsize, EventList& eventlist, QueueLogger* logger)
  : EventSource(eventlist,"queue"),
    _logger(logger), _bitrate(bitrate), _num_drops(0)
{
    Queue(bitrate, maxsize, eventlist, logger, 0, 0, NULL);
}

Queue::Queue(linkspeed_bps bitrate, mem_b maxsize, EventList& eventlist, QueueLogger* logger, int tor, int port,
        DynExpTopology *top)
  : EventSource(eventlist,"queue"), _maxsize(maxsize), _tor(tor), _port(port), _top(top),
    _logger(logger), _bitrate(bitrate), _num_drops(0)
{
    _ps_per_byte = (simtime_picosec)((pow(10.0, 12.0) * 8) / _bitrate);
    int slices = top ? top->get_nsuperslice()*2 : 1;
    //the _dl_queueth element of the vectors is the downlink implementation (so a normal queue)
    _dl_queue = slices;
    stringstream ss;
    _routing = new HohoRouting();
    _queue_alarm = new QueueAlarm(eventlist, port, this, top);
    //ss << "queue(" << bitrate/1000000 << "Mb/s," << maxsize << "bytes)";
    //_nodename = ss.str();
}


void Queue::beginService() {
    /* schedule the next dequeue event */
    assert(!_enqueued.empty());
    eventlist().sourceIsPendingRel(*this, drainTime(_enqueued.back()));
    _sending_pkt = _enqueued.back();
}

void Queue::completeService() {
    /* dequeue the packet */
    assert(!_enqueued.empty());
    Packet* pkt = _enqueued.back();
    _enqueued.pop_back();
    _queuesize -= pkt->size();

    /* tell the packet to move on to the next pipe */
    sendFromQueue(pkt);

    if (!_enqueued.empty()) {
        /* schedule the next dequeue event */
        beginService();
    }
}

void Queue::sendFromQueue(Packet* pkt) {
    Pipe* nextpipe; // the next packet sink will be a pipe
    DynExpTopology* top = pkt->get_topology();
    if (pkt->get_crthop() < 0) {
        // we're sending out of the NIC
        // debug:
        //cout << "Sending out of the NIC onto pipe " << pkt->get_src() << endl;

        nextpipe = top->get_pipe_serv_tor(pkt->get_src());
        nextpipe->receivePacket(*pkt);
    } else {
        // we're sending out of a ToR queue
        if (top->is_last_hop(pkt->get_crtport())) {
            pkt->set_lasthop(true);
            // if this port is not connected to _dst, then drop the packet
            if (!top->port_dst_match(pkt->get_crtport(), pkt->get_crtToR(), pkt->get_dst())) {

                switch (pkt->type()) {
                    case NDP:
                        cout << "!!! NDP";
                        break;
                    case NDPACK:
                        cout << "!!! NDPACK";
                        break;
                    case NDPNACK:
                        cout << "!!! NDPNACK";
                        break;
                    case NDPPULL:
                        cout << "!!! NDPPULL";
                        break;
                    case TCP:
                        cout << "!!! TCP";
                        TcpPacket *tcppkt = (TcpPacket*)&pkt;
                        tcppkt->get_tcpsrc()->add_to_dropped(tcppkt->seqno());
                }
                cout << " packet dropped: port & dst didn't match! (queue.cpp)" << endl;
                cout << "    ToR = " << pkt->get_crtToR() << ", port = " << pkt->get_crtport() <<
                    ", src = " << pkt->get_src() << ", dst = " << pkt->get_dst() << endl;
                cout << "    FAILED " << pkt->get_ndpsrc()->get_flowsize() << " bytes" << endl; 

                pkt->free(); // drop the packet

                return;
            }
        }
        nextpipe = top->get_pipe_tor(pkt->get_crtToR(), pkt->get_crtport());
        nextpipe->receivePacket(*pkt);
    }
}

void Queue::doNextEvent() {
    completeService();
}


void Queue::receivePacket(Packet& pkt) {
    if (queuesize()+pkt.size() > _maxsize) {
	/* if the packet doesn't fit in the queue, drop it */
	if (_logger)
	    _logger->logQueue(*this, QueueLogger::PKT_DROP, pkt);
	pkt.flow().logTraffic(pkt, *this, TrafficLogger::PKT_DROP);
	pkt.free();
    cout << "!!! Packet dropped: queue overflow!" << endl;
	_num_drops++;
	return;
    }
    pkt.flow().logTraffic(pkt, *this, TrafficLogger::PKT_ARRIVE);

    /* enqueue the packet */
    bool queueWasEmpty = _enqueued.empty();
    _enqueued.push_front(&pkt);
    _queuesize += pkt.size();
    if (_logger) _logger->logQueue(*this, QueueLogger::PKT_ENQUEUE, pkt);

    if (queueWasEmpty) {
	/* schedule the dequeue event */
	assert(_enqueued.size() == 1);
	beginService();
    }
}

simtime_picosec Queue::serviceTime() {
    return queuesize() * _ps_per_byte;
}

int Queue::next_tx_slice(int crt_slice){
    assert(crt_slice != _dl_queue); 
    int now =_top->time_to_slice(eventlist().now());
    if(crt_slice == now)
        return now;
    int i = (crt_slice+1)%(_top->get_nsuperslice()*2);
    //look for next slice queue that has to send
    while (i != now){
        if (slice_queuesize(i) > 0)
            return i;
        i = (i+1)%(_top->get_nsuperslice()*2);
    }
    return now;
}

simtime_picosec Queue::get_queueing_delay(int slice){
    /*
    assert(slice != _dl_queue);
    int i = _crt_tx_slice;
    mem_b bytes = 0;
    while (i != slice){
        bytes += slice_queuesize(i); 
        i = (i+1)%(_top->get_nsuperslice()*2);
    }
    return (bytes+slice_queuesize(slice))*_ps_per_byte;
    */
    return slice_queuesize(slice)*_ps_per_byte;
}

mem_b Queue::slice_queuesize(int slice){
    assert(slice <= _dl_queue);
    return _queuesize;
}

mem_b Queue::queuesize() {
    return _queuesize;    
}
/*
mem_b Queue::queuesize() {
    if(_top->is_downlink(_port))
        return _queuesize[_dl_queue];
    int sum = 0;
    int i;
    for(i = 0; i < _top->get_nsuperslice()*2; i++){
        sum += _queuesize[i];
    }
    return sum;
}
*/
//////////////////////////////////////////////////
//              Priority Queue                  //
//////////////////////////////////////////////////

PriorityQueue::PriorityQueue(DynExpTopology* top, linkspeed_bps bitrate, mem_b maxsize,
			     EventList& eventlist, QueueLogger* logger, int node)
    : Queue(bitrate, maxsize, eventlist, logger)
{
    _top = top;
    _node = node;

    _bytes_sent = 0;

    _queuesize[Q_RLB] = 0;
    _queuesize[Q_LO] = 0;
    _queuesize[Q_MID] = 0;
    _queuesize[Q_HI] = 0;
    _servicing = Q_NONE;
    //_state_send = LosslessQueue::READY;
    //TODO Why do I need to redefine this for it to not be 0????
    _ps_per_byte = (simtime_picosec)((pow(10.0, 12.0) * 8) / _bitrate);
}

PriorityQueue::queue_priority_t PriorityQueue::getPriority(Packet& pkt) {
    queue_priority_t prio = Q_LO;
    switch (pkt.type()) {
    case TCPACK:
    case NDPACK:
    case NDPNACK:
    case NDPPULL:
    case NDPLITEACK:
    case NDPLITERTS:
    case NDPLITEPULL:
        prio = Q_HI;
        break;
    case NDP:
        if (pkt.header_only()) {
            if (pkt.get_longflow()) {
                prio = _q_hi;
            } else {
                prio = Q_HI;
            }
        } else {
            NdpPacket* np = (NdpPacket*)(&pkt);
            if (np->retransmitted()) {
                if (pkt.get_longflow()) {
                    prio = _q_mid;
                } else {
                    prio = Q_MID;
                }
            } else {
                if (pkt.get_longflow()) {
                    prio = _q_lo;
                } else {
                    prio = Q_LO;
                }
            }
        }
        break;
    case TCP:
    case IP:
    case NDPLITE:
        prio = Q_LO;
        break;
    default:
        cout << "NIC couldn't identify packet type." << endl;
        abort();
    }

    return prio;
}

simtime_picosec PriorityQueue::serviceTime(Packet& pkt) {
    queue_priority_t prio = getPriority(pkt);
    switch (prio) {
    case Q_LO:
	   //cout << "q_lo: " << _queuesize[Q_HI] + _queuesize[Q_MID] + _queuesize[Q_LO] << " ";
	   return (_queuesize[Q_HI] + _queuesize[Q_MID] + _queuesize[Q_LO]) * _ps_per_byte;
    case Q_MID:
	   //cout << "q_mid: " << _queuesize[Q_MID] + _queuesize[Q_LO] << " ";
	   return (_queuesize[Q_HI] + _queuesize[Q_MID]) * _ps_per_byte;
    case Q_HI:
	   //cout << "q_hi: " << _queuesize[Q_LO] << " ";
	   return _queuesize[Q_HI] * _ps_per_byte;
    case Q_RLB:
      //abort(); // we should never check this for an RLB packet
      // YX: Modified because long flows may have low priority.
      return (_queuesize[Q_HI] + _queuesize[Q_MID] + _queuesize[Q_LO] + _queuesize[Q_RLB]) * _ps_per_byte;

    default:
	   abort();
    }
}

void PriorityQueue::receivePacket(Packet& pkt) {

    queue_priority_t prio = getPriority(pkt);

    // debug:
    // cout << "   NIC received a packet." << endl;
    // cout << "long flow? " << pkt.get_longflow() << endl;

    //pkt.flow().logTraffic(pkt, *this, TrafficLogger::PKT_ARRIVE);

    /* enqueue the packet */
    bool queueWasEmpty = queuesize() == 0;

    _queuesize[prio] += pkt.size();
    _queue[prio].push_front(&pkt);

    //if (_logger)
    //    _logger->logQueue(*this, QueueLogger::PKT_ENQUEUE, pkt);

    if (queueWasEmpty) { // && _state_send==LosslessQueue::READY) {
        /* schedule the dequeue event */
        assert(_queue[Q_RLB].size() + _queue[Q_LO].size() + _queue[Q_MID].size() + _queue[Q_HI].size() == 1);
        beginService();
    }
}

void PriorityQueue::beginService() {
    //assert(_state_send == LosslessQueue::READY);

    /* schedule the next dequeue event */
    for (int prio = Q_HI; prio >= Q_RLB; --prio) {
        if (_queuesize[prio] > 0) {
            eventlist().sourceIsPendingRel(*this, drainTime(_queue[prio].back()));
            /*
            cout << "PrioQueue (node " << _node << ") sending a packet of size " << 
                _queue[prio].back()->size() << " at " << timeAsUs(eventlist().now()) << " us" << endl;
            cout << "   will be drained in " << timeAsUs(drainTime(_queue[prio].back())) << " us" << endl;
            */

            _servicing = (queue_priority_t)prio;
            return;
        }
    }
}

void PriorityQueue::completeService() {


	// the below doesn't work because RLB can shut down the queue in between `beginService` and `completeService`
    //assert(!_queue[_servicing].empty());
    //assert(_servicing != Q_NONE);

	if (!_queue[_servicing].empty()) {

	    Packet* pkt;

	    switch (_servicing) {
	    case Q_RLB:
	    case Q_LO:
	    case Q_MID:
	    case Q_HI:
	    {
	        pkt = _queue[_servicing].back(); // get the pointer to the packet
	        _queue[_servicing].pop_back(); // delete the element of the queue
	        _queuesize[_servicing] -= pkt->size(); // decrement the queue size

	        int new_bytes_sent = _bytes_sent + pkt->size();
	        _bytes_sent = new_bytes_sent;

	        // set the routing info

            pkt->set_src_ToR(_top->get_firstToR(pkt->get_src())); // set the sending ToR. This is used for subsequent routing

	        if (pkt->get_src_ToR() == _top->get_firstToR(pkt->get_dst())) {
	            // the packet is being sent within the same rack
	            pkt->set_lasthop(false);
	            pkt->set_crthop(-1);
	            pkt->set_crtToR(-1);
	            pkt->set_maxhops(0); // want to select a downlink port immediately
	        } else {
	            // the packet is being sent between racks
              int slice = _top->time_to_slice(eventlist().now());

	            // get the number of available paths for this packet during this slice
	            int npaths = _top->get_no_paths(pkt->get_src_ToR(),
	                _top->get_firstToR(pkt->get_dst()), slice);

	            if (npaths == 0)
	                cout << "Error: there were no paths!" << endl;
	            assert(npaths > 0);

	            // randomly choose a path for the packet
	            // !!! todo: add other options like permutation, etc...
	            int path_index = random() % npaths;

	            pkt->set_slice_sent(slice); // "timestamp" the packet
	            pkt->set_path_index(path_index); // set which path the packet will take

	            // set some initial packet parameters used for label switching
	            // *this could also be done in NDP before the packet is sent to the NIC
	            pkt->set_lasthop(false);
	            pkt->set_crthop(-1);
	            pkt->set_crtToR(-1);
	            // pkt->set_maxhops(_top->get_no_hops(pkt->get_src_ToR(),
	            //     _top->get_firstToR(pkt->get_dst()), slice, path_index));
              // YX: no need to set a limit to # max hops, need double check.
              pkt->set_maxhops(INT_MAX);
	        }

            if(pkt->type() == TCP) {
                TcpPacket *tcp_pkt = (TcpPacket*)pkt;
                //cout << "NIC sending " << tcp_pkt->seqno() << " at " << eventlist().now() << endl;
            }
	        /* tell the packet to move on to the next pipe */
            pkt->set_fabricts(eventlist().now());
	        sendFromQueue(pkt);

	        break;
	    }
	    case Q_NONE:
	    	break;
	        //abort();
	    }
	}

    if (queuesize() > 0) {
        beginService();
    } else {

        // debug:
        //cout << "NIC stopped sending" << endl;

        _servicing = Q_NONE;
    }
}

mem_b PriorityQueue::queuesize() {
    return _queuesize[Q_RLB] + _queuesize[Q_LO] + _queuesize[Q_MID] + _queuesize[Q_HI];
}
