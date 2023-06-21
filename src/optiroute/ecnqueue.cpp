// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-        
#include "ecnqueue.h"
#include <math.h>
#include "datacenter/dynexp_topology.h"
#include "ecn.h"
#include "tcp.h"
#include "dctcp.h"
#include "queue_lossless.h"
#include "tcppacket.h"
#include <iostream>

ECNQueue::ECNQueue(linkspeed_bps bitrate, mem_b maxsize, 
			 EventList& eventlist, QueueLogger* logger, mem_b  K,
             int tor, int port, DynExpTopology *top, Routing* routing)
    : Queue(bitrate,maxsize,eventlist,logger,tor,port,top,routing), 
      _K(K)
{
    _state_send = LosslessQueue::READY;
    _top = top;
    // TODO: change
    int slices = top->get_nslice()*2;
    _dl_queue = slices;
    _enqueued.resize(slices + 1);
    _queuesize.resize(slices + 1);
    std::fill(_queuesize.begin(), _queuesize.end(), 0);
}

void
ECNQueue::receivePacket(Packet & pkt)
{
    //is this a PAUSE packet?
    if (pkt.type()==ETH_PAUSE){
        EthPausePacket* p = (EthPausePacket*)&pkt;
        
        if (p->sleepTime()>0){
            //remote end is telling us to shut up.
            //assert(_state_send == LosslessQueue::READY);
            if (queuesize()>0)
            //we have a packet in flight
            _state_send = LosslessQueue::PAUSE_RECEIVED;
            else
            _state_send = LosslessQueue::PAUSED;
            
            //cout << timeAsMs(eventlist().now()) << " " << _name << " PAUSED "<<endl;
        }
        else {
            //we are allowed to send!
            _state_send = LosslessQueue::READY;
            //cout << timeAsMs(eventlist().now()) << " " << _name << " GO "<<endl;
            
            //start transmission if we have packets to send!
            if(queuesize()>0)
            beginService();
        }
        
        pkt.free();
        return;
    }

    int pkt_slice = _top->is_downlink(_port) ? 0 : pkt.get_crtslice();
    //cout<< "Queue " << _tor << "," << _port << "Slice received from PKT:"<<pkt.get_crtslice()<<" at " <<eventlist().now()<< "delay: " << get_queueing_delay(pkt.get_crtslice()) << endl;
    // dump_queuesize();
    if (queuesize() + pkt.size() > _maxsize) {
        /* if the packet doesn't fit in the queue, drop it */
        /*
           if (_logger) 
           _logger->logQueue(*this, QueueLogger::PKT_DROP, pkt);
           pkt.flow().logTraffic(pkt, *this, TrafficLogger::PKT_DROP);
           */
        if(pkt.type() == TCP){
            TcpPacket *tcppkt = (TcpPacket*)&pkt;
            tcppkt->get_tcpsrc()->add_to_dropped(tcppkt->seqno());
            cout<<"Current slice:"<< _crt_tx_slice<<" Slice received from PKT:"<<pkt.get_crtslice()<<" at " <<eventlist().now()<< "\n";
            cout << "DROPPED because calendarqueue full\n";
            dump_queuesize();
        }
        pkt.free();
        _num_drops++;
        return;
    }
    //pkt.flow().logTraffic(pkt, *this, TrafficLogger::PKT_ARRIVE);

    //mark on enqueue
    //    if (_queuesize > _K)
    //  pkt.set_flags(pkt.flags() | ECN_CE);

    /* enqueue the packet */
    bool queueWasEmpty = _enqueued[_crt_tx_slice].empty(); //is the current queue empty?
    _enqueued[pkt_slice].push_front(&pkt);
    _queuesize[pkt_slice] += pkt.size();
    pkt.inc_queueing(queuesize());
    //dump_queuesize();

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

    //_queuesize[_crt_tx_slice] -= _sending_pkt->size();
    Packet *pkt = _sending_pkt;
    unsigned seqno = 0;
    if(pkt->type() == TCP) {
        seqno = ((TcpPacket*)pkt)->seqno();
    } else if (pkt->type() == TCPACK) {
        seqno = ((TcpAck*)pkt)->ackno();
    }
    
    // cout << "Queue " << _tor << "," << _port << " beginService seq " << seqno << 
    //  " pktslice" << pkt->get_crtslice() << " tx slice " << _crt_tx_slice << " at " << eventlist().now() << "delay: " << get_queueing_delay(pkt->get_crtslice()) << endl;
    
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
    _queuesize[_crt_tx_slice] -= _sending_pkt->size();
    int pkt_slice = pkt->get_crtslice();

    // cout << "Queue " << _tor << "," << _port << " completeService seq " << seqno << 
    //     " pktslice" << pkt_slice << " tx slice " << _crt_tx_slice << " at " << eventlist().now() <<
	// " src,dst " << pkt->get_src() << "," << pkt->get_dst() << endl;
    // dump_queuesize();

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
    for(int i = 0; i < _dl_queue; i++){
        sum += _queuesize[i];
    }
    return sum;
}

void ECNQueue::dump_queuesize() {
    cout<<nodename()<<":  ";
    for(int i = 0; i < _dl_queue; i++){
        cout<<i<<":"<<_queuesize[i]<<" ";
    }
    cout<<"\n";
}
