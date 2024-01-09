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
// #define DEBUG

ECNQueue::ECNQueue(linkspeed_bps bitrate, mem_b maxsize, 
			 EventList& eventlist, QueueLogger* logger, mem_b  K,
             int tor, int port, DynExpTopology *top, Routing* routing)
    : Queue(bitrate,maxsize,eventlist,logger,tor,port,top,routing), 
      _K(K)
{
    _state_send = LosslessQueue::READY;
    _top = top;
    // TODO: change
    int slices = top->get_nlogicslices();
    _dl_queue = slices;
    _enqueued.resize(slices + 1);
    _queuesize.resize(slices + 1);
    std::fill(_queuesize.begin(), _queuesize.end(), 0);
}

void
ECNQueue::receivePacket(Packet & pkt)
{
    if(pkt.id() == 2979155) {
      cout << "DEBUG receivePacket " << _tor << " " << _port << " crt_slice " << _crt_tx_slice << " pktslice " << pkt.get_crtslice() << " hop " << pkt.get_hop_index() << endl;
  }
    if(pkt.id() == 2980467) {
      cout << "DEBUG EFB receivePacket " << _tor << " " << _port << " crt_slice " << _crt_tx_slice << " pktslice " << pkt.get_crtslice() << " hop " << pkt.get_hop_index() << endl;
  }

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
    #ifdef DEBUG
    cout<< "Queue " << _tor << "," << _port << "Slice received from PKT:"<<pkt.get_crtslice()<<" at " <<eventlist().now()<< "delay: " << get_queueing_delay(pkt.get_crtslice()) << endl;
    dump_queuesize();
    #endif
    // dump_queuesize();
    if (pkt.size() == UINT16_MAX) {
         if(pkt.type() == TCP){
            TcpPacket *tcppkt = (TcpPacket*)&pkt;
            tcppkt->get_tcpsrc()->add_to_dropped(tcppkt->seqno());
            cout << "DROPPED because expander routing failed\n";
            dump_queuesize();
        }
        pkt.free();
        _num_drops++;
        return;
    }

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

    //for early feedback, check for ECN at enqueue time
    if (queuesize() > _K){
      if(_early_fb_enabled && pkt.type() == TCP) {
        if(!pkt.early_fb()){
          sendEarlyFeedback(pkt);
        }
        pkt.set_early_fb();
        //cout << "early fb " << _tor << " " << _port << endl;
      }
    }

    /* enqueue the packet */
    bool queueWasEmpty = _enqueued[_crt_tx_slice].empty(); //is the current queue empty?
    _enqueued[pkt_slice].push_front(&pkt);
    _queuesize[pkt_slice] += pkt.size();
    pkt.inc_queueing(queuesize());

    //record queuesize per slice
    int slice = _top->time_to_logic_slice(eventlist().now());
    if (queuesize() > _max_recorded_size_slice[slice]) {
        _max_recorded_size_slice[slice] = queuesize();
    }
    if (queuesize() > _max_recorded_size) {
        _max_recorded_size = queuesize();
    }
    
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

    Packet* to_be_sent = _enqueued[_crt_tx_slice].back();
    DynExpTopology* top = to_be_sent->get_topology();
    
    simtime_picosec finish_push = eventlist().now() + drainTime(to_be_sent) /*229760*/;

    int finish_push_slice = top->time_to_logic_slice(finish_push); // plus the link delay
    if(top->is_reconfig(finish_push)) {
        finish_push_slice = top->absolute_logic_slice_to_slice(finish_push_slice + 1);
    }

    if(!top->is_downlink(_port) && finish_push_slice != _crt_tx_slice) {
        // Uplink port attempting to serve pkt across configurations
        #ifdef DEBUG
        cout<<"Uplink port attempting to serve pkt across configurations\n";
        #endif
        return;
    }

    eventlist().sourceIsPendingRel(*this, drainTime(_enqueued[_crt_tx_slice].back()));
    _is_servicing = true;
    _last_service_begin = eventlist().now();
    _sending_pkt = _enqueued[_crt_tx_slice].back();
    _enqueued[_crt_tx_slice].pop_back();

    //mark on dequeue
    if (queuesize() > _K){
      _sending_pkt->set_flags(_sending_pkt->flags() | ECN_CE);
    }

    //_queuesize[_crt_tx_slice] -= _sending_pkt->size();
    
    #ifdef DEBUG
    unsigned seqno = 0;
    if(_sending_pkt->type() == TCP) {
        seqno = ((TcpPacket*)_sending_pkt)->seqno();
    } else if (_sending_pkt->type() == TCPACK) {
        seqno = ((TcpAck*)_sending_pkt)->ackno();
    }
    cout << "Queue " << _tor << "," << _port << " beginService seq " << seqno << 
        " pktslice" << _sending_pkt->get_crtslice() << " tx slice " << _crt_tx_slice << " at " << eventlist().now() <<
    " src,dst " << _sending_pkt->get_src() << "," << _sending_pkt->get_dst() << endl;
    dump_queuesize();
    #endif
    
    
}


void
ECNQueue::completeService()
{
    /* dequeue the packet */
    assert(_sending_pkt);
    if(_crt_tx_slice != _sending_pkt->get_crtslice()) {
    cout << "sending_pkt " << _sending_pkt->get_crtslice() << " efb " << _sending_pkt->early_fb() << " tx_slice " << _crt_tx_slice << " port " << _port << endl; 
    cout << "crt_tor " << _sending_pkt->get_crtToR() << " crt_port " << _sending_pkt->get_crtport() << " dst_tor " << _top->get_firstToR(_sending_pkt->get_dst()) << endl;
  }
    assert(_crt_tx_slice == _sending_pkt->get_crtslice());

    unsigned seqno = 0;
    if(_sending_pkt->type() == TCP) {
        seqno = ((TcpPacket*)_sending_pkt)->seqno();
    } else if (_sending_pkt->type() == TCPACK) {
        seqno = ((TcpAck*)_sending_pkt)->ackno();
    }
    
    #ifdef DEBUG
    cout << "Queue " << _tor << "," << _port << " completeService seq " << seqno << 
        " pktslice" << _sending_pkt->get_crtslice() << " tx slice " << _crt_tx_slice << " at " << eventlist().now() <<
    " src,dst " << _sending_pkt->get_src() << "," << _sending_pkt->get_dst() << endl;
    dump_queuesize();
    #endif
    
    _queuesize[_crt_tx_slice] -= _sending_pkt->size();

    if(_sending_pkt->id() == 2979155) {
      cout << "DEBUG completeService " << _tor << " " << _port << " crt_slice " << _crt_tx_slice << endl; 
  }
    if(_sending_pkt->id() == 2980467) {
      cout << "DEBUG EFB completeService " << _tor << " " << _port << " crt_slice " << _crt_tx_slice << " pktslice " << _sending_pkt->get_crtslice() << " hop " << _sending_pkt->get_hop_index() << endl;
  }
    sendFromQueue(_sending_pkt);
    _sending_pkt = NULL;
    _is_servicing = false;
    
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

void
ECNQueue::sendEarlyFeedback(Packet &pkt) {
        //return the packet to the sender
        assert(pkt.type() == TCP);
        DynExpTopology* top = pkt.get_topology();

        TcpSrc* tcpsrc = NULL;
        unsigned seqno;
        if(pkt.type() == TCP) {
            tcpsrc = ((TcpPacket*)(&pkt))->get_tcpsrc();
            seqno = ((TcpPacket*)(&pkt))->seqno();
        } else {
            return;
        }
  /*
        assert(pkt.get_earlyhop() == pkt.get_crthop());
        if(pkt.id() == 68072) {
        cout << "SENDING " << pkt.get_earlyhop() << " " <<
            _tor << " " << _port << endl;
        }
  */
        assert(tcpsrc != NULL);
        TcpAck *ack = tcpsrc->alloc_tcp_ack();
        int old_src_ToR = _top->get_firstToR(pkt.get_src());
        // flip the source and dst of the packet: 
        int s = pkt.get_src();
        int d = pkt.get_dst();
        ack->set_src(d);
        ack->set_dst(s);
        ack->set_src_ToR(pkt.get_crtToR());
       _routing->routing_from_PQ(ack, eventlist().now());
        ack->set_ts(pkt.get_fabricts());
        ack->set_crtToR(pkt.get_crtToR());
        ack->set_early_fb();
        ack->set_lasthop(false);
        ack->set_flags(ECN_ECHO)  ;
        ack->set_hop_index(0);
        ack->set_crthop(0);
        if(pkt.id() == 2979155) {
          cout << "DEBUG Early feedback generated id " << ack->id() << endl;
        }
  

        // get the current ToR, this will be the new src_ToR of the packet
        int new_src_ToR = ack->get_crtToR();

        if (new_src_ToR == old_src_ToR) {
            // the packet got returned at the source ToR
            // we need to send on a downlink right away
            ack->set_crtport(top->get_lastport(ack->get_dst()));
            ack->set_maxhops(0);
            ack->set_crtslice(0);

            // debug:
            //cout << "   packet RTSed at the first ToR (ToR = " << new_src_ToR << ")" << endl;

        } else {
            ack->set_src_ToR(new_src_ToR);
            _routing->routing_from_ToR(ack, eventlist().now(), eventlist().now()); 
        }
        Queue* nextqueue = top->get_queue_tor(ack->get_crtToR(), ack->get_crtport());
        assert(nextqueue);
        nextqueue->receivePacket(*ack);
}
