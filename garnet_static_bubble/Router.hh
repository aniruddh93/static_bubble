/*
 * Copyright (c) 2008 Princeton University
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors: Niket Agarwal
 *          Aniruddh Ramrakhyani (static bubble implementation)
 */

#ifndef __MEM_RUBY_NETWORK_GARNET_FIXED_PIPELINE_ROUTER_D_HH__
#define __MEM_RUBY_NETWORK_GARNET_FIXED_PIPELINE_ROUTER_D_HH__


#include <iostream>
#include <vector>

#include "mem/ruby/common/Consumer.hh"
#include "mem/ruby/common/NetDest.hh"
#include "mem/ruby/network/BasicRouter.hh"
#include "mem/ruby/network/garnet2.0/CommonTypes.hh"
#include "mem/ruby/network/garnet2.0/GarnetNetwork.hh"
#include "mem/ruby/network/garnet2.0/flit.hh"
#include "params/GarnetRouter.hh"

class NetworkLink;
class CreditLink;
class InputUnit;
class OutputUnit;
class RoutingUnit;
class SwitchAllocator;
class CrossbarSwitch;
class FaultModel;
class flitBuffer;
class flit;

class Router : public BasicRouter, public Consumer
{
  public:
    typedef GarnetRouterParams Params;
    Router(const Params *p);

    ~Router();

    void wakeup();
    void print(std::ostream& out) const {};

    void init();
    void addInPort(PortDirection inport_dirn, NetworkLink *link, CreditLink *credit_link);
    void addOutPort(PortDirection outport_dirn, NetworkLink *link,
                    const NetDest& routing_table_entry,
                    int link_weight, CreditLink *credit_link);

    int get_num_vcs()       { return m_num_vcs; }
    int get_num_vnets()     { return m_virtual_networks; }
    int get_vc_per_vnet()   { return m_vc_per_vnet; }
    int get_num_inports()   { return m_input_unit.size(); }
    int get_num_outports()  { return m_output_unit.size(); }
    int get_id()            { return m_id; }

    void init_net_ptr(GarnetNetwork* net_ptr)
    {
        m_network_ptr = net_ptr;
    }

  //static bubble scheme
  void set_counter(unsigned counter_count, PortDirection counter_vc_port, int counter_vc_number, counter_state cntr_state, counter_threshold thresh);
  counter_state get_counter_state() { return m_counter->state; }
  bool is_static_bubble_node() { return m_is_static_bubble_node; }
  bool is_deadlock() { return m_is_deadlock; }
  void set_is_deadlock(bool is_deadlock) { m_is_deadlock = is_deadlock; }
  int get_io_pbuffer_node_id() { return m_io_priority_buffer->node_id; }
  int get_io_pbuffer_inport() { return m_io_priority_buffer->inport; }
  int get_io_pbuffer_outport() { return m_io_priority_buffer->outport; }
  int get_io_pbuffer_vnet() { return m_io_priority_buffer->vnet; }
  void increment_counter();
  bool check_counter();
  void send_probe();
  void send_disable();
  void send_enable();
  flitBuffer* getProbeQueue();
  flitBuffer* getDisableQueue();
  flitBuffer* getEnableQueue();
  flitBuffer* getCheck_probeQueue();
  RoutingUnit* getRoutingUnit_ref() {return m_routing_unit; }
  void latch_deadlock_path(flit *probe); 
  PortDirection get_counter_direction() { return m_counter->vc_port_direction; }
  int get_counter_vc_num() { return m_counter->vc_num; }
  void set_IO_priority_buffer(int node_id, int vnet, PortDirection input_port_dirn, int inport, PortDirection output_port_dirn, int outport);
  void copy_turns(flit *flt);
  std::pair<PortDirection, int> get_cntr_next_pointer();
  void sb_switched_off_msg();
  void switch_off_sb();
  int get_prev_router_id();
  void process_disable_msg(flit *flt);
  void process_enable_msg(flit *flt);
  void send_check_probe();

 
    GarnetNetwork* get_net_ptr()                    { return m_network_ptr; }
    std::vector<InputUnit *>& get_inputUnit_ref()   { return m_input_unit; }
    std::vector<OutputUnit *>& get_outputUnit_ref() { return m_output_unit; }
    PortDirection getOutportDirection(int outport);
    PortDirection getInportDirection(int inport);

  int route_compute(RouteInfo route, int inport, PortDirection direction, int vc);
    void grant_switch(int inport, flit *t_flit);
    void schedule_wakeup(Cycles time);

    std::string getPortDirectionName(PortDirection direction);
    void printFaultVector(std::ostream& out);
    void printAggregateFaultProbability(std::ostream& out);

    void regStats();
    void collateStats();
    void resetStats();

    bool get_fault_vector(int temperature, float fault_vector[]){
        return m_network_ptr->fault_model->fault_vector(m_id, temperature,
                                                        fault_vector);
    }
    bool get_aggregate_fault_probability(int temperature,
                                         float *aggregate_fault_prob){
        return m_network_ptr->fault_model->fault_prob(m_id, temperature,
                                                      aggregate_fault_prob);
    }

    uint32_t functionalWrite(Packet *);


//static bubble scheme
struct counter
{
  unsigned  m_counter_count;
  
  //variables for implemeting a round robin counter
  PortDirection vc_port_direction;
  int vc_num;
  counter_state state;
  counter_threshold threshold;
};

struct IO_priority_buffer
{
  int node_id;
  int vnet;
  PortDirection input_port_dirn;
  int inport;
  PortDirection output_port_dirn;
  int outport;
};

  private:
    int m_virtual_networks, m_num_vcs, m_vc_per_vnet;
    GarnetNetwork *m_network_ptr;

    std::vector<InputUnit *> m_input_unit;
    std::vector<OutputUnit *> m_output_unit;
    RoutingUnit *m_routing_unit;
    SwitchAllocator *m_sw_alloc;
    CrossbarSwitch *m_switch;

  //variables for static bubble scheme

  
  bool m_is_deadlock;

  IO_priority_buffer *m_io_priority_buffer;
  bool m_static_bubble_on;
  std::vector<turn> m_deadlock_path;
  bool m_direction_bit_buffer_valid;
  bool m_is_static_bubble_node;
  counter *m_counter;

  //queues for enable, disable and probe msgs
  flitBuffer *probeQueue;
  flitBuffer *disableQueue;
  flitBuffer *enableQueue;
  flitBuffer *check_probeQueue;

  // end of static bubble variable declaration 

    // Statistical variables required for power computations
    Stats::Scalar m_buffer_reads;
    Stats::Scalar m_buffer_writes;

    Stats::Scalar m_sw_input_arbiter_activity;
    Stats::Scalar m_sw_output_arbiter_activity;

    Stats::Scalar m_crossbar_activity;
};

#endif // __MEM_RUBY_NETWORK_GARNET_FIXED_PIPELINE_ROUTER_D_HH__
