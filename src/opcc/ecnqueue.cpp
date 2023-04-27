// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-        
#include "ecnqueue.h"
#include <math.h>
#include "datacenter/dynexp_topology.h"
#include "ecn.h"
#include "network.h"
#include "tcp.h"
#include "dctcp.h"
#include "queue_lossless.h"
#include "tcppacket.h"
#include <iostream>

ECNQueue::ECNQueue(linkspeed_bps bitrate, mem_b maxsize, 
			 EventList& eventlist, QueueLogger* logger, mem_b  K,
             int tor, int port, DynExpTopology *top)
    : Queue(bitrate,maxsize,eventlist,logger,tor,port,top), 
      _K(K)
{
    _state_send = LosslessQueue::READY;
    _top = top;
    int slices = top->get_nsuperslice()*2;
    _dl_queue = slices;
    _enqueued.resize(slices+1);
    _queuesize.resize(slices+1);
    std::fill(_queuesize.begin(), _queuesize.end(), 0);
}


void
ECNQueue::receivePacket(Packet & pkt)
{
    unsigned seqno = 0;
    if(pkt.type() == TCP) {
        seqno = ((TcpPacket*)&pkt)->seqno();
    } else if (pkt.type() == TCPACK) {
        seqno = ((TcpAck*)&pkt)->ackno();
    }
    //downlink ports only have one queue
    int pkt_slice = _top->is_downlink(_port) ? 0 : pkt.get_crtslice();
/*
    cout << "Queue " << _tor << "," << _port << " receivePacket seq " << seqno << 
        " pktslice " << pkt_slice << " tx slice " << _crt_tx_slice << " at " << eventlist().now() <<
	" src,dst " << pkt.get_src() << "," << pkt.get_dst() << endl;
*/

    if (queuesize()+pkt.size() > _maxsize) {
        /* if the packet doesn't fit in the queue, drop it */
        if(pkt.type() == TCP){
            TcpPacket *tcppkt = (TcpPacket*)&pkt;
            tcppkt->get_tcpsrc()->add_to_dropped(tcppkt->seqno());
        }
        cout << "DROPPED AT QUEUE" << endl;
        pkt.free();
        _num_drops++;
        return;
    }
    //cout << "Queuesize " << _tor << "," << _port << " " << queuesize() << " " << slice_queuesize(pkt_slice) << endl;

    /* enqueue the packet */
    //cout << _crt_tx_slice << " " << _top->get_nsuperslice()*2 << endl;
    bool queueWasEmpty = _enqueued[_crt_tx_slice].empty(); //is the current queue empty?
    _enqueued[pkt_slice].push_front(&pkt);
    _queuesize[pkt_slice] += pkt.size();
    pkt.inc_queueing(queuesize());

    if (queueWasEmpty && !_sending_pkt && pkt_slice == _crt_tx_slice) {
	/* schedule the dequeue event */
	assert(_enqueued[_crt_tx_slice].size() == 1);
	    beginService();
    } 
}

void ECNQueue::beginService() {
    /* schedule the next dequeue event */
    assert(!_enqueued[_crt_tx_slice].empty());
    assert(drainTime(_enqueued[_crt_tx_slice].back()) != 0);
    eventlist().sourceIsPendingRel(*this, drainTime(_enqueued[_crt_tx_slice].back()));
    _sending_pkt = _enqueued[_crt_tx_slice].back();
    _enqueued[_crt_tx_slice].pop_back();

    //mark on dequeue
    if (queuesize() > _K)
	  _sending_pkt->set_flags(_sending_pkt->flags() | ECN_CE);

    _queuesize[_crt_tx_slice] -= _sending_pkt->size();
    Packet *pkt = _sending_pkt;
    unsigned seqno = 0;
    if(pkt->type() == TCP) {
        seqno = ((TcpPacket*)pkt)->seqno();
    } else if (pkt->type() == TCPACK) {
        seqno = ((TcpAck*)pkt)->ackno();
    }
    int pkt_slice = pkt->get_crtslice();
    /*
    cout << "Queue " << _tor << "," << _port << " beginService seq " << seqno << 
        " pktslice" << pkt_slice << " tx slice " << _crt_tx_slice << " at " << eventlist().now() << endl;
    */
}

void
ECNQueue::completeService()
{
    /* dequeue the packet */
    assert(_sending_pkt);

    Packet *pkt = _sending_pkt;
    unsigned seqno = 0;
    if(pkt->type() == TCP) {
        seqno = ((TcpPacket*)pkt)->seqno();
    } else if (pkt->type() == TCPACK) {
        seqno = ((TcpAck*)pkt)->ackno();
    }
    int pkt_slice = pkt->get_crtslice();
/*
    cout << "Queue " << _tor << "," << _port << " completeService seq " << seqno << 
        " pktslice" << pkt_slice << " tx slice " << _crt_tx_slice << " at " << eventlist().now() <<
	" src,dst " << pkt->get_src() << "," << pkt->get_dst() << endl;
*/

    sendFromQueue(_sending_pkt);
    _sending_pkt = NULL;

    if (!_enqueued[_crt_tx_slice].empty() && _state_send==LosslessQueue::READY) {
        /* schedule the next dequeue event */
        beginService();
    }
}

mem_b ECNQueue::slice_queuesize(int slice){
    assert(slice <= _dl_queue);
    return _queuesize[slice];
}

mem_b ECNQueue::queuesize() {
    if(_top->is_downlink(_port))
        return _queuesize[0];
    int sum = 0;
    int i;
    for(i = 0; i < _top->get_nsuperslice()*2; i++){
        sum += _queuesize[i];
    }
    return sum;
}
