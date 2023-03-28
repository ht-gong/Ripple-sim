// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-
#include "compositequeue.h"
#include <math.h>

#include <climits>
#include <iostream>
#include <sstream>
#include <algorithm>

#include "dynexp_topology.h"

extern uint32_t delay_host2ToR; // nanoseconds, host-to-tor link
extern uint32_t delay_ToR2ToR; // nanoseconds, tor-to-tor link

// !!! NOTE: this one does selective RLB packet dropping.

// !!! NOTE: this has been modified to also include a lower priority RLB queue

CompositeQueue::CompositeQueue(linkspeed_bps bitrate, mem_b maxsize, EventList& eventlist,
			       QueueLogger* logger, int tor, int port)
  : Queue(bitrate, maxsize, eventlist, logger), _hop_delay_forward(HopDelayForward(eventlist, this))
{
  _tor = tor;
  _port = port;
  // original version:
  //_ratio_high = 10; // number of headers to send per data packet (originally 24 for Jan '18 version)
  //_ratio_low = 1; // number of full packets
  // new version:
  _ratio_high = 640 * _shortflow_bound; // bytes (640 = 10 64B headers)
  _ratio_low = 1500; // bytes (1500 = 1 1500B packet)
	_ratio_high_prime = 640 * _longflow_bound; // bytes (640 = 10 64B headers)
  _ratio_low_prime = 1500; // bytes (1500 = 1 1500B packet)
  _crt = 0;
  _num_headers = 0;
  _num_packets = 0;
  _num_acks = 0;
  _num_nacks = 0;
  _num_pulls = 0;
  _num_drops = 0;
  _num_stripped = 0;
  _num_bounced = 0;

  _queuesize_high = _queuesize_low = _queuesize_high_prime = _queuesize_low_prime = 0;
  _serv = QUEUE_INVALID;

}

void CompositeQueue::beginService(){
	// YX: Not sure what this does. Do we need to extend it for long flows?
	if ( !_enqueued_high.empty() && !_enqueued_low.empty() ){

		if (_crt >= (_ratio_high+_ratio_low))
			_crt = 0;

		if (_crt< _ratio_high) {
			_serv = QUEUE_HIGH;
			Packet* sent_pkt = _enqueued_high.back();
			simtime_picosec sent_time = _hop_delay_forward.routing(sent_pkt, eventlist().now());
			eventlist().sourceIsPending(*this, sent_time);
			_crt = _crt + 64; // !!! hardcoded header size for now...

			// debug:
			//if (_tor == 0 && _port == 6)
			//	cout << "composite_queue sending a header (full packets in queue)" << endl;
		} else {
			assert(_crt < _ratio_high+_ratio_low);
			_serv = QUEUE_LOW;
			Packet* sent_pkt = _enqueued_low.back();
			simtime_picosec sent_time = _hop_delay_forward.routing(sent_pkt, eventlist().now());
			eventlist().sourceIsPending(*this, sent_time);
			int sz = _enqueued_low.back()->size();
			_crt = _crt + sz;
			// debug:
			//if (_tor == 0 && _port == 6) {
			//	cout << "composite_queue sending a full packet (headers in queue)" << endl;
			//	cout << "   NDP packet size measured at composite_queue = " << sz << " bytes" << endl;
			//}
		}
		return;
	}

	if (!_enqueued_high.empty()) {
		_serv = QUEUE_HIGH;
		Packet* sent_pkt = _enqueued_high.back();
		simtime_picosec sent_time = _hop_delay_forward.routing(sent_pkt, eventlist().now());
		eventlist().sourceIsPending(*this, sent_time);

		// debug:
		//if (_tor == 0 && _port == 6)
		//	cout << "composite_queue sending a header (no packets in queue)" << endl;

	} else if (!_enqueued_low.empty()) {
		_serv = QUEUE_LOW;
		Packet* sent_pkt = _enqueued_low.back();
		simtime_picosec sent_time = _hop_delay_forward.routing(sent_pkt, eventlist().now());
		eventlist().sourceIsPending(*this, sent_time);

		// debug:
		//if (_tor == 0 && _port == 6)
		//	cout << "composite_queue sending a full packet: " << (_enqueued_low.back())->size() << " bytes (no headers in queue)" << endl;

	} else if (!_enqueued_high_prime.empty()) {
		_serv = QUEUE_HIGH_PRIME;
		Packet* sent_pkt = _enqueued_high_prime.back();
		simtime_picosec sent_time = _hop_delay_forward.routing(sent_pkt, eventlist().now());
		eventlist().sourceIsPending(*this, sent_time);
	} else if (!_enqueued_low_prime.empty()) {
		_serv = QUEUE_LOW_PRIME;
		Packet* sent_pkt = _enqueued_low_prime.back();
		simtime_picosec sent_time = _hop_delay_forward.routing(sent_pkt, eventlist().now());
		eventlist().sourceIsPending(*this, sent_time);
	} else {
		assert(0);
		_serv = QUEUE_INVALID;
	}
}

void CompositeQueue::completeService() {
	Packet* pkt;

	uint64_t new_NDP_bytes_sent;

	bool sendingpkt = true;

	if (_serv == QUEUE_LOW) {
		assert(!_enqueued_low.empty());
		pkt = _enqueued_low.back();
		_enqueued_low.pop_back();
		_queuesize_low -= pkt->size();
		_num_packets++;
	} else if (_serv == QUEUE_HIGH) {
		assert(!_enqueued_high.empty());
		pkt = _enqueued_high.back();
		_enqueued_high.pop_back();
		_queuesize_high -= pkt->size();
    	if (pkt->type() == NDPACK)
			_num_acks++;
    	else if (pkt->type() == NDPNACK)
			_num_nacks++;
    	else if (pkt->type() == NDPPULL)
			_num_pulls++;
    	else {
			_num_headers++;
    	}
  	} else if (_serv == QUEUE_LOW_PRIME) {
			assert(!_enqueued_low_prime.empty());
			pkt = _enqueued_low_prime.back();
			_enqueued_low_prime.pop_back();
			_queuesize_low_prime -= pkt->size();
			_num_packets++;
		} else if (_serv == QUEUE_HIGH_PRIME) {
			assert(!_enqueued_high_prime.empty());
			pkt = _enqueued_high_prime.back();
			_enqueued_high_prime.pop_back();
			_queuesize_high_prime -= pkt->size();
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

    if (sendingpkt)
  		sendFromQueue(pkt);

  	_serv = QUEUE_INVALID;

  	if (!_enqueued_high.empty() || !_enqueued_low.empty() || !_enqueued_high_prime.empty() || !_enqueued_low_prime.empty())
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

    //pkt.flow().logTraffic(pkt,*this,TrafficLogger::PKT_ARRIVE);

    // debug:
	//if (pkt.been_bounced() == true && pkt.bounced() == false) {
	//	cout << "ToR " << _tor << " received a previously bounced packet" << endl;
	//	cout << "    src = " << pkt.get_src() << endl;
	//} else if (pkt.bounced() == true) {
	//	cout << "ToR " << _tor << " received a currently bounced packet" << endl;
	//	cout << "    src = " << pkt.get_src() << endl;
	//}
	if (!pkt.get_longflow()) {
	  if (!pkt.header_only()){

	      	// debug:
			//if (_tor == 0 && _port == 6)
			//	cout << "> received a full packet: " << pkt.size() << " bytes" << endl;

			if (_queuesize_low + pkt.size() <= _maxsize * _shortflow_bound || drand()<0.5) {
				//regular packet; don't drop the arriving packet

				// we are here because either the queue isn't full or,
				// it might be full and we randomly chose an
				// enqueued packet to trim

				bool chk = true;

				if (_queuesize_low + pkt.size() > _maxsize * _shortflow_bound) {
					// we're going to drop an existing packet from the queue

					// debug:
					//if (_tor == 0 && _port == 6)
					//	cout << "  x clipping a queued packet. (_queuesize_low = " << _queuesize_low << ", pkt.size() = " << pkt.size() << ", _maxsize = " << _maxsize << ")" << endl;

					if (_enqueued_low.empty()){
						//cout << "QUeuesize " << _queuesize_low << " packetsize " << pkt.size() << " maxsize " << _maxsize << endl;
						assert(0);
					}
					//take last packet from low prio queue, make it a header and place it in the high prio queue

					Packet* booted_pkt = _enqueued_low.front();

					// added a check to make sure that the booted packet makes enough space in the queue
	      			// for the incoming packet
					if (booted_pkt->size() >= pkt.size()) {

						chk = true;

						_enqueued_low.pop_front();
						_queuesize_low -= booted_pkt->size();

						booted_pkt->strip_payload();
						_num_stripped++;
						booted_pkt->flow().logTraffic(*booted_pkt,*this,TrafficLogger::PKT_TRIM);
						if (_logger)
							_logger->logQueue(*this, QueueLogger::PKT_TRIM, pkt);

						if (_queuesize_high+booted_pkt->size() > _maxsize * _shortflow_bound) {

							// debug:
							//cout << "!!! NDP - header queue overflow <booted> ..." << endl;

							// old stuff -------------
							//cout << "Error - need to implement RTS handling!" << endl;
							//abort();
							//booted_pkt->free();
							// -----------------------

							// new stuff:

							if (booted_pkt->bounced() == false) {
                                _hop_delay_forward.bouncePkt(&pkt);
								_num_bounced++;
						    } else {
						    	// debug:
								cout << "   ... this is an RTS packet. Dropped.\n";
								//booted_pkt->flow().logTraffic(*booted_pkt,*this,TrafficLogger::PKT_DROP);
								booted_pkt->free();
								//if (_logger) _logger->logQueue(*this, QueueLogger::PKT_DROP, pkt);
						    }
						}
						else {
							_enqueued_high.push_front(booted_pkt);
							_queuesize_high += booted_pkt->size();
						}
					} else {
						chk = false;
					}
				}

				if (chk) {
					// the new packet fit
					assert(_queuesize_low + pkt.size() <= _maxsize * _shortflow_bound);
					_enqueued_low.push_front(&pkt);
					_queuesize_low += pkt.size();
					if (_serv==QUEUE_INVALID) {
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

	    if (_queuesize_high + pkt.size() > _maxsize * _shortflow_bound){

			// debug:
			//cout << "!!! NDP - header queue overflow ..." << endl;

			// old stuff -------------
			//cout << "_queuesize_high = " << _queuesize_high << endl;
			//cout << "Error - need to implement RTS handling!" << endl;
			//abort();
			//pkt.free();
			// -----------------------

			// new stuff:

			if (pkt.bounced() == false) {
	    		//return the packet to the sender
	    		//if (_logger) _logger->logQueue(*this, QueueLogger::PKT_BOUNCE, pkt);
	    		//pkt.flow().logTraffic(pkt,*this,TrafficLogger::PKT_BOUNCE);

	    		// debug:
				//cout << "   ... returning to sender." << endl;

				// DynExpTopology* top = pkt.get_topology();
	    	// 	pkt.bounce();
                _hop_delay_forward.bouncePkt(&pkt);
	    		_num_bounced++;

	    		return;
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

	    _enqueued_high.push_front(&pkt);
	    _queuesize_high += pkt.size();
		} else { // Duplication code for long flows. Need to be optimized.
			if (!pkt.header_only()){

		      	// debug:
				//if (_tor == 0 && _port == 6)
				//	cout << "> received a full packet: " << pkt.size() << " bytes" << endl;

				if (_queuesize_low_prime + pkt.size() <= _maxsize * _longflow_bound || drand()<0.5) {
					//regular packet; don't drop the arriving packet

					// we are here because either the queue isn't full or,
					// it might be full and we randomly chose an
					// enqueued packet to trim

					bool chk = true;

					if (_queuesize_low_prime + pkt.size() > _maxsize * _longflow_bound) {
						// we're going to drop an existing packet from the queue

						// debug:
						//if (_tor == 0 && _port == 6)
						//	cout << "  x clipping a queued packet. (_queuesize_low = " << _queuesize_low << ", pkt.size() = " << pkt.size() << ", _maxsize = " << _maxsize << ")" << endl;

						if (_enqueued_low_prime.empty()){
							//cout << "QUeuesize " << _queuesize_low << " packetsize " << pkt.size() << " maxsize " << _maxsize << endl;
							assert(0);
						}
						//take last packet from low prio queue, make it a header and place it in the high prio queue

						Packet* booted_pkt = _enqueued_low_prime.front();

						// added a check to make sure that the booted packet makes enough space in the queue
		      			// for the incoming packet
						if (booted_pkt->size() >= pkt.size()) {

							chk = true;

							_enqueued_low_prime.pop_front();
							_queuesize_low_prime -= booted_pkt->size();

							booted_pkt->strip_payload();
							_num_stripped++;
							booted_pkt->flow().logTraffic(*booted_pkt,*this,TrafficLogger::PKT_TRIM);
							if (_logger)
								_logger->logQueue(*this, QueueLogger::PKT_TRIM, pkt);

							if (_queuesize_high_prime+booted_pkt->size() > _maxsize * _longflow_bound) {

								// debug:
								//cout << "!!! NDP - header queue overflow <booted> ..." << endl;

								// old stuff -------------
								//cout << "Error - need to implement RTS handling!" << endl;
								//abort();
								//booted_pkt->free();
								// -----------------------

								// new stuff:

								if (booted_pkt->bounced() == false) {
									//return the packet to the sender
									//if (_logger) _logger->logQueue(*this, QueueLogger::PKT_BOUNCE, *booted_pkt);
									//booted_pkt->flow().logTraffic(pkt,*this,TrafficLogger::PKT_BOUNCE);

									// debug:
									//cout << "   ... returning to sender." << endl;

									// DynExpTopology* top = booted_pkt->get_topology();
									// booted_pkt->bounce(); // indicate that the packet has been bounced
									_num_bounced++;
							    } else {
							    	// debug:
									cout << "   ... this is an RTS packet. Dropped.\n";
									//booted_pkt->flow().logTraffic(*booted_pkt,*this,TrafficLogger::PKT_DROP);
									booted_pkt->free();
									//if (_logger) _logger->logQueue(*this, QueueLogger::PKT_DROP, pkt);
							    }
							}
							else {
								_enqueued_high_prime.push_front(booted_pkt);
								_queuesize_high_prime += booted_pkt->size();
							}
						} else {
							chk = false;
						}
					}

					if (chk) {
						// the new packet fit
						assert(_queuesize_low_prime + pkt.size() <= _maxsize * _longflow_bound);
						_enqueued_low_prime.push_front(&pkt);
						_queuesize_low_prime += pkt.size();
						if (_serv==QUEUE_INVALID) {
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

		    if (_queuesize_high_prime + pkt.size() > _maxsize * _longflow_bound){

				// debug:
				//cout << "!!! NDP - header queue overflow ..." << endl;

				// old stuff -------------
				//cout << "_queuesize_high = " << _queuesize_high << endl;
				//cout << "Error - need to implement RTS handling!" << endl;
				//abort();
				//pkt.free();
				// -----------------------

				// new stuff:

				if (pkt.bounced() == false) {
		    		//return the packet to the sender
		    		//if (_logger) _logger->logQueue(*this, QueueLogger::PKT_BOUNCE, pkt);
		    		//pkt.flow().logTraffic(pkt,*this,TrafficLogger::PKT_BOUNCE);

		    		// debug:
					//cout << "   ... returning to sender." << endl;

					// DynExpTopology* top = pkt.get_topology();
		    	// 	pkt.bounce();
                    _hop_delay_forward.bouncePkt(&pkt);
		    		_num_bounced++;
		    		return;
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

		    _enqueued_high_prime.push_front(&pkt);
		    _queuesize_high_prime += pkt.size();
		}

    if (_serv == QUEUE_INVALID) {
			beginService();
    }
}

mem_b CompositeQueue::queuesize() {
    return _queuesize_low + _queuesize_high + _queuesize_low_prime + _queuesize_high;
}

HopDelayForward::HopDelayForward(EventList &eventlist, CompositeQueue* cq)
	: EventSource(eventlist, "HopDelayForward"), _composite_queue(cq) {}

void HopDelayForward::insertBouncedQ(simtime_picosec t, Packet* pkt) {
	pair<simtime_picosec, Packet*> item = make_pair(t, pkt);
	// YX: TODO: change to binary search with comparator
    // FD: IDK still to test vv
    _bounced_pkts.insert(std::upper_bound(_bounced_pkts.begin(), _bounced_pkts.end(),
                item, [](pair<simtime_picosec, Packet*> a, pair<simtime_picosec, Packet*> b){
                return a.first < b.first;
                }), item);
    /*
	bool inserted = false;
	for (int i = 0; i < _bounced_pkts.size(); i++) {
		if (_bounced_pkts[i].first <= item.first) {
			_bounced_pkts.insert(_bounced_pkts.begin()+i, item);
			inserted = true;
			break;
		}
	}
	if (!inserted) {
		_bounced_pkts.push_back(item);
	}
    */
}

void HopDelayForward::bouncePkt(Packet* pkt) {
	DynExpTopology* top = pkt->get_topology();
	pkt->bounce(); // indicate that the packet has been bounced

	// flip the source and dst of the packet:
	int s = pkt->get_src();
	int d = pkt->get_dst();
	pkt->set_src(d);
	pkt->set_dst(s);

	// get the current ToR, this will be the new src_ToR of the packet
	int new_src_ToR = pkt->get_crtToR();

	if (new_src_ToR == pkt->get_src_ToR()) {
		// the packet got returned at the source ToR
		// we need to send on a downlink right away
		pkt->set_src_ToR(new_src_ToR);
		pkt->set_crtport(top->get_lastport(pkt->get_dst()));
		pkt->set_maxhops(0);
		pkt->set_crthop(0);
		pkt->set_crtToR(new_src_ToR);

		Queue* nextqueue = top->get_queue_tor(pkt->get_crtToR(), pkt->get_crtport());
		nextqueue->receivePacket(*pkt);
		// debug:
		//cout << "   packet RTSed at the first ToR (ToR = " << new_src_ToR << ")" << endl;

	} else {
		pkt->set_src_ToR(new_src_ToR);
		// YX: Can be removed, need double check
		pkt->set_path_index(0); // set which path the packet will take
		// YX: no need to set a limit to # max hops, need double check.
		pkt->set_maxhops(INT_MAX);

		simtime_picosec sent_time = routing(pkt, eventlist().now());
		insertBouncedQ(sent_time, pkt);
		eventlist().sourceIsPending(*this, sent_time);
	}
}

void HopDelayForward::doNextEvent() {
	pair<simtime_picosec, Packet*> to_sent = _bounced_pkts.back();
	assert(to_sent.first == eventlist().now());
	Packet* pkt = to_sent.second;
	DynExpTopology* top = pkt->get_topology();
	Queue* nextqueue = top->get_queue_tor(pkt->get_crtToR(), pkt->get_crtport());
	nextqueue->receivePacket(*pkt);
	_bounced_pkts.pop_back();
}

simtime_picosec HopDelayForward::routing(Packet* pkt, simtime_picosec t) {
	DynExpTopology* top = pkt->get_topology();
	int slice = top->time_to_slice(t);

	pair<int, int> route = top->get_routing(pkt->get_crtToR(),
		top->get_firstToR(pkt->get_dst()), slice);

	int uplink = route.first;
	int sent_slice = route.second;

	if (sent_slice == slice) {
		int finish_slice = top->time_to_slice(t + _composite_queue->drainTime(pkt)
			+ timeFromNs(delay_ToR2ToR)); // plus the link delay

		if (finish_slice == slice) {
			return (t + _composite_queue->drainTime(pkt));
		}
		// May wrap around the cycle
		int slice_delta = (finish_slice - slice) % (top->get_nsuperslice() * 2);

		if (slice_delta == 1) {
			if (top->get_nextToR(slice, pkt->get_crtToR(), uplink) ==
					top->get_nextToR(finish_slice, pkt->get_crtToR(), uplink)) {
				pkt->set_crthop(uplink);
				return (t + _composite_queue->drainTime(pkt));
			}
			// Miss this slice, have to wait till the next slice
			int next_absolute_slice = top->time_to_absolute_slice(t) + 1;
			simtime_picosec next_time = top->get_slice_start_time(next_absolute_slice);
			return routing(pkt, next_time);
		}

		assert(slice_delta == 2); // Cannot be more than 2!

		int middle_slice = (slice + 1) % (top->get_nsuperslice() * 2);

		if (top->get_nextToR(slice, pkt->get_crtToR(), uplink) ==
				top->get_nextToR(middle_slice, pkt->get_crtToR(), uplink) ==
				top->get_nextToR(finish_slice, pkt->get_crtToR(), uplink)) {
			pkt->set_crthop(uplink);
			return (t + _composite_queue->drainTime(pkt));
		}
		// Miss this slice, have to wait till the next slice
		int next_absolute_slice = top->time_to_absolute_slice(t) + 1;
		simtime_picosec next_time = top->get_slice_start_time(next_absolute_slice);
		return routing(pkt, next_time);
	} else {
		// Send at the beginning of the sent_slice
		int wait_slice = (sent_slice - slice) % (top->get_nsuperslice() * 2);
		int sent_absolute_slice = top->time_to_absolute_slice(t) + wait_slice;
		pkt->set_crthop(uplink);
		return (top->get_slice_start_time(sent_absolute_slice) +
			_composite_queue->drainTime(pkt));
	}
}
