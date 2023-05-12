// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-
#ifndef COMPOSITE_QUEUE_H
#define COMPOSITE_QUEUE_H
#ifndef CALENDAR_QUEUE
#define CALENDAR_QUEUE
#endif

/*
 * A composite queue that transforms packets into headers when there is no space and services headers with priority.
 */

// !!! NOTE: this one does selective RLB packet dropping.

// !!! NOTE: this has been modified to also include a lower priority RLB queue


#define QUEUE_INVALID 0
#define QUEUE_LOW_PRIME 1 // modified
#define QUEUE_HIGH_PRIME 2 // modified
#define QUEUE_LOW 3
#define QUEUE_HIGH 4


#include <list>
#include "queue.h"
#include "config.h"
#include "eventlist.h"
#include "network.h"
#include "loggertypes.h"
#include "hoho_utils.h"

class CompositeQueue;

// A util class
class HopDelayForward : public EventSource {
 public:
   HopDelayForward(EventList &eventlist);
   void bouncePkt(Packet* pkt);
   void doNextEvent();
   //simtime_picosec routing(Packet* pkt, simtime_picosec t);
   void insertBouncedQ(simtime_picosec t, Packet* pkt);
 private:
   vector<pair<simtime_picosec, Packet*>> _bounced_pkts; // sorted with decreasing timestamp, pop from back
};

class CompositeQueue : public Queue {
 public:
    CompositeQueue(linkspeed_bps bitrate, mem_b maxsize,
		   EventList &eventlist, QueueLogger* logger, DynExpTopology* topology, int tor, int port);
    virtual void receivePacket(Packet& pkt);
    virtual void doNextEvent();
    // should really be private, but loggers want to see
    vector<mem_b> _queuesize_low, _queuesize_high, _queuesize_low_prime, _queuesize_high_prime;
    int num_headers() const { return _num_headers;}
    int num_packets() const { return _num_packets;}
    int num_stripped() const { return _num_stripped;}
    int num_bounced() const { return _num_bounced;}
    int num_acks() const { return _num_acks;}
    int num_nacks() const { return _num_nacks;}
    int num_pulls() const { return _num_pulls;}
    virtual mem_b queuesize();
    mem_b queuesize_short();
    mem_b queuesize_short_data();
    mem_b queuesize_short_ctl();
    mem_b slice_queuesize_low(int slice);
    mem_b slice_queuesize_high(int slice);
    mem_b slice_queuesize(int slice);
    mem_b slice_maxqueuesize(int slice);
    virtual void setName(const string& name) {
	Logged::setName(name);
	_nodename += name;
    }
    virtual const string& nodename() { return _nodename; }

    int _num_packets;
    int _num_headers; // only includes data packets stripped to headers, not acks or nacks
    int _num_acks;
    int _num_nacks;
    int _num_pulls;
    int _num_stripped; // count of packets we stripped
    int _num_bounced;  // count of packets we bounced
    
    //queue occupancy logging
    int _max_tot_qsize;
    vector<int> _max_slice_qsize;
    
 protected:
    // Mechanism
    void beginService(); // start serving the item at the head of the queue
    void completeService(); // wrap up serving the item at the head of the queue
    int next_tx_slice(int crt_slice);
    simtime_picosec get_queueing_delay(int slice);
    void clearSliceQueue(int slice); // clear calendar queue in slice

    int _serv;
    int _ratio_high, _ratio_low, _ratio_high_prime, _ratio_low_prime, _crt;
    int _crt_send_ratio_high, _crt_send_ratio_low, _crt_trim_ratio;

    vector<list<Packet*>> _enqueued_low;
    vector<list<Packet*>> _enqueued_high;
    vector<list<Packet*>> _enqueued_low_prime;
    vector<list<Packet*>> _enqueued_high_prime;

    Packet* _sending_pkt = NULL;

    list<Packet*> _enqueued_rlb; // rlb queue

  private:
    // Must sum up to 1.0, can be tuned
    double _shortflow_bound = 0.6;
    double _longflow_bound = 0.4;

    HopDelayForward _hop_delay_forward;
    //for events
    friend class HopDelayForward;
};

#endif
