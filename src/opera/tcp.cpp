// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-        
#include "tcp.h"
//#include "mtcp.h"
#include "datacenter/dynexp_topology.h"
#include "ecn.h"
#include <iostream>
#include <algorithm>

#define KILL_THRESHOLD 5
////////////////////////////////////////////////////////////////
//  TCP SOURCE
////////////////////////////////////////////////////////////////

TcpSrc::TcpSrc(TcpLogger* logger, TrafficLogger* pktlogger, 
	       EventList &eventlist, DynExpTopology *top, int flow_src, int flow_dst)
    : EventSource(eventlist,"tcp"),  _logger(logger), _flow(pktlogger),
      _top(top), _flow_src(flow_src), _flow_dst(flow_dst)
{
    _mss = Packet::data_packet_size();
    _maxcwnd = 0xffffffff;//MAX_SENT*_mss;
    _sawtooth = 0;
    _subflow_id = -1;
    _rtt_avg = timeFromMs(0);
    _rtt_cum = timeFromMs(0);
    _base_rtt = timeInf;
    _cap = 0;
    _flow_size = ((uint64_t)1)<<63;
    _highest_sent = 0;
    _packets_sent = 0;
    _app_limited = -1;
    _established = false;
    _effcwnd = 0;

    //_ssthresh = 30000;
    _ssthresh = 0xffffffff;

#ifdef MODEL_RECEIVE_WINDOW
    _highest_data_seq = 0;
#endif

    _last_acked = 0;
    _last_ping = timeInf;
    _dupacks = 0;
    _rtt = 0;
    _rto = timeFromMs(3000);
    _mdev = 0;
    _recoverq = 0;
    _in_fast_recovery = false;
    //_mSrc = NULL;
    _drops = 0;

    //_old_route = NULL;
    _last_packet_with_old_route = 0;

    _rtx_timeout_pending = false;
    _RFC2988_RTO_timeout = timeInf;

    _nodename = "tcpsrc";
}

void TcpSrc::set_app_limit(int pktps) {
    if (_app_limited==0 && pktps){
	_cwnd = _mss;
    }
    _ssthresh = 0xffffffff;
    _app_limited = pktps;
    send_packets();
}

void 
TcpSrc::startflow() {
    //cout << "startflow()\n";
    _cwnd = 10*_mss;
    _unacked = _cwnd;
    _established = false;

    send_packets();
}

void TcpSrc::add_to_dropped(uint64_t seqno) {
    _dropped_at_queue.push_back(seqno);
}

bool TcpSrc::was_it_dropped(uint64_t seqno) {
    vector<uint64_t>::iterator it;
    it = find(_dropped_at_queue.begin(), _dropped_at_queue.end(), seqno);
    if (it != _dropped_at_queue.end()) {
        cout << "DROPPED\n";
        _dropped_at_queue.erase(it);
        return true;
    } else {
        return false;
    }
}
uint32_t TcpSrc::effective_window() {
    return _in_fast_recovery?_ssthresh:_cwnd;
}

void 
TcpSrc::connect(TcpSink& sink, 
		simtime_picosec starttime) {
    //_route = &routeout;

    //assert(_route);
    _sink = &sink;
    _flow.id = id; // identify the packet flow with the TCP source that generated it
    _sink->connect(*this);
    _start_time = starttime;
    cout << "Flow start " << _flow_src << " " << _flow_dst << " " << starttime << endl;

    //printf("Tcp %x msrc %x\n",this,_mSrc);
    eventlist().sourceIsPending(*this,starttime);
}

Queue* 
TcpSrc::sendToNIC(Packet* pkt) {
    DynExpTopology* top = pkt->get_topology();
    Queue* nic = top->get_queue_serv_tor(pkt->get_src()); // returns pointer to nic queue
    assert(nic);
    nic->receivePacket(*pkt); // send this packet to the nic queue
    return nic; // return pointer so NDP source can update send time
}

#define ABS(X) ((X)>0?(X):-(X))

void
TcpSrc::receivePacket(Packet& pkt) 
{
    simtime_picosec ts;
    TcpAck *p = (TcpAck*)(&pkt);
    TcpAck::seq_t seqno = p->ackno();
    //cout << "TcpSrc receive packet ackno:" << seqno << endl;

    //pkt.flow().logTraffic(pkt,*this,TrafficLogger::PKT_RCVDESTROY);

    ts = p->ts();
    p->free();

    if (seqno < _last_acked) {
        //cout << "O seqno" << seqno << " last acked "<< _last_acked;
        return;
    }

    if (seqno==1){
        //assert(!_established);
        _established = true;
    }
    else if (seqno>1 && !_established) {
        cout << "Should be _established " << seqno << endl;
    }

    //assert(seqno >= _last_acked);  // no dups or reordering allowed in this simple simulator

    //compute rtt
    uint64_t m = eventlist().now()-ts;

    if (m!=0){
        if (_rtt>0){
            uint64_t abs;
            if (m>_rtt)
                abs = m - _rtt;
            else
                abs = _rtt - m;

            _mdev = 3 * _mdev / 4 + abs/4;
            _rtt = 7*_rtt/8 + m/8;
            _rto = _rtt + 4*_mdev;
        } else {
            _rtt = m;
            _mdev = m/2;
            _rto = _rtt + 4*_mdev;
        }
        if (_base_rtt==timeInf || _base_rtt > m)
            _base_rtt = m;
    }
    //  cout << "Base "<<timeAsMs(_base_rtt)<< " RTT " << timeAsMs(_rtt)<< " Queued " << queued_packets << endl;

    if (_rto<timeFromMs(1))
        _rto = timeFromMs(1);

    if (seqno >= _flow_size && !_finished){
        cout << "FCT " << get_flow_src() << " " << get_flow_dst() << " " << get_flowsize() <<
            " " << timeAsMs(eventlist().now() - get_start_time()) << " " << fixed 
            << timeAsMs(get_start_time()) << " " << _found_reorder << " " << _found_retransmit << endl;
        if (_found_reorder == 0) assert(_found_retransmit == 0);
        /*
        if (_found_reorder == 0 && _found_retransmit != 0) {
            cout << "BAD\n";
        }
        if (get_flow_src() == 578 && get_flow_dst() == 163) {
            exit(1);
        }
        */
        _finished = true;
    }

    if (seqno > _last_acked) { // a brand new ack
        _RFC2988_RTO_timeout = eventlist().now() + _rto;// RFC 2988 5.3
        _last_ping = eventlist().now();

        if (seqno >= _highest_sent) {
            _highest_sent = seqno;
            _RFC2988_RTO_timeout = timeInf;// RFC 2988 5.2
            _last_ping = timeInf;
        }

#ifdef MODEL_RECEIVE_WINDOW
        int cnt;

        _sent_packets.ack_packet(seqno);

        //if ((cnt = _sent_packets.ack_packet(seqno)) > 2)
        //  cout << "ACK "<<cnt<<" packets on " << _flow.id << " " << _highest_sent+1 << " packets in flight " << _sent_packets.crt_count << " diff " << (_highest_sent+_mss-_last_acked)/1000 << " last_acked " << _last_acked << " at " << timeAsMs(eventlist().now()) << endl;
#endif

        if (!_in_fast_recovery) { // best behaviour: proper ack of a new packet, when we were expecting it
                                  //clear timers

            _last_acked = seqno;
            _dupacks = 0;
            inflate_window();

            if (_cwnd>_maxcwnd) {
                _cwnd = _maxcwnd;
            }

            _unacked = _cwnd;
            _effcwnd = _cwnd;
            if (_logger) 
                _logger->logTcp(*this, TcpLogger::TCP_RCV);
            send_packets();
            return;
        }
        // We're in fast recovery, i.e. one packet has been
        // dropped but we're pretending it's not serious
        if (seqno >= _recoverq) { 
            // got ACKs for all the "recovery window": resume
            // normal service
            uint32_t flightsize = _highest_sent - seqno;
            _cwnd = min(_ssthresh, flightsize + _mss);
            _unacked = _cwnd;
            _effcwnd = _cwnd;
            _last_acked = seqno;
            _dupacks = 0;
            _in_fast_recovery = false;

            if (_logger) 
                _logger->logTcp(*this, TcpLogger::TCP_RCV_FR_END);
            send_packets();
            return;
        }
        // In fast recovery, and still getting ACKs for the
        // "recovery window"
        // This is dangerous. It means that several packets
        // got lost, not just the one that triggered FR.
        uint32_t new_data = seqno - _last_acked;
        _last_acked = seqno;
        if (new_data < _cwnd) 
            _cwnd -= new_data; 
        else 
            _cwnd = 0;
        _cwnd += _mss;
        if (_logger) 
            _logger->logTcp(*this, TcpLogger::TCP_RCV_FR);
        retransmit_packet();
        send_packets();
        return;
    }
    // It's a dup ack
    if (_in_fast_recovery) { // still in fast recovery; hopefully the prodigal ACK is on it's way 
        _cwnd += _mss;
        if (_cwnd>_maxcwnd) {
            _cwnd = _maxcwnd;
        }
        // When we restart, the window will be set to
        // min(_ssthresh, flightsize+_mss), so keep track of
        // this
        _unacked = min(_ssthresh, (uint32_t)(_highest_sent-_recoverq+_mss)); 
        if (_last_acked+_cwnd >= _highest_sent+_mss) 
            _effcwnd=_unacked; // starting to send packets again
        if (_logger) 
            _logger->logTcp(*this, TcpLogger::TCP_RCV_DUP_FR);
        send_packets();
        return;
    }
    // Not yet in fast recovery. What should we do instead?
    _dupacks++;

        if (_dupacks!=3) 
        { // not yet serious worry
            /*
            if (_logger) 
                _logger->logTcp(*this, TcpLogger::TCP_RCV_DUP);
            */
            send_packets();
            return;
        }
    // _dupacks==3
    if (_last_acked < _recoverq) {  
        /* See RFC 3782: if we haven't recovered from timeouts
           etc. don't do fast recovery */
        /*
        if (_logger) 
            _logger->logTcp(*this, TcpLogger::TCP_RCV_3DUPNOFR);
        */
        return;
    }

    // begin fast recovery
    
    //only count drops in CA state
    _drops++;
    //print if retransmission is due to reordered packet (was not dropped)
    if (!was_it_dropped(_last_acked+1)) {
        cout << "RETRANSMIT " << _flow_src << " " << _flow_dst << " " << _flow_size  << " " << seqno << endl;
        _found_retransmit++;
    }

    deflate_window();

    if (_sawtooth>0)
        _rtt_avg = _rtt_cum/_sawtooth;
    else
        _rtt_avg = timeFromMs(0);

    _sawtooth = 0;
    _rtt_cum = timeFromMs(0);

    retransmit_packet();
    _cwnd = _ssthresh + 3 * _mss;
    _unacked = _ssthresh;
    _effcwnd = 0;
    _in_fast_recovery = true;
    _recoverq = _highest_sent; // _recoverq is the value of the
                               // first ACK that tells us things
                               // are back on track
    /*
       if (_logger) 
       _logger->logTcp(*this, TcpLogger::TCP_RCV_DUP_FASTXMIT);
       */
}

void TcpSrc::deflate_window(){
	_ssthresh = max(_cwnd/2, (uint32_t)(2 * _mss));
}

void
TcpSrc::inflate_window() {
    int newly_acked = (_last_acked + _cwnd) - _highest_sent;
    // be very conservative - possibly not the best we can do, but
    // the alternative has bad side effects.
    if (newly_acked > _mss) newly_acked = _mss; 
    if (newly_acked < 0)
        return;
    if (_cwnd < _ssthresh) { //slow start
        int increase = min(_ssthresh - _cwnd, (uint32_t)newly_acked);
        _cwnd += increase;
        newly_acked -= increase;
    } else {
        // additive increase
        uint32_t pkts = _cwnd/_mss;

        double queued_fraction = 1 - ((double)_base_rtt/_rtt);

        if (queued_fraction>=0.5&&_cap)
            return;

        _cwnd += (newly_acked * _mss) / _cwnd;  //XXX beware large windows, when this increase gets to be very small

        if (pkts!=_cwnd/_mss) {
            _rtt_cum += _rtt;
            _sawtooth ++;
        }
    }
}

// Note: the data sequence number is the number of Byte1 of the packet, not the last byte.
void 
TcpSrc::send_packets() {
    int c = _cwnd;

    if (!_established){
        //cout << "need to establish\n";
        //send SYN packet and wait for SYN/ACK
        TcpPacket * p  = TcpPacket::new_syn_pkt(_top, _flow, _flow_src, _flow_dst, this, _sink, 1, 1);
        assert(p->size() == 1);
        _highest_sent = 1;

        sendToNIC(p);

        if(_RFC2988_RTO_timeout == timeInf) {// RFC2988 5.1
            _RFC2988_RTO_timeout = eventlist().now() + _rto;
        }	
        //cout << "Sending SYN, waiting for SYN/ACK" << endl;
        return;
    }

    if (_app_limited >= 0 && _rtt > 0) {
        uint64_t d = (uint64_t)_app_limited * _rtt/1000000000;
        if (c > d) {
            c = d;
        }
        //if (c<1000)
        //c = 1000;

        if (c==0){
            //      _RFC2988_RTO_timeout = timeInf;
        }

        //rtt in ms
        //printf("%d\n",c);
    }

    while ((_last_acked + c >= _highest_sent + _mss) 
            && (_highest_sent < _flow_size)) {
        uint64_t data_seq = 0;

        uint16_t size = _highest_sent+_mss <= _flow_size ? _mss : _flow_size-_highest_sent+1;
        TcpPacket* p = TcpPacket::newpkt(_top, _flow, _flow_src, _flow_dst, this, _sink, _highest_sent+1, data_seq, size);
        //cout << "sending seqno:" << p->seqno() << endl;
        //p->flow().logTraffic(*p,*this,TrafficLogger::PKT_CREATESEND);
        p->set_ts(eventlist().now());

        _highest_sent += size;  //XX beware wrapping
        _packets_sent += size;

        sendToNIC(p);

        if(_RFC2988_RTO_timeout == timeInf) {// RFC2988 5.1
            _RFC2988_RTO_timeout = eventlist().now() + _rto;
        }
    }
    //cout << "stopped sending last_acked " << _last_acked << " _highest_sent " << _highest_sent << " c " << c << endl;
}

void 
TcpSrc::retransmit_packet() {
    if (!_established){
        assert(_highest_sent == 1);

        TcpPacket* p  = TcpPacket::new_syn_pkt(_top, _flow, _flow_src, _flow_dst, this, _sink, 1, 1);
        sendToNIC(p);

        cout << "Resending SYN, waiting for SYN/ACK" << endl;
        return;	
    }

    uint64_t data_seq = 0;

    TcpPacket* p = TcpPacket::newpkt(_top, _flow, _flow_src, _flow_dst, this, _sink, _last_acked+1, data_seq, _pkt_size);

    //p->flow().logTraffic(*p,*this,TrafficLogger::PKT_CREATESEND);
    p->set_ts(eventlist().now());
    sendToNIC(p);

    _packets_sent += _mss;

    if(_RFC2988_RTO_timeout == timeInf) {// RFC2988 5.1
        _RFC2988_RTO_timeout = eventlist().now() + _rto;
    }
}

void TcpSrc::rtx_timer_hook(simtime_picosec now, simtime_picosec period) {
    if (now <= _RFC2988_RTO_timeout || _RFC2988_RTO_timeout==timeInf || _finished) 
        return;

    if (_highest_sent == 0) 
        return;

    cout <<"At " << now/(double)1000000000<< " RTO " << _rto/1000000000 << " MDEV " 
        << _mdev/1000000000 << " RTT "<< _rtt/1000000000 << " SEQ " << _last_acked / _mss << " HSENT "  << _highest_sent 
        << " CWND "<< _cwnd/_mss << " FAST RECOVERY? " << 	_in_fast_recovery << " Flow ID " 
        << str()  << endl;

    // here we can run into phase effects because the timer is checked
    // only periodically for ALL flows but if we keep the difference
    // between scanning time and real timeout time when restarting the
    // flows we should minimize them !
    if(!_rtx_timeout_pending) {
        _rtx_timeout_pending = true;

        // check the timer difference between the event and the real value
        simtime_picosec too_late = now - (_RFC2988_RTO_timeout);

        // careful: we might calculate a negative value if _rto suddenly drops very much
        // to prevent overflow but keep randomness we just divide until we are within the limit
        while(too_late > period) too_late >>= 1;

        // carry over the difference for restarting
        simtime_picosec rtx_off = (period - too_late)/200;

        eventlist().sourceIsPendingRel(*this, rtx_off);

        //reset our rtx timerRFC 2988 5.5 & 5.6

        _rto *= 2;
        //if (_rto > timeFromMs(1000))
        //  _rto = timeFromMs(1000);
        _RFC2988_RTO_timeout = now + _rto;
    }
}

void TcpSrc::doNextEvent() {
    if(_rtx_timeout_pending) {
	_rtx_timeout_pending = false;

	if (_logger) 
	    _logger->logTcp(*this, TcpLogger::TCP_TIMEOUT);

	if (_in_fast_recovery) {
	    uint32_t flightsize = _highest_sent - _last_acked;
	    _cwnd = min(_ssthresh, flightsize + _mss);
	}

	deflate_window();

	_cwnd = _mss;

	_unacked = _cwnd;
	_effcwnd = _cwnd;
	_in_fast_recovery = false;
	_recoverq = _highest_sent;

	if (_established)
	    _highest_sent = _last_acked + _mss;

	_dupacks = 0;

	retransmit_packet();

	if (_sawtooth>0)
	    _rtt_avg = _rtt_cum/_sawtooth;
	else
	    _rtt_avg = timeFromMs(0);

	_sawtooth = 0;
	_rtt_cum = timeFromMs(0);
    } else {
	startflow();
    }
}

////////////////////////////////////////////////////////////////
//  TCP SINK
////////////////////////////////////////////////////////////////

TcpSink::TcpSink() 
    : Logged("sink"), _cumulative_ack(0) , _packets(0), _crt_path(0)
{
    _nodename = "tcpsink";
}

void 
TcpSink::connect(TcpSrc& src) {
    _src = &src;
    _cumulative_ack = 0;
    _drops = 0;
}

// Note: _cumulative_ack is the last byte we've ACKed.
// seqno is the first byte of the new packet.
void
TcpSink::receivePacket(Packet& pkt) {
    //cout << "TcpSink receivePacket" << endl;
    TcpPacket *p = (TcpPacket*)(&pkt);
    TcpPacket::seq_t seqno = p->seqno();
    simtime_picosec ts = p->ts();
    simtime_picosec fts = p->get_fabricts();

    bool marked = p->flags()&ECN_CE;
    
    int size = p->size(); // TODO: the following code assumes all packets are the same size
    //pkt.flow().logTraffic(pkt,*this,TrafficLogger::PKT_RCVDESTROY);
    if (last_ts > fts){
        cout << "REORDER " << " " << _src->get_flow_src()<< " " << _src->get_flow_dst() << " "
            << _src->get_flowsize() << " " << 
            "EARLY " << last_ts << " " << last_hops << " " << last_queueing << " " 
            "LATE " << ts << " " << p->get_crthop() << " " << p->get_queueing() << endl;
        _src->_found_reorder++;
    }
    last_ts = fts;
    last_hops = p->get_crthop();
    last_queueing = p->get_queueing();
    /*
    if (p->get_src() == 578 && p->get_dst() == 163 && seqno == 2873+1) {
        cout << "RECVD " << p->ts()/1E6 << endl;
    }
    */
    p->free();

    _packets+= p->size();

    //cout << "Sink recv seqno " << seqno << " size " << size << endl;

    if (seqno == _cumulative_ack+1) { // it's the next expected seq no
	_cumulative_ack = seqno + size - 1;
	//cout << "New cumulative ack is " << _cumulative_ack << endl;
	// are there any additional received packets we can now ack?
	while (!_received.empty() && (_received.front() == _cumulative_ack+1) ) {
	    _received.pop_front();
	    _cumulative_ack+= size;
	}
    } else if (seqno < _cumulative_ack+1) {
    } else { // it's not the next expected sequence number
        /*
        if(_src->get_flow_src() == 578 && _src->get_flow_dst() == 163 && _cumulative_ack+1 == 2873+1) {
            cout << "EXPECTING 2874 GOT " << seqno << " " << ts/1E6 << endl;
        }
        */
	if (_received.empty()) {
	    _received.push_front(seqno);
	    //it's a drop in this simulator there are no reorderings.
	    _drops += (1000 + seqno-_cumulative_ack-1)/1000;
	} else if (seqno > _received.back()) { // likely case
	    _received.push_back(seqno);
	} else { // uncommon case - it fills a hole
	    list<uint64_t>::iterator i;
	    for (i = _received.begin(); i != _received.end(); i++) {
		if (seqno == *i) break; // it's a bad retransmit
		if (seqno < (*i)) {
		    _received.insert(i, seqno);
		    break;
		}
	    }
	}
    }
    send_ack(ts,marked);
}

Queue* 
TcpSink::sendToNIC(Packet* pkt) {
    //cout << "sendToNIC " << pkt->get_src() << " " << pkt->get_dst() << endl;
    DynExpTopology* top = pkt->get_topology();
    Queue* nic = top->get_queue_serv_tor(pkt->get_src()); // returns pointer to nic queue
    assert(nic);
    nic->receivePacket(*pkt); // send this packet to the nic queue
    return nic; // return pointer so NDP source can update send time
}

void 
TcpSink::send_ack(simtime_picosec ts,bool marked) {
    //terribly ugly but that's how opera people made it...
    //just use the previous tcpsrc as a source, packet will get routed based
    //on the inverted src/sink ids and then be received by the source at the end
    TcpAck *ack = TcpAck::newpkt(_src->_top, _src->_flow, _src->_flow_dst, _src->_flow_src, 
            _src, 0, 0, _cumulative_ack, 0);

    //ack->flow().logTraffic(*ack,*this,TrafficLogger::PKT_CREATESEND);
    ack->set_ts(ts);
    if (marked) 
        ack->set_flags(ECN_ECHO);
    else
        ack->set_flags(0);

    sendToNIC(ack);
}

////////////////////////////////////////////////////////////////
//  TCP RETRANSMISSION TIMER
////////////////////////////////////////////////////////////////

TcpRtxTimerScanner::TcpRtxTimerScanner(simtime_picosec scanPeriod, EventList& eventlist)
    : EventSource(eventlist,"RtxScanner"), _scanPeriod(scanPeriod) {
    eventlist.sourceIsPendingRel(*this, _scanPeriod);
}

void 
TcpRtxTimerScanner::registerTcp(TcpSrc &tcpsrc) {
    _tcps.push_back(&tcpsrc);
}

void TcpRtxTimerScanner::doNextEvent() {
    simtime_picosec now = eventlist().now();
    tcps_t::iterator i;
    for (i = _tcps.begin(); i!=_tcps.end(); i++) {
	(*i)->rtx_timer_hook(now,_scanPeriod);
    }
    eventlist().sourceIsPendingRel(*this, _scanPeriod);
}
