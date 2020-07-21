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
 *          Tushar Krishna
 *          Aniruddh Ramrakhyani (static bubble implementation)
 */

#ifndef __MEM_RUBY_NETWORK_GARNET_FIXED_PIPELINE_GARNETNETWORK_D_HH__
#define __MEM_RUBY_NETWORK_GARNET_FIXED_PIPELINE_GARNETNETWORK_D_HH__

#include <iostream>
#include <vector>

#include "mem/ruby/network/Network.hh"
#include "mem/ruby/network/fault_model/FaultModel.hh"
#include "mem/ruby/network/garnet2.0/CommonTypes.hh"
#include "params/GarnetNetwork.hh"

class FaultModel;
class NetworkInterface;
class Router;
class NetDest;
class NetworkLink;
class CreditLink;

class GarnetNetwork : public Network
{
  public:
    typedef GarnetNetworkParams Params;
    GarnetNetwork(const Params *p);

    ~GarnetNetwork();
    void init();

    // Configuration (set externally)

    // for 2D topology
  int getNumRows() const { return m_num_rows; }
  int getNumCols() { return m_num_cols; }

    // for network
    uint32_t getNiFlitSize() const { return m_ni_flit_size; }
    uint32_t getNumPipeStages() const { return m_num_pipe_stages; }
    uint32_t getVCsPerVnet() const { return m_vcs_per_vnet; }
    uint32_t getBuffersPerDataVC() { return m_buffers_per_data_vc; }
    uint32_t getBuffersPerCtrlVC() { return m_buffers_per_ctrl_vc; }
    int getRoutingAlgorithm() const { return m_routing_algorithm; }

  //Escape VC getter function
  int getEscapeVC() {return m_enable_escape_vc;}

  //static bubble scheme
  bool getStaticBubbleSchemeEnabled() { return (m_static_bubble_scheme_enabled == 1); }
  bool getDisableCheckEnabled() { return (m_disable_check_enabled == 1); }

  PortDirection get_turn2outport_dirn(PortDirection m_dir, turn t);
  turn get_outport_dirn2turn(PortDirection m_dir, PortDirection dir);
  
  PortDirection get_west_port_turn2outport_dirn( turn t) { return  west_port_turn2outport_dirn[t]; }
  turn get_west_port_outport_dirn2turn(PortDirection dir) { return west_port_outport_dirn2turn[dir]; }

  PortDirection get_east_port_turn2outport_dirn(turn t) { return east_port_turn2outport_dirn[t]; }
  turn get_east_port_outport_dirn2turn(PortDirection dir) { return east_port_outport_dirn2turn[dir];}

  PortDirection get_north_port_turn2outport_dirn(turn t) { return north_port_turn2outport_dirn[t]; }
  turn get_north_port_outport_dirn2turn(PortDirection dir) {return north_port_outport_dirn2turn[dir]; }

  PortDirection get_south_port_turn2outport_dirn(turn t) { return south_port_turn2outport_dirn[t]; }
  turn get_south_port_outport_dirn2turn(PortDirection dir) { return south_port_outport_dirn2turn[dir]; }


    bool isFaultModelEnabled() const { return m_enable_fault_model; }
    FaultModel* fault_model;


    // Internal configuration
    bool isVNetOrdered(int vnet) const { return m_ordered[vnet]; }
    VNET_type
    get_vnet_type(int vc)
    {
        int vnet = vc/getVCsPerVnet();
        return m_vnet_type[vnet];
    }
    int getNumRouters();
    int get_router_id(int ni);


    // Methods used by Topology to setup the network
    void makeOutLink(SwitchID src, NodeID dest, BasicLink* link,
                     LinkDirection direction,
                     const NetDest& routing_table_entry);
    void makeInLink(NodeID src, SwitchID dest, BasicLink* link,
                    LinkDirection direction,
                    const NetDest& routing_table_entry);
    void makeInternalLink(SwitchID src, SwitchID dest,
                          PortDirection src_outport_dirn,
                          PortDirection dest_inport_dirn,
                          BasicLink* link,
                          LinkDirection direction,
                          const NetDest& routing_table_entry);

    //! Function for performing a functional write. The return value
    //! indicates the number of messages that were written.
    uint32_t functionalWrite(Packet *pkt);

    // Stats
    void collateStats();
    void regStats();
    void print(std::ostream& out) const;

    // increment counters
    void increment_injected_packets(int vnet) { m_packets_injected[vnet]++; }
    void increment_received_packets(int vnet) { m_packets_received[vnet]++; }

    void
    increment_packet_network_latency(Cycles latency, int vnet)
    {
        m_packet_network_latency[vnet] += latency;
    }

    void
    increment_packet_queueing_latency(Cycles latency, int vnet)
    {
        m_packet_queueing_latency[vnet] += latency;
    }

    void increment_injected_flits(int vnet) { m_flits_injected[vnet]++; }
    void increment_received_flits(int vnet) { m_flits_received[vnet]++; }

    void
    increment_flit_network_latency(Cycles latency, int vnet)
    {
        m_flit_network_latency[vnet] += latency;
    }

    void
    increment_flit_queueing_latency(Cycles latency, int vnet)
    {
        m_flit_queueing_latency[vnet] += latency;
    }

    void
    increment_total_hops(int hops)
    {
        m_total_hops += hops;
    }

  protected:
    // Configuration
    int m_num_rows;
    int m_num_cols;
    uint32_t m_ni_flit_size;
    uint32_t m_num_pipe_stages;
    uint32_t m_vcs_per_vnet;
    uint32_t m_buffers_per_ctrl_vc;
    uint32_t m_buffers_per_data_vc;
    int m_routing_algorithm;
    bool m_enable_fault_model;

  //For Escape VC 
    int m_enable_escape_vc;

  //For static bubble scheme: variable to store if enabled
    int m_static_bubble_scheme_enabled;
    int m_disable_check_enabled;

    // Statistical variables
    Stats::Vector m_packets_received;
    Stats::Vector m_packets_injected;
    Stats::Vector m_packet_network_latency;
    Stats::Vector m_packet_queueing_latency;

    Stats::Formula m_avg_packet_vnet_latency;
    Stats::Formula m_avg_packet_vqueue_latency;
    Stats::Formula m_avg_packet_network_latency;
    Stats::Formula m_avg_packet_queueing_latency;
    Stats::Formula m_avg_packet_latency;

    Stats::Vector m_flits_received;
    Stats::Vector m_flits_injected;
    Stats::Vector m_flit_network_latency;
    Stats::Vector m_flit_queueing_latency;

    Stats::Formula m_avg_flit_vnet_latency;
    Stats::Formula m_avg_flit_vqueue_latency;
    Stats::Formula m_avg_flit_network_latency;
    Stats::Formula m_avg_flit_queueing_latency;
    Stats::Formula m_avg_flit_latency;

    Stats::Scalar m_average_link_utilization;
    Stats::Vector m_average_vc_load;

    Stats::Scalar  m_total_hops;
    Stats::Formula m_avg_hops;

  private:
    GarnetNetwork(const GarnetNetwork& obj);
    GarnetNetwork& operator=(const GarnetNetwork& obj);

    std::vector<VNET_type > m_vnet_type;
    std::vector<Router *> m_routers;   // All Routers in Network
    std::vector<NetworkLink *> m_networklinks; // All network (flit) links in the network
    std::vector<CreditLink *> m_creditlinks; // All credit links in the network
    std::vector<NetworkInterface *> m_nis;   // All NI's in Network

  //static bubble scheme
    
  std::map<turn, PortDirection> west_port_turn2outport_dirn;
  std::map<PortDirection, turn> west_port_outport_dirn2turn;

  std::map<turn, PortDirection> east_port_turn2outport_dirn;
  std::map<PortDirection, turn> east_port_outport_dirn2turn;

  std::map<turn, PortDirection> north_port_turn2outport_dirn;
  std::map<PortDirection, turn> north_port_outport_dirn2turn;

  std::map<turn, PortDirection> south_port_turn2outport_dirn;
  std::map<PortDirection, turn> south_port_outport_dirn2turn;
};

inline std::ostream&
operator<<(std::ostream& out, const GarnetNetwork& obj)
{
    obj.print(out);
    out << std::flush;
    return out;
}

#endif // __MEM_RUBY_NETWORK_GARNET_FIXED_PIPELINE_GARNETNETWORK_D_HH__
