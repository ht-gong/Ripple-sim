// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-
#include "compositequeue.h"
#include <math.h>

#include <climits>
#include <iostream>
#include <sstream>
#include <algorithm>

#include "dynexp_topology.h"

//use clearing calendar queues
//#define CALENDAR_CLEAR

int max_used_queues = 0;
extern uint32_t delay_host2ToR; // nanoseconds, host-to-tor link
extern uint32_t delay_ToR2ToR; // nanoseconds, tor-to-tor link

#define TRIM_RATIO INT_MAX
#define SEND_RATIO INT_MAX


// !!! NOTE: this one does selective RLB packet dropping.

// !!! NOTE: this has been modified to also include a lower priority RLB queue

CompositeQueue::CompositeQueue(linkspeed_bps bitrate, mem_b maxsize, EventList& eventlist,
        QueueLogger* logger, DynExpTopology* topology, int tor, int port)
    : Queue(bitrate, maxsize, eventlist, logger), _hop_delay_forward(HopDelayForward(eventlist)), _top(topology),
      _alarm(QueueAlarm(eventlist, port, this, topology))
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

    _crt_send_ratio_high = 0;
    _crt_send_ratio_low = 0;
    _crt_trim_ratio = 0;
    _crt_tx_slice = topology->time_to_slice(eventlist.now());

    int slices = topology->get_nsuperslice()*2;
    //the _dl_queueth element of the vectors is the downlink implementation (so a normal queue)
    _dl_queue = slices;

    _enqueued_high.resize(slices+1);
    _enqueued_low.resize(slices+1);
    _enqueued_high_prime.resize(slices+1);
    _enqueued_low_prime.resize(slices+1);

    _queuesize_high.resize(slices+1);
    _queuesize_low.resize(slices+1);
    _queuesize_high_prime.resize(slices+1);
    _queuesize_low_prime.resize(slices+1);

    std::fill(_queuesize_high.begin(), _queuesize_high.end(), 0);
    std::fill(_queuesize_low.begin(), _queuesize_low.end(), 0);
    std::fill(_queuesize_high_prime.begin(), _queuesize_high_prime.end(), 0);
    std::fill(_queuesize_low_prime.begin(), _queuesize_low_prime.end(), 0);

    //occupancy logging
    _max_tot_qsize = 0;
    _max_slice_qsize.resize(slices+1);
    std::fill(_max_slice_qsize.begin(), _max_slice_qsize.end(), 0);

    _serv = QUEUE_INVALID;
}

void CompositeQueue::beginService(){
    //cout << "beginService " << _tor << " " << _port << endl;
    assert(_serv == QUEUE_INVALID);
    Packet* sent_pkt =  NULL;
#ifdef CALENDAR_CLEAR
    int crt = _top->is_downlink(_port) ? _dl_queue : _top->time_to_slice(eventlist().now());
#else
    int crt = _top->is_downlink(_port) ? _dl_queue : _crt_tx_slice;
#endif
    bool sent_succeed = false;
    //at least one high prio queue and one low prio queue have to send
    if ( (!_enqueued_high[crt].empty() || !_enqueued_high_prime[crt].empty()) && (!_enqueued_low[crt].empty() || !_enqueued_low_prime[crt].empty())){
        if (_crt >= (_ratio_high+_ratio_low))
            _crt = 0;
        //high prio turn
        if (_crt< _ratio_high) {
            //both queues have a packet, choose which to send by highprio send ratio
            if (!_enqueued_high[crt].empty() && !_enqueued_high_prime[crt].empty()){
                if (_crt_send_ratio_high < SEND_RATIO){
                    assert(!_enqueued_high[crt].empty());
                    _serv = QUEUE_HIGH;
                    //raise send ratio
                    _crt_send_ratio_high++;
                } else {
                    assert(!_enqueued_high_prime[crt].empty());
                    _serv = QUEUE_HIGH_PRIME;
                    //reset send ratio
                    _crt_send_ratio_high = 0;
                }
                //only short flow high prio has to send
            } else if (!_enqueued_high[crt].empty()){
                _serv = QUEUE_HIGH;
                //only long flow high prio has to send
            } else {
                _serv = QUEUE_HIGH_PRIME;
            }
            //update high/low prio send ratio
            _crt += 64; //hard coded header size
                        //low prio turn
        } else {
            assert(_crt < _ratio_high+_ratio_low);
            int sz;
            //both queues have a packet, choose which to send by lowprio send ratio
            if (!_enqueued_low[crt].empty() && !_enqueued_low_prime[crt].empty()){
                if (_crt_send_ratio_low < SEND_RATIO){
                    assert(!_enqueued_low[crt].empty());
                    _serv = QUEUE_LOW;
                    sz = _enqueued_low[crt].back()->size();
                    _crt_send_ratio_low++;
                } else {
                    assert(!_enqueued_low_prime[crt].empty());
                    _serv = QUEUE_LOW_PRIME;
                    sz = _enqueued_low_prime[crt].back()->size();
                    _crt_send_ratio_low = 0;
                }
                //only short flow low prio has to send
            } else if (!_enqueued_low[crt].empty()){
                _serv = QUEUE_LOW;
                sz = _enqueued_low[crt].back()->size();
                //only long flow low prio has to send
            } else {
                _serv = QUEUE_LOW_PRIME;
                sz = _enqueued_low_prime[crt].back()->size();
            }
            //update low/low prio send ratio
            _crt += sz; //hard coded header size
        }
        //only high prio and high prio prime have to send
    } else if (!_enqueued_high[crt].empty() && !_enqueued_high_prime[crt].empty()){
        if (_crt_send_ratio_high < SEND_RATIO){
            assert(!_enqueued_high[crt].empty());
            _serv = QUEUE_HIGH;
            _crt_send_ratio_high++;
        } else {
            assert(!_enqueued_high_prime[crt].empty());
            _serv = QUEUE_HIGH_PRIME;
            _crt_send_ratio_high = 0;
        }
        //only low prio and low prio prime have to send
    } else if (!_enqueued_low[crt].empty() && !_enqueued_low_prime[crt].empty()){
        if (_crt_send_ratio_low < SEND_RATIO){
            _serv = QUEUE_LOW;
            _crt_send_ratio_low++;
        } else {
            assert(!_enqueued_low_prime[crt].empty());
            _serv = QUEUE_LOW_PRIME;
            _crt_send_ratio_low = 0;
        } 
        //only one queue has to send
    } else {
        if (!_enqueued_high[crt].empty()) {
            _serv = QUEUE_HIGH;
        } else if (!_enqueued_low[crt].empty()) {
            _serv = QUEUE_LOW;
        } else if (!_enqueued_high_prime[crt].empty()) {
            _serv = QUEUE_HIGH_PRIME;
        } else if (!_enqueued_low_prime[crt].empty()) {
            _serv = QUEUE_LOW_PRIME;
        } else {
            assert(0);
            _serv = QUEUE_INVALID;
        }
    }
    switch(_serv){
        case QUEUE_HIGH:
        {
            sent_pkt = _enqueued_high[crt].back();
            if(crt != _dl_queue) assert(sent_pkt->get_crtslice() == crt);
            int finish_slice = _top->time_to_slice(eventlist().now()+drainTime(sent_pkt) + timeFromNs(delay_ToR2ToR));
            bool in_time = (crt != _dl_queue) && _top->get_nextToR(crt, sent_pkt->get_crtToR(), sent_pkt->get_crtport()) 
                == _top->get_nextToR(finish_slice, sent_pkt->get_crtToR(), sent_pkt->get_crtport());
            if (crt==_dl_queue || in_time) {
                assert(!_enqueued_high[crt].empty());
                _enqueued_high[crt].pop_back();
                _queuesize_high[crt] -= sent_pkt->size();
                if (sent_pkt->type() == NDPACK)
                    _num_acks++;
                else if (sent_pkt->type() == NDPNACK)
                    _num_nacks++;
                else if (sent_pkt->type() == NDPPULL)
                    _num_pulls++;
                else {
                    _num_headers++;
                }
                eventlist().sourceIsPendingRel(*this, drainTime(sent_pkt));
                _sending_pkt = sent_pkt;
                sent_succeed = true;
            } else {
                _queuesize_high[crt] -= sent_pkt->size();
                _enqueued_high[crt].pop_back();
            }
            break;
        }
        case QUEUE_LOW:
        {
            sent_pkt = _enqueued_low[crt].back();
            if(crt != _dl_queue) assert(sent_pkt->get_crtslice() == crt);
            int finish_slice = _top->time_to_slice(eventlist().now()+drainTime(sent_pkt) + timeFromNs(delay_ToR2ToR));
            bool in_time = (crt != _dl_queue) && _top->get_nextToR(crt, sent_pkt->get_crtToR(), sent_pkt->get_crtport()) 
                == _top->get_nextToR(finish_slice, sent_pkt->get_crtToR(), sent_pkt->get_crtport());
            if (crt==_dl_queue || in_time) {
                assert(!_enqueued_low[crt].empty());
                _enqueued_low[crt].pop_back();
                _queuesize_low[crt] -= sent_pkt->size();
                _num_packets++;
                eventlist().sourceIsPendingRel(*this, drainTime(sent_pkt));
                _sending_pkt = sent_pkt;
                sent_succeed = true;
            } else {
                _queuesize_low[crt] -= sent_pkt->size();
                _enqueued_low[crt].pop_back();
            }
            break;
        }
        case QUEUE_HIGH_PRIME:
        {
            sent_pkt = _enqueued_high_prime[crt].back();
            if(crt != _dl_queue) assert(sent_pkt->get_crtslice() == crt);
            int finish_slice = _top->time_to_slice(eventlist().now()+drainTime(sent_pkt) + timeFromNs(delay_ToR2ToR));
            bool in_time = (crt != _dl_queue) && _top->get_nextToR(crt, sent_pkt->get_crtToR(), sent_pkt->get_crtport()) 
                == _top->get_nextToR(finish_slice, sent_pkt->get_crtToR(), sent_pkt->get_crtport());
            if (crt==_dl_queue || in_time) {
                assert(!_enqueued_high_prime[crt].empty());
                _enqueued_high_prime[crt].pop_back();
                _queuesize_high_prime[crt] -= sent_pkt->size();
                if (sent_pkt->type() == NDPACK)
                    _num_acks++;
                else if (sent_pkt->type() == NDPNACK)
                    _num_nacks++;
                else if (sent_pkt->type() == NDPPULL)
                    _num_pulls++;
                else {
                    _num_headers++;
                }
                eventlist().sourceIsPendingRel(*this, drainTime(sent_pkt));
                _sending_pkt = sent_pkt;
                sent_succeed = true;
            } else {
                _queuesize_high_prime[crt] -= sent_pkt->size();
                _enqueued_high_prime[crt].pop_back();
            }
            break;
        }
        case QUEUE_LOW_PRIME:
        {
            sent_pkt = _enqueued_low_prime[crt].back();
            if(crt != _dl_queue) assert(sent_pkt->get_crtslice() == crt);
            int finish_slice = _top->time_to_slice(eventlist().now()+drainTime(sent_pkt) + timeFromNs(delay_ToR2ToR));
            bool in_time = (crt != _dl_queue) && _top->get_nextToR(crt, sent_pkt->get_crtToR(), sent_pkt->get_crtport()) 
                == _top->get_nextToR(finish_slice, sent_pkt->get_crtToR(), sent_pkt->get_crtport());
            if (crt==_dl_queue || in_time) {
                assert(!_enqueued_low_prime[crt].empty());
                _enqueued_low_prime[crt].pop_back();
                _queuesize_low_prime[crt] -= sent_pkt->size();
                _num_packets++;
                eventlist().sourceIsPendingRel(*this, drainTime(sent_pkt));
                _sending_pkt = sent_pkt;
                sent_succeed = true;
                _crt_trim_ratio = 0;
            } else {
                _queuesize_low_prime[crt] -= sent_pkt->size();
                _enqueued_low_prime[crt].pop_back();
            }
            break;
        }
        default:
            assert(0);
    }

    // No silent failures allowed with no RTO
    if(!sent_succeed){
        //cout << "Missed send timing" << endl;
        if (sent_pkt->header_only()){
            if(sent_pkt->bounced())
                sent_pkt->free();
            else
                _hop_delay_forward.bouncePkt(sent_pkt);
        } else {
            sent_pkt->strip_payload();
            receivePacket(*sent_pkt);
        }
        _serv = QUEUE_INVALID;
    }

#ifdef CALENDAR_CLEAR 
    if (!sent_succeed && slice_queuesize(crt) > 0) {
        beginService();
    }
#else
    if (!sent_succeed){
        assert(crt != _dl_queue);
        if(slice_queuesize(crt) > 0){
            beginService();
        } else {
            _crt_tx_slice = next_tx_slice(crt);
            if (slice_queuesize(_crt_tx_slice) > 0)
                beginService();
        }
    }
#endif
}

void CompositeQueue::completeService() {
    //cout << "completeService " << _tor << " " << _port << endl;
    assert(_sending_pkt != NULL);

#ifdef CALENDAR_CLEAR
    int crt = _top->is_downlink(_port) ? _dl_queue : _top->time_to_slice(eventlist().now());
#else
    int crt = _top->is_downlink(_port) ? _dl_queue : _crt_tx_slice;
#endif

    sendFromQueue(_sending_pkt);

    _sending_pkt = NULL;
    _serv = QUEUE_INVALID;

#ifdef CALENDAR_CLEAR 
    if (slice_queuesize(crt) > 0)
        beginService();
#else
    //does this calendar have more to send?
    if (slice_queuesize(crt) > 0){
        beginService();
    //if not and this is not a downlink, switch to next calendar
    } else if (crt != _dl_queue) {
       _crt_tx_slice = next_tx_slice(crt);
       if (slice_queuesize(_crt_tx_slice) > 0)
           beginService();
    }
#endif
}

void CompositeQueue::doNextEvent() {
    completeService();
}

void CompositeQueue::receivePacket(Packet& _pkt) {
    Packet* pkt = &_pkt;
    Packet* booted_pkt = NULL;
    bool packet_added = false;
    int crt = _top->is_downlink(_port) ? _dl_queue : _pkt.get_crtslice();
    int time_crt = _top->time_to_slice(eventlist().now());
    //cout << "receivePacket " << _tor << " " << _port << " for pkt " << pkt->get_src() << " " << pkt->get_dst() << " " << crt << " current slice " << time_crt << " hop " << pkt->get_crthop() << " invalid? " << (_serv==QUEUE_INVALID) << endl;
    if (!pkt->get_longflow()) {
        bool drop_long = false;
        if (!pkt->header_only()){
            //if (slice_queuesize(crt) + pkt->size() <= slice_maxqueuesize(crt) || (_queuesize_low_prime[crt] > 0 && _crt_trim_ratio < TRIM_RATIO && (drop_long=true))
            if (slice_queuesize_low(crt) + pkt->size() <= slice_maxqueuesize(crt) || (_queuesize_low_prime[crt] > 0 && _crt_trim_ratio < TRIM_RATIO && (drop_long=true))
                    || (drand()<0.5 && !_enqueued_low[crt].empty())) {
                //regular packet; don't drop the arriving packet
                // we are here because either the queue isn't full or,
                // it might be full and we randomly chose an
                // enqueued packet to trim from long queue or own queue

                bool chk = true;

                //there is a packet to trime
                //if (slice_queuesize(crt) + pkt->size() > slice_maxqueuesize(crt)) { 
                if (slice_queuesize_low(crt) + pkt->size() > slice_maxqueuesize(crt)) { 
                    //it is a packet from the long queue
                    if (drop_long){
                        booted_pkt = _enqueued_low_prime[crt].front();
                    } else {
                        //take last packet from low prio queue, make it a header and place it in the high prio queue
                        booted_pkt = _enqueued_low[crt].front();
                    }
                    // added a check to make sure that the booted packet makes enough space in the queue
                    // for the incoming packet
                    if (booted_pkt->size() >= pkt->size()) {

                        chk = true;

                        if (drop_long){
                            //cout << "Dropping LONG for SHORT packet" << endl;
                            _enqueued_low_prime[crt].pop_front();
                            _queuesize_low_prime[crt] -= booted_pkt->size();
                            _crt_trim_ratio++;
                        } else {
                            _enqueued_low[crt].pop_front();
                            _queuesize_low[crt] -= booted_pkt->size();
                        }

                        booted_pkt->strip_payload();
                        _num_stripped++;
                        booted_pkt->flow().logTraffic(*booted_pkt,*this,TrafficLogger::PKT_TRIM);
                        if (_logger)
                            _logger->logQueue(*this, QueueLogger::PKT_TRIM, *pkt);

                    } else {
                        chk = false;
                        booted_pkt = NULL;
                    }
                }

                if (chk) {
                    // the new packet fit
                    //assert(slice_queuesize(crt) + pkt->size() <= slice_maxqueuesize(crt));
                    assert(slice_queuesize_low(crt) + pkt->size() <= slice_maxqueuesize(crt));
                    _enqueued_low[crt].push_front(pkt);
                    _queuesize_low[crt] += pkt->size();
                    packet_added = true;
                } else {
                    // the packet wouldn't fit if we booted the existing packet
                    pkt->strip_payload();
                    _num_stripped++;
                }

            } else {
                //strip payload on the arriving packet - low priority queue is full
                pkt->strip_payload();
                _num_stripped++;

            }
        }

        //if a packet was booted, it is now a header to be enqueued in high prio
        if (booted_pkt != NULL){
            pkt = booted_pkt;
        }

        if (pkt->header_only() && !pkt->get_longflow()){
            //FD original opera version has separate full queue sizes for high and low queues
            //so using the overall queue size might break something
            //if (slice_queuesize(crt) + pkt->size() > slice_maxqueuesize(crt)){
            if (slice_queuesize_high(crt) + pkt->size() > slice_maxqueuesize(crt)){
                cout << "Nospace for header" << endl;
                if (pkt->bounced() == false) {
                    _hop_delay_forward.bouncePkt(pkt);
                    _num_bounced++;
                } else {

                    // debug:
                    cout << "   ... this is an RTS packet. Dropped.\n";
                    //if (_logger) _logger->logQueue(*this, QueueLogger::PKT_DROP, pkt);
                    //pkt->flow().logTraffic(pkt,*this,TrafficLogger::PKT_DROP);

                    pkt->free();
                    _num_drops++;
                }
            } else {
                _enqueued_high[crt].push_front(pkt);
                _queuesize_high[crt] += pkt->size();
                packet_added = true;
            }
        }
    }
    if (pkt->get_longflow()){
        if (!pkt->header_only()){
            //if (slice_queuesize(crt) + pkt->size() <= slice_maxqueuesize(crt) || (drand()<0.5 && !_enqueued_low_prime[crt].empty())) {
            if (slice_queuesize_low(crt) + pkt->size() <= slice_maxqueuesize(crt) || (drand()<0.5 && !_enqueued_low_prime[crt].empty())) {
                //regular packet; don't drop the arriving packet

                // we are here because either the queue isn't full or,
                // it might be full and we randomly chose an
                // enqueued packet to trim

                bool chk = true;

                if (slice_queuesize_low(crt) + pkt->size() > slice_maxqueuesize(crt)) {
                //if (slice_queuesize(crt) + pkt->size() > slice_maxqueuesize(crt)) {
                    // we're going to drop an existing packet from the queue
                    if (_enqueued_low_prime[crt].empty()){
                        //cout << "QUeuesize " << _queuesize_low << " packetsize " << pkt->size() << " maxsize " << _maxsize << endl;
                        assert(0);
                    }
                    //take last packet from low prio queue, make it a header and place it in the high prio queue

                    booted_pkt = _enqueued_low_prime[crt].front();

                    // added a check to make sure that the booted packet makes enough space in the queue
                    // for the incoming packet
                    if (booted_pkt->size() >= pkt->size()) {

                        chk = true;

                        _enqueued_low_prime[crt].pop_front();
                        _queuesize_low_prime[crt] -= booted_pkt->size();

                        booted_pkt->strip_payload();
                        _num_stripped++;
                        booted_pkt->flow().logTraffic(*booted_pkt,*this,TrafficLogger::PKT_TRIM);
                        if (_logger)
                            _logger->logQueue(*this, QueueLogger::PKT_TRIM, *pkt);
                    } else {
                        chk = false;
                        booted_pkt = NULL;
                    }
                }

                if (chk) {
                    // the new packet fit
                    //assert(slice_queuesize(crt) + pkt->size() <= slice_maxqueuesize(crt));
                    assert(slice_queuesize_low(crt) + pkt->size() <= slice_maxqueuesize(crt));
                    _enqueued_low_prime[crt].push_front(pkt);
                    _queuesize_low_prime[crt] += pkt->size();
                    packet_added = true;
                } else {
                    // the packet wouldn't fit if we booted the existing packet
                    pkt->strip_payload();
                    _num_stripped++;
                }

            } else {
                //strip payload on the arriving packet - low priority queue is full
                pkt->strip_payload();
                _num_stripped++;
            }
        }

        //if a packet was booted, it is now a header to be enqueued in high prio
        if (booted_pkt != NULL){
            pkt = booted_pkt;
        }

        if (pkt->header_only()) {
            //if (slice_queuesize(crt) + pkt->size() > slice_maxqueuesize(crt)){
            if (slice_queuesize_high(crt) + pkt->size() > slice_maxqueuesize(crt)){
                cout << "Nospace for header" << endl;
                if (pkt->bounced() == false) {
                    _hop_delay_forward.bouncePkt(pkt);
                    _num_bounced++;
                } else {
                    // debug:
                    cout << "   ... this is an RTS packet. Dropped.\n";
                    //if (_logger) _logger->logQueue(*this, QueueLogger::PKT_DROP, pkt);
                    //pkt->flow().logTraffic(pkt,*this,TrafficLogger::PKT_DROP);

                    pkt->free();
                    _num_drops++;
                }
            } else {
                _enqueued_high_prime[crt].push_front(pkt);
                _queuesize_high_prime[crt] += pkt->size();
                packet_added = true;
            }
        }
    }

    //track max queue occupancy
    mem_b crt_tot_qsize = queuesize();
    mem_b crt_slice_qsize = slice_queuesize(crt);
    _max_tot_qsize = crt_tot_qsize > _max_tot_qsize ? crt_tot_qsize : _max_tot_qsize;
    _max_slice_qsize[crt] = crt_slice_qsize > _max_slice_qsize[crt] ? crt_slice_qsize : _max_slice_qsize[crt];

    if (_serv == QUEUE_INVALID && ((crt==_dl_queue && slice_queuesize(_dl_queue) > 0) || slice_queuesize(time_crt)) > 0) {
        _crt_tx_slice = time_crt;
        beginService();
    }
}

void CompositeQueue::clearSliceQueue(int slice){
    //debug 
    int high_drop = _enqueued_high[slice].size() + _enqueued_high_prime[slice].size();
    int low_drop = _enqueued_low[slice].size() + _enqueued_low_prime[slice].size();
    if(high_drop + low_drop > 0)
        cout << "Calendarclear " << high_drop+low_drop << " " << high_drop << " " << low_drop << endl;

    for (std::list<Packet*>::iterator it = _enqueued_high[slice].begin(); it != _enqueued_high[slice].end(); ++it){
        Packet* pkt = (*it);
        if (pkt->header_only()){
            if(pkt->bounced())
                pkt->free();
            else
                _hop_delay_forward.bouncePkt(pkt);
        } else {
            pkt->strip_payload();
            _hop_delay_forward.bouncePkt(pkt);
        }
    }
    for (std::list<Packet*>::iterator it = _enqueued_low[slice].begin(); it != _enqueued_low[slice].end(); ++it){
        Packet* pkt = (*it);
        if (pkt->header_only()){
            if(pkt->bounced())
                pkt->free();
            else
                _hop_delay_forward.bouncePkt(pkt);
        } else {
            pkt->strip_payload();
            _hop_delay_forward.bouncePkt(pkt);
        }
    }
    for (std::list<Packet*>::iterator it = _enqueued_high_prime[slice].begin(); it != _enqueued_high_prime[slice].end(); ++it){
        Packet* pkt = (*it);
        if (pkt->header_only()){
            if(pkt->bounced())
                pkt->free();
            else
                _hop_delay_forward.bouncePkt(pkt);
        } else {
            pkt->strip_payload();
            _hop_delay_forward.bouncePkt(pkt);
        }
    }
    for (std::list<Packet*>::iterator it = _enqueued_low_prime[slice].begin(); it != _enqueued_low_prime[slice].end(); ++it){
        Packet* pkt = (*it);
        if (pkt->header_only()){
            if(pkt->bounced())
                pkt->free();
            else
                _hop_delay_forward.bouncePkt(pkt);
        } else {
            pkt->strip_payload();
            _hop_delay_forward.bouncePkt(pkt);
        }
    }

    //remove all elements from lists
    _enqueued_high[slice].clear();
    _enqueued_low[slice].clear();
    _enqueued_high_prime[slice].clear();
    _enqueued_low_prime[slice].clear();

    //reset queue sizes
    _queuesize_high[slice] = 0;
    _queuesize_low[slice] = 0;
    _queuesize_high_prime[slice] = 0;
    _queuesize_low_prime[slice] = 0;
}

int CompositeQueue::next_tx_slice(int crt_slice){
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


simtime_picosec CompositeQueue::get_queueing_delay(int slice){
    assert(slice != _dl_queue);
    int i = _crt_tx_slice;
    mem_b bytes = 0;
    while (i != slice){
        bytes += slice_queuesize(i); 
        i = (i+1)%(_top->get_nsuperslice()*2);
    }
    return (bytes+slice_queuesize(slice))*_ps_per_byte;
}

//return queuesize according to corresponding slice duration
mem_b CompositeQueue::slice_maxqueuesize(int slice){
    return _maxsize;
    assert(slice <= _dl_queue);
    if (slice == _dl_queue){
        return _maxsize;
    } else {
        //duration of slice * queue tx time
        return _ps_per_byte*_top->get_slicetime(slice%2);
    }
}

mem_b CompositeQueue::slice_queuesize_low(int slice){
    assert(slice <= _dl_queue);
    return _queuesize_low[slice]+_queuesize_low_prime[slice];
}
mem_b CompositeQueue::slice_queuesize_high(int slice){
    assert(slice <= _dl_queue);
    return _queuesize_high[slice]+_queuesize_high_prime[slice];
}
mem_b CompositeQueue::slice_queuesize(int slice){
    assert(slice <= _dl_queue);
    return _queuesize_high[slice]+_queuesize_low[slice]
        +_queuesize_high_prime[slice]+_queuesize_low_prime[slice];
}

mem_b CompositeQueue::queuesize() {
    int sum = 0;
    int i;
    for(i = 0; i < _top->get_nsuperslice()*2; i++){
        sum += _queuesize_high[i];
        sum += _queuesize_low[i];
        sum += _queuesize_high_prime[i];
        sum += _queuesize_low_prime[i];
    }
    return sum;
}

HopDelayForward::HopDelayForward(EventList &eventlist)
    : EventSource(eventlist, "HopDelayForward") {}

    void HopDelayForward::insertBouncedQ(simtime_picosec t, Packet* pkt) {
        pair<simtime_picosec, Packet*> item = make_pair(t, pkt);
        // YX: TODO: change to binary search with comparator
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

	pair<int, int> route;
	if (pkt->get_longflow()) {
		// YX: get_crtToR() should give you the first ToR only, please test
        //cout << "direct routing between " << pkt->get_crtToR() << " & " << top->get_firstToR(pkt->get_dst()) << endl;
		route = top->get_direct_routing(pkt->get_crtToR(), top->get_firstToR(pkt->get_dst()), slice);
	} else {
		route = top->get_routing(pkt->get_crtToR(), top->get_firstToR(pkt->get_dst()), slice);
	}
    
	int uplink = route.first;
	int sent_slice = route.second;

    //track max distance from current queue, short flows only
    if(!pkt->get_longflow() && (pkt->get_crtToR() != top->get_firstToR(pkt->get_dst()))){
        int diff = (((sent_slice-slice) + top->get_nsuperslice()*2) % (top->get_nsuperslice()*2))+1;
        //cout << slice << " " << sent_slice << " " << diff << endl;
        max_used_queues = diff > max_used_queues ? diff : max_used_queues;
    }

    // if this is the last ToR, need to get downlink port
    if(pkt->get_crtToR() == top->get_firstToR(pkt->get_dst())){
        pkt->set_crtport(top->get_lastport(pkt->get_dst()));
    } else {
        //FD uplink never seems to get changed so I just put this here
	    pkt->set_crtport(uplink);
    }
    pkt->set_crtslice(sent_slice);

    //cout << "hop " << pkt->get_crthop() << " crtToR " << pkt->get_crtToR() << " port " << pkt->get_crtport() << endl;
    Queue* q = top->get_queue_tor(pkt->get_crtToR(), uplink);
    CompositeQueue *cq = dynamic_cast<CompositeQueue*>(q);

    if (sent_slice == slice) {
        // YX: TODO: calculate delay considering the queue occupancy, same below
        int finish_slice = top->time_to_slice(t + cq->drainTime(pkt) + timeFromNs(delay_ToR2ToR) + cq->slice_queuesize_low(sent_slice)); // plus the link delay

		if (finish_slice == slice) {
			return (t + cq->drainTime(pkt));
		}
    cout << "Reroute\n";
		// May wrap around the cycle
		int slice_delta = (finish_slice - slice + top->get_nsuperslice() * 2)
			% (top->get_nsuperslice() * 2);

        if (slice_delta == 1) {
            if (top->get_nextToR(slice, pkt->get_crtToR(), uplink) ==
                    top->get_nextToR(finish_slice, pkt->get_crtToR(), uplink)) {
                return (t + cq->drainTime(pkt));
            }
            // Miss this slice, have to wait till the next slice
            int next_absolute_slice = top->time_to_absolute_slice(t) + 1;
            simtime_picosec next_time = top->get_slice_start_time(next_absolute_slice);
            return routing(pkt, next_time);
        }

        assert(slice_delta == 2); // Cannot be more than 2!

        int middle_slice = (slice + 1) % (top->get_nsuperslice() * 2);

		int next_tor1 = top->get_nextToR(slice, pkt->get_crtToR(), uplink);
		int next_tor2 = top->get_nextToR(middle_slice, pkt->get_crtToR(), uplink);
		int next_tor3 = top->get_nextToR(finish_slice, pkt->get_crtToR(), uplink);

		if ((next_tor1 == next_tor2) && (next_tor2 == next_tor3)) {
			return (t + cq->drainTime(pkt));
		}
		// Miss this slice, have to wait till the next slice
		int next_absolute_slice = top->time_to_absolute_slice(t) + 1;
		simtime_picosec next_time = top->get_slice_start_time(next_absolute_slice);
		return routing(pkt, next_time);
	} else {
		// Send at the beginning of the sent_slice
		int wait_slice = (sent_slice - slice + top->get_nsuperslice() * 2)
			% (top->get_nsuperslice() * 2);
		int sent_absolute_slice = top->time_to_absolute_slice(t) + wait_slice;
        return (top->get_slice_start_time(sent_absolute_slice) +
            cq->drainTime(pkt));
	}
}

QueueAlarm::QueueAlarm(EventList &eventlist, int port, CompositeQueue* q, DynExpTopology* top)
    : EventSource(eventlist, "QueueAlarm"), _queue(q), _top(top)
{
    //downlink queue uses no alarm
    if (top->is_downlink(port)){
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

    if(_queue->_serv == QUEUE_INVALID && _queue->slice_queuesize(crt) > 0){
        //cout << "queue slice " << crt << " alarm beginService" << endl;
        _queue->_crt_tx_slice = crt;
        _queue->beginService();
    }
    int next_absolute_slice = _top->time_to_absolute_slice(t) + 1;
    simtime_picosec next_time = _top->get_slice_start_time(next_absolute_slice);
    assert(next_time > t);
    eventlist().sourceIsPending(*this, next_time); 
}
