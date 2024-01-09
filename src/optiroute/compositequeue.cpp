// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-        
#include "compositequeue.h"
#include <math.h>

#include <iostream>
#include <sstream>

#include "dynexp_topology.h"
#include "rlbpacket.h" // added for debugging
#include "rlbmodule.h"
#include "ndppacket.h"

// !!! NOTE: this one does selective RLB packet dropping.

// !!! NOTE: this has been modified to also include a lower priority RLB queue

CompositeQueue::CompositeQueue(linkspeed_bps bitrate, mem_b maxsize, EventList& eventlist, 
			       QueueLogger* logger, int tor, int port, DynExpTopology *top, Routing* routing)
  : Queue(bitrate, maxsize, eventlist, logger, tor, port, top, routing)
{
  _tor = tor;
  _port = port;
  // original version:
  //_ratio_high = 10; // number of headers to send per data packet (originally 24 for Jan '18 version)
  //_ratio_low = 1; // number of full packets
  // new version:
  _ratio_high = 640; // bytes (640 = 10 64B headers)
  _ratio_low = 1500; // bytes (1500 = 1 1500B packet)
  _crt = 0;
  _num_headers = 0;
  _num_packets = 0;
  _num_acks = 0;
  _num_nacks = 0;
  _num_pulls = 0;
  _num_drops = 0;
  _num_stripped = 0;
  _num_bounced = 0;

  int slices = top->get_nlogicslices();
  _enqueued_high.resize(slices + 1);
  _enqueued_low.resize(slices + 1);
  _enqueued_rlb.resize(slices + 1);
  _queuesize_high.resize(slices + 1);
  _queuesize_low.resize(slices + 1);
  _queuesize_rlb.resize(slices + 1);
  std::fill(_queuesize_high.begin(), _queuesize_high.end(), 0);
  std::fill(_queuesize_low.begin(), _queuesize_low.end(), 0);
  std::fill(_queuesize_rlb.begin(), _queuesize_rlb.end(), 0);
  _serv = QUEUE_INVALID;

}

bool CompositeQueue::canBeginService(Packet* to_be_sent) {
  simtime_picosec finish_push = eventlist().now() + drainTime(to_be_sent) /*229760*/;

  int finish_push_slice = _top->time_to_logic_slice(finish_push); // plus the link delay
  if(_top->is_reconfig(finish_push)) {
    finish_push_slice = _top->absolute_logic_slice_to_slice(finish_push_slice + 1);
  }

  if(!_top->is_downlink(_port) && finish_push_slice != _crt_tx_slice) {
    // Uplink port attempting to serve pkt across configurations
#ifdef DEBUG
    cout<<"Uplink port attempting to serve pkt across configurations\n";
#endif
    cout<<"Uplink port attempting to serve pkt across configurations\n";
    return false;
  } else {
    return true;
  }
}

//this code is cursed
void CompositeQueue::returnToSender(Packet *pkt) {
  cout << "RTS\n";
  //return the packet to the sender
  //if (_logger) _logger->logQueue(*this, QueueLogger::PKT_BOUNCE, *pkt);
  //pkt->flow().logTraffic(pkt,*this,TrafficLogger::PKT_BOUNCE);

  // debug:
  //cout << "   ... returning to sender." << endl;

  DynExpTopology* top = pkt->get_topology();
  pkt->bounce(); // indicate that the packet has been bounced
  _num_bounced++;

  int old_src_ToR = _top->get_firstToR(pkt->get_src());
  // flip the source and dst of the packet:
  int s = pkt->get_src();
  int d = pkt->get_dst();
  pkt->set_src(d);
  pkt->set_dst(s);
  assert(pkt->get_crtToR() >= 0);
  pkt->set_src_ToR(pkt->get_crtToR());
  _routing->routing_from_PQ(pkt, eventlist().now());
  pkt->set_crtToR(pkt->get_src_ToR());
  pkt->set_lasthop(false);
  pkt->set_hop_index(0);
  pkt->set_crthop(0);

  // get the current ToR, this will be the new src_ToR of the packet
  int new_src_ToR = pkt->get_crtToR();

  if (new_src_ToR == old_src_ToR) {
    // the packet got returned at the source ToR
    // we need to send on a downlink right away
    pkt->set_crtport(top->get_lastport(pkt->get_dst()));
    pkt->set_maxhops(0);
    pkt->set_crtslice(0);

    // debug:
    //cout << "   packet RTSed at the first ToR (ToR = " << new_src_ToR << ")" << endl;

  } else {
    pkt->set_src_ToR(new_src_ToR);
    _routing->routing_from_ToR(pkt, eventlist().now(), eventlist().now()); 
  }
  // debug:
  //cout << "   packet RTSed to node " << pkt->get_dst() << " at ToR = " << new_src_ToR << endl;

  Queue* nextqueue = top->get_queue_tor(pkt->get_crtToR(), pkt->get_crtport());
  nextqueue->receivePacket(*pkt);
}

void CompositeQueue::beginService(){
	if ( !_enqueued_high[_crt_tx_slice].empty() && !_enqueued_low[_crt_tx_slice].empty() ){

		if (_crt >= (_ratio_high+_ratio_low))
			_crt = 0;

		if (_crt< _ratio_high) {
      if(canBeginService(_enqueued_high[_crt_tx_slice].back())){
        _serv = QUEUE_HIGH;
        eventlist().sourceIsPendingRel(*this, drainTime(_enqueued_high[_crt_tx_slice].back()));
        _crt = _crt + 64; // !!! hardcoded header size for now...
      }

			// debug:
			//if (_tor == 0 && _port == 6)
			//	cout << "composite_queue sending a header (full packets in queue)" << endl;
		} else {
			assert(_crt < _ratio_high+_ratio_low);
      if(canBeginService(_enqueued_low[_crt_tx_slice].back())){
        _serv = QUEUE_LOW;
        eventlist().sourceIsPendingRel(*this, drainTime(_enqueued_low[_crt_tx_slice].back()));
        int sz = _enqueued_low[_crt_tx_slice].back()->size();
        _crt = _crt + sz;
      }
			// debug:
			//if (_tor == 0 && _port == 6) {
			//	cout << "composite_queue sending a full packet (headers in queue)" << endl;
			//	cout << "   NDP packet size measured at composite_queue = " << sz << " bytes" << endl;
			//}
		}
		return;
	}

	if (!_enqueued_high[_crt_tx_slice].empty()) {
    if(canBeginService(_enqueued_high[_crt_tx_slice].back())){
      _serv = QUEUE_HIGH;
      eventlist().sourceIsPendingRel(*this, drainTime(_enqueued_high[_crt_tx_slice].back()));
    }

		// debug:
		//if (_tor == 0 && _port == 6)
		//	cout << "composite_queue sending a header (no packets in queue)" << endl;

	} else if (!_enqueued_low[_crt_tx_slice].empty()) {
    if(canBeginService(_enqueued_low[_crt_tx_slice].back())){
      _serv = QUEUE_LOW;
      eventlist().sourceIsPendingRel(*this, drainTime(_enqueued_low[_crt_tx_slice].back()));
    }

		// debug:
		//if (_tor == 0 && _port == 6)
		//	cout << "composite_queue sending a full packet: " << (_enqueued_low[_crt_tx_slice].back())->size() << " bytes (no headers in queue)" << endl;

	} else if (!_enqueued_rlb[_crt_tx_slice].empty()) {
		_serv = QUEUE_RLB;
		eventlist().sourceIsPendingRel(*this, drainTime(_enqueued_rlb[_crt_tx_slice].back()));
	} else {
    cout << "assert0 " << _tor << " " << _port << " " << queuesize() << " " << slice_queuesize(_crt_tx_slice) << endl;
		assert(0);
		_serv = QUEUE_INVALID;
	}
}

void CompositeQueue::completeService() {

	Packet* pkt;

	uint64_t new_NDP_bytes_sent;

	bool sendingpkt = true;

	if (_serv == QUEUE_RLB) {
		assert(!_enqueued_rlb[_crt_tx_slice].empty());
		pkt = _enqueued_rlb[_crt_tx_slice].back();

		DynExpTopology* top = pkt->get_topology();
		int ul = top->no_of_hpr(); // !!! # uplinks = # downlinks = # hosts per rack

		if (_port >= ul) { // it's a ToR uplink

			// check if we're still connected to the right rack:
			
			int slice = top->time_to_slice(eventlist().now());

            bool pktfound = false;

            while (!_enqueued_rlb[_crt_tx_slice].empty()) {

            	pkt = _enqueued_rlb[_crt_tx_slice].back(); // get the next packet

            	// get the destination ToR:
            	int dstToR = top->get_firstToR(pkt->get_dst());
            	// get the currently-connected ToR:
            	int nextToR = top->get_nextToR(slice, pkt->get_crtToR(), pkt->get_crtport());

            	if (dstToR == nextToR && !top->is_reconfig(eventlist().now())) {
            		// this is a "fresh" RLB packet
            		_enqueued_rlb[_crt_tx_slice].pop_back();
					_queuesize_rlb[_crt_tx_slice] -= pkt->size();
					pktfound = true;
            		break;
            	} else {
            		// this is an old packet, "drop" it and move on to the next one

            		RlbPacket *p = (RlbPacket*)(pkt);

            		// debug:
            		//cout << "X dropped an RLB packet at port " << pkt->get_crtport() << ", seqno:" << p->seqno() << endl;
            		//cout << "   ~ checked @ " << timeAsUs(eventlist().now()) << " us: specified ToR:" << dstToR << ", current ToR:" << nextToR << endl;
            		
            		// old version: just drop the packet
            		//pkt->free();

            		// new version: NACK the packet
            		// NOTE: have not actually implemented NACK mechanism... Future work
            		RlbModule* module = top->get_rlb_module(p->get_src()); // returns pointer to Rlb module that sent the packet
    				module->receivePacket(*p, 1); // 1 means to put it at the front of the queue

            		_enqueued_rlb[_crt_tx_slice].pop_back(); // pop the packet
					_queuesize_rlb[_crt_tx_slice] -= pkt->size(); // decrement the queue size
            	}
            }

            // we went through the whole queue and they were all "old" packets
            if (!pktfound)
            	sendingpkt = false;

		} else { // its a ToR downlink
			_enqueued_rlb[_crt_tx_slice].pop_back();
			_queuesize_rlb[_crt_tx_slice] -= pkt->size();
		}

		//_num_packets++;
	} else if (_serv == QUEUE_LOW) {
		assert(!_enqueued_low[_crt_tx_slice].empty());
		pkt = _enqueued_low[_crt_tx_slice].back();
		_enqueued_low[_crt_tx_slice].pop_back();
		_queuesize_low[_crt_tx_slice] -= pkt->size();
		_num_packets++;
	} else if (_serv == QUEUE_HIGH) {
		assert(!_enqueued_high[_crt_tx_slice].empty());
		pkt = _enqueued_high[_crt_tx_slice].back();
		_enqueued_high[_crt_tx_slice].pop_back();
		_queuesize_high[_crt_tx_slice] -= pkt->size();
    	if (pkt->type() == NDPACK)
			_num_acks++;
    	else if (pkt->type() == NDPNACK)
			_num_nacks++;
    	else if (pkt->type() == NDPPULL)
			_num_pulls++;
    	else {
			_num_headers++;
    	}
  	} else {
    	assert(0);
  	}
    
    if (sendingpkt) {
      assert(pkt->get_crtslice() == _crt_tx_slice);
  		sendFromQueue(pkt);
  }

  	_serv = QUEUE_INVALID;

  	if (!_enqueued_high[_crt_tx_slice].empty() || !_enqueued_low[_crt_tx_slice].empty() || !_enqueued_rlb[_crt_tx_slice].empty())
  		beginService();
}

void CompositeQueue::doNextEvent() {
	completeService();
}

void CompositeQueue::receivePacket(Packet& pkt) {

	// debug:
	//if (pkt.get_time_sent() == 342944606400 && pkt.get_real_src() == 177 && pkt.get_real_dst() == 423)
	//	cout << "debug @compositequeue: ToR " << _tor << ", port " << _port << " received the packet" << endl;

	// debug:
	//cout << "_maxsize = " << _maxsize << endl;
	//cout << "CompositeQueue (node " << _nodename << ") sending a packet at " << timeAsUs(eventlist().now()) << " us" << endl;
    //pkt.flow().logTraffic(pkt,*this,TrafficLogger::PKT_ARRIVE);

    // debug:
	//if (pkt.been_bounced() == true && pkt.bounced() == false) {
	//	cout << "ToR " << _tor << " received a previously bounced packet" << endl;
	//	cout << "    src = " << pkt.get_src() << endl;
	//} else if (pkt.bounced() == true) {
	//	cout << "ToR " << _tor << " received a currently bounced packet" << endl;
	//	cout << "    src = " << pkt.get_src() << endl;
	//}

  if (pkt.size() == UINT16_MAX) {
    pkt.free();
    _num_drops++;
    return;
  }
  int pkt_slice = _top->is_downlink(_port) ? 0 : pkt.get_crtslice();
	switch (pkt.type()) {
    case RLB:
    {
        
    	// debug:
    	//RlbPacket *p = (RlbPacket*)(&pkt);
        //    if (p->seqno() == 1)
        //        cout << "# marked packet queued at ToR: " << _tor << ", port: " << _port << endl;

    	_enqueued_rlb[pkt_slice].push_front(&pkt);
		_queuesize_rlb[pkt_slice] += pkt.size();

        break;
    }
    case NDP:
    {
        NdpPacket *p = (NdpPacket*)&pkt;
        p->inc_queueing(_queuesize_low[pkt_slice]);
        /*
        if (p->get_src() == 403 && p->get_dst() == 19 && (p->seqno() == 1066949 || p->seqno() == 1065513)) {
            cout << "SEQ " << p->seqno() << " INQUEUE " << nodename() << " AT " << 
                eventlist().now()/1E6 << endl;
        }
        */
    }
    case NDPACK:
    case NDPNACK:
    case NDPPULL:
    {
        if (!pkt.header_only()){

        	// debug:
			//if (_tor == 0 && _port == 6)
			//	cout << "> received a full packet: " << pkt.size() << " bytes" << endl;

			if (_queuesize_low[pkt_slice] + pkt.size() <= _maxsize /*|| drand()<0.5*/) {
				//regular packet; don't drop the arriving packet

				// we are here because either the queue isn't full or,
				// it might be full and we randomly chose an
				// enqueued packet to trim

				bool chk = true;
		    
				if (_queuesize_low[pkt_slice] + pkt.size() > _maxsize) {
					// we're going to drop an existing packet from the queue

					// debug:
					//if (_tor == 0 && _port == 6)
					//	cout << "  x clipping a queued packet. (_queuesize_low[pkt_slice] = " << _queuesize_low[pkt_slice] << ", pkt.size() = " << pkt.size() << ", _maxsize = " << _maxsize << ")" << endl;

					if (_enqueued_low[pkt_slice].empty()){
						//cout << "QUeuesize " << _queuesize_low[pkt_slice] << " packetsize " << pkt.size() << " maxsize " << _maxsize << endl;
						assert(0);
					}
					//take last packet from low prio queue, make it a header and place it in the high prio queue

					Packet* booted_pkt = _enqueued_low[pkt_slice].front();

					// added a check to make sure that the booted packet makes enough space in the queue
        			// for the incoming packet
					if (booted_pkt->size() >= pkt.size()) {

						chk = true;

						_enqueued_low[pkt_slice].pop_front();
						_queuesize_low[pkt_slice] -= booted_pkt->size();

						booted_pkt->strip_payload();
						_num_stripped++;
						booted_pkt->flow().logTraffic(*booted_pkt,*this,TrafficLogger::PKT_TRIM);
						if (_logger)
							_logger->logQueue(*this, QueueLogger::PKT_TRIM, pkt);
				
						if (_queuesize_high[pkt_slice]+booted_pkt->size() > _maxsize) {

							// debug:
							//cout << "!!! NDP - header queue overflow <booted> ..." << endl;

							// old stuff -------------
							//cout << "Error - need to implement RTS handling!" << endl;
							//abort();
							//booted_pkt->free();
							// -----------------------

							// new stuff:

							if (booted_pkt->bounced() == false) {
                return returnToSender(booted_pkt); 
						    } else {
						    	// debug:
								cout << "   ... this is an RTS packet. Dropped.\n";
								//booted_pkt->flow().logTraffic(*booted_pkt,*this,TrafficLogger::PKT_DROP);
								booted_pkt->free();
								//if (_logger) _logger->logQueue(*this, QueueLogger::PKT_DROP, pkt);
						    }
						}
						else {
							_enqueued_high[pkt_slice].push_front(booted_pkt);
							_queuesize_high[pkt_slice] += booted_pkt->size();
						}
					} else {
						chk = false;
					}
				}

				if (chk) {
					// the new packet fit
					assert(_queuesize_low[pkt_slice] + pkt.size() <= _maxsize);
					_enqueued_low[pkt_slice].push_front(&pkt);
					_queuesize_low[pkt_slice] += pkt.size();
					if (_serv==QUEUE_INVALID && slice_queuesize(_crt_tx_slice) > 0) {
						beginService();
					}
					return;
				} else {
					// the packet wouldn't fit if we booted the existing packet
					pkt.strip_payload();
					_num_stripped++;
				}

			} else {
				//strip payload on the arriving packet - low priority queue is full
				pkt.strip_payload();
				_num_stripped++;

				// debug:
				//if (_tor == 0 && _port == 6)
				//	cout << "  > stripping payload of arriving packet" << endl;

			}
	    }

	    assert(pkt.header_only());
	    
	    if (_queuesize_high[pkt_slice] + pkt.size() > _maxsize){
			
			// debug:
			//cout << "!!! NDP - header queue overflow ..." << endl;

			// old stuff -------------
			//cout << "_queuesize_high[pkt_slice] = " << _queuesize_high[pkt_slice] << endl;
			//cout << "Error - need to implement RTS handling!" << endl;
			//abort();
			//pkt.free();
			// -----------------------

			// new stuff:

			if (pkt.bounced() == false) {
        return returnToSender(&pkt); 

			} else {

				// debug:
				cout << "   ... this is an RTS packet. Dropped.\n";
				//if (_logger) _logger->logQueue(*this, QueueLogger::PKT_DROP, pkt);
	    		//pkt.flow().logTraffic(pkt,*this,TrafficLogger::PKT_DROP);

				pkt.free();
				_num_drops++;
	    		return;
			}
	    }

	    // debug:
	    //if (_tor == 0 && _port == 6)
	    //	cout << "  > enqueueing header" << endl;
	    
	    _enqueued_high[pkt_slice].push_front(&pkt);
	    _queuesize_high[pkt_slice] += pkt.size();

        break;
    }
    }
    
		if (_serv==QUEUE_INVALID && slice_queuesize(_crt_tx_slice) > 0) {
		beginService();
    }
}

mem_b CompositeQueue::queuesize() {
    if(_top->is_downlink(_port))
        return _queuesize_low[0]+_queuesize_high[0];
    int slices = _top->get_nlogicslices();
    int sum = 0;
    for(int i = 0; i < slices; i++){
        sum += _queuesize_low[i]+_queuesize_high[i];
    }
    return sum;
}

mem_b CompositeQueue::slice_queuesize(int slice){
    return _queuesize_low[slice]+_queuesize_high[slice];
}
