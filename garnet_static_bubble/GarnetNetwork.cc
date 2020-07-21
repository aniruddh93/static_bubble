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

#include "mem/ruby/network/garnet2.0/GarnetNetwork.hh"

#include <cassert>

#include "base/cast.hh"
#include "base/stl_helpers.hh"
#include "mem/ruby/common/NetDest.hh"
#include "mem/ruby/network/MessageBuffer.hh"
#include "mem/ruby/network/garnet2.0/CreditLink.hh"
#include "mem/ruby/network/garnet2.0/GarnetLink.hh"
#include "mem/ruby/network/garnet2.0/NetworkInterface.hh"
#include "mem/ruby/network/garnet2.0/NetworkLink.hh"
#include "mem/ruby/network/garnet2.0/Router.hh"
#include "mem/ruby/system/RubySystem.hh"

using namespace std;
using m5::stl_helpers::deletePointers;

GarnetNetwork::GarnetNetwork(const Params *p)
    : Network(p)
{

    m_num_rows = p->num_rows;
    m_ni_flit_size = p->ni_flit_size;
    m_num_pipe_stages = p->num_pipe_stages;
    m_vcs_per_vnet = p->vcs_per_vnet;
    m_buffers_per_data_vc = p->buffers_per_data_vc;
    m_buffers_per_ctrl_vc = p->buffers_per_ctrl_vc;
    m_routing_algorithm = p->routing_algorithm;

    //Escape VC
    m_enable_escape_vc=p->enable_escape_vc;

    //static bubble
     m_static_bubble_scheme_enabled=p->enable_static_bubble;
     m_disable_check_enabled = p->enable_disable_check;

     // cout<<"\nll: "<<m_static_bubble_scheme_enabled<<"\n";

    m_enable_fault_model = p->enable_fault_model;
    if (m_enable_fault_model)
        fault_model = p->fault_model;

    m_vnet_type.resize(m_virtual_networks);

    for(int i = 0 ; i < m_virtual_networks ; i++)
    {
        if (m_vnet_type_names[i] == "response")
            m_vnet_type[i] = DATA_VNET_; // carries data (and ctrl) packets
        else
            m_vnet_type[i] = CTRL_VNET_; // carries only ctrl packets
    }

    // record the routers
    for (vector<BasicRouter*>::const_iterator i =  p->routers.begin();i != p->routers.end(); ++i)
      {
        Router* router = safe_cast<Router*>(*i);
        m_routers.push_back(router);

        // initialize the router's network pointers
        router->init_net_ptr(this);

      }

    // record the network interfaces
    for (vector<ClockedObject*>::const_iterator i = p->netifs.begin();
         i != p->netifs.end(); ++i) {
        NetworkInterface *ni = safe_cast<NetworkInterface *>(*i);
        m_nis.push_back(ni);
        ni->init_net_ptr(this);
    }
}

void
GarnetNetwork::init()
{
    Network::init();

    for (int i=0; i < m_nodes; i++) {
        m_nis[i]->addNode(m_toNetQueues[i], m_fromNetQueues[i]);
    }

    // The topology pointer should have already been initialized in the
    // parent network constructor
    assert(m_topology_ptr != NULL);
    m_topology_ptr->createLinks(this);

    // Initialize topology specific parameters
    if (getNumRows() > 0)
    {
        // 2D topology (mesh/torus) ...
        m_num_rows = getNumRows();
        m_num_cols = m_routers.size() / m_num_rows;
        assert(m_num_rows * m_num_cols == m_routers.size());

	//initialize the turn-outport_dirn maps : static bubble scheme
	
	west_port_turn2outport_dirn[LEFT] = N_ ;
	west_port_turn2outport_dirn[RIGHT] = S_ ;
	west_port_turn2outport_dirn[STRAIGHT] = E_ ;

	west_port_outport_dirn2turn[N_] = LEFT ;
	west_port_outport_dirn2turn[E_] = STRAIGHT ;
	west_port_outport_dirn2turn[S_] = RIGHT ;


	east_port_turn2outport_dirn[STRAIGHT] = W_;
	east_port_turn2outport_dirn[LEFT] = S_;
	east_port_turn2outport_dirn[RIGHT] = N_;

	east_port_outport_dirn2turn[W_] = STRAIGHT;
	east_port_outport_dirn2turn[N_] = RIGHT;
	east_port_outport_dirn2turn[S_] = LEFT;


	north_port_turn2outport_dirn[LEFT] = E_;
	north_port_turn2outport_dirn[STRAIGHT] = S_;
	north_port_turn2outport_dirn[RIGHT] = W_;

	north_port_outport_dirn2turn[E_] = LEFT;
	north_port_outport_dirn2turn[S_] = STRAIGHT;
	north_port_outport_dirn2turn[W_] = RIGHT;


	south_port_turn2outport_dirn[LEFT] = W_;
	south_port_turn2outport_dirn[RIGHT] = E_;
	south_port_turn2outport_dirn[STRAIGHT] = N_;

	south_port_outport_dirn2turn[W_] = LEFT;
	south_port_outport_dirn2turn[E_] = RIGHT;
	south_port_outport_dirn2turn[N_] = STRAIGHT;

    }
    else
    {
        m_num_rows = -1;
        m_num_cols = -1;
    }

    // FaultModel: declare each router to the fault model
    if(isFaultModelEnabled()){
        for (vector<Router*>::const_iterator i= m_routers.begin();
             i != m_routers.end(); ++i) {
            Router* router = safe_cast<Router*>(*i);
            int router_id M5_VAR_USED =
                fault_model->declare_router(router->get_num_inports(),
                                            router->get_num_outports(),
                                            router->get_vc_per_vnet(),
                                            getBuffersPerDataVC(),
                                            getBuffersPerCtrlVC());
            assert(router_id == router->get_id());
            router->printAggregateFaultProbability(cout);
            router->printFaultVector(cout);
        }
    }
}

GarnetNetwork::~GarnetNetwork()
{
    deletePointers(m_routers);
    deletePointers(m_nis);
    deletePointers(m_networklinks);
    deletePointers(m_creditlinks);
}

/*
 * This function creates a link from the Network Interface (NI)
 * into the Network.
 * It creates a Network Link from the NI to a Router and a Credit Link from
 * the Router to the NI
*/

void
GarnetNetwork::makeInLink(NodeID src, SwitchID dest, BasicLink* link,
                            LinkDirection direction,
                            const NetDest& routing_table_entry)
{
    assert(src < m_nodes);

    GarnetExtLink* garnet_link = safe_cast<GarnetExtLink*>(link);
    NetworkLink* net_link = garnet_link->m_network_links[direction];
    CreditLink* credit_link = garnet_link->m_credit_links[direction];

    m_networklinks.push_back(net_link);
    m_creditlinks.push_back(credit_link);

    PortDirection dest_inport_dirn = L_;
    m_routers[dest]->addInPort(dest_inport_dirn, net_link, credit_link);
    m_nis[src]->addOutPort(net_link, credit_link, dest);
}

/*
 * This function creates a link from the Network to a NI.
 * It creates a Network Link from a Router to the NI and
 * a Credit Link from NI to the Router
*/

void
GarnetNetwork::makeOutLink(SwitchID src, NodeID dest, BasicLink* link,
                             LinkDirection direction,
                             const NetDest& routing_table_entry)
{
    assert(dest < m_nodes);
    assert(src < m_routers.size());
    assert(m_routers[src] != NULL);

    GarnetExtLink* garnet_link = safe_cast<GarnetExtLink*>(link);
    NetworkLink* net_link = garnet_link->m_network_links[direction];
    CreditLink* credit_link = garnet_link->m_credit_links[direction];

    m_networklinks.push_back(net_link);
    m_creditlinks.push_back(credit_link);

    PortDirection src_outport_dirn = L_;
    m_routers[src]->addOutPort(src_outport_dirn, net_link,
                               routing_table_entry,
                               link->m_weight, credit_link);
    m_nis[dest]->addInPort(net_link, credit_link);
}

/*
 * This function creates an internal network link
*/

void
GarnetNetwork::makeInternalLink(SwitchID src, SwitchID dest,
                                PortDirection src_outport_dirn,
                                PortDirection dest_inport_dirn,
                                BasicLink* link,
                                LinkDirection direction,
                                const NetDest& routing_table_entry)
{
    GarnetIntLink* garnet_link = safe_cast<GarnetIntLink*>(link);
    NetworkLink* net_link = garnet_link->m_network_links[direction];
    CreditLink* credit_link = garnet_link->m_credit_links[direction];

    m_networklinks.push_back(net_link);
    m_creditlinks.push_back(credit_link);

    m_routers[dest]->addInPort(dest_inport_dirn, net_link, credit_link);
    m_routers[src]->addOutPort(src_outport_dirn, net_link,
                               routing_table_entry,
                               link->m_weight, credit_link);
}

int
GarnetNetwork::getNumRouters()
{
    return m_routers.size();
}

int
GarnetNetwork::get_router_id(int ni)
{
    return m_nis[ni]->get_router_id();
}

void
GarnetNetwork::regStats()
{
    // Packets
    m_packets_received
        .init(m_virtual_networks)
        .name(name() + ".packets_received")
        .flags(Stats::pdf | Stats::total | Stats::nozero | Stats::oneline)
        ;

    m_packets_injected
        .init(m_virtual_networks)
        .name(name() + ".packets_injected")
        .flags(Stats::pdf | Stats::total | Stats::nozero | Stats::oneline)
        ;

    m_packet_network_latency
        .init(m_virtual_networks)
        .name(name() + ".packet_network_latency")
        .flags(Stats::oneline)
        ;

    m_packet_queueing_latency
        .init(m_virtual_networks)
        .name(name() + ".packet_queueing_latency")
        .flags(Stats::oneline)
        ;

    for (int i = 0; i < m_virtual_networks; i++) {
        m_packets_received.subname(i, csprintf("vnet-%i", i));
        m_packets_injected.subname(i, csprintf("vnet-%i", i));
        m_packet_network_latency.subname(i, csprintf("vnet-%i", i));
        m_packet_queueing_latency.subname(i, csprintf("vnet-%i", i));
    }

    m_avg_packet_vnet_latency
        .name(name() + ".average_packet_vnet_latency")
        .flags(Stats::oneline);
    m_avg_packet_vnet_latency = m_packet_network_latency / m_packets_received;

    m_avg_packet_vqueue_latency
        .name(name() + ".average_packet_vqueue_latency")
        .flags(Stats::oneline);
    m_avg_packet_vqueue_latency = m_packet_queueing_latency / m_packets_received;

    m_avg_packet_network_latency.name(name() + ".average_packet_network_latency");
    m_avg_packet_network_latency = sum(m_packet_network_latency) / sum(m_packets_received);

    m_avg_packet_queueing_latency.name(name() + ".average_packet_queueing_latency");
    m_avg_packet_queueing_latency = sum(m_packet_queueing_latency) / sum(m_packets_received);

    m_avg_packet_latency.name(name() + ".average_packet_latency");
    m_avg_packet_latency = m_avg_packet_network_latency + m_avg_packet_queueing_latency;

    // Flits
    m_flits_received
        .init(m_virtual_networks)
        .name(name() + ".flits_received")
        .flags(Stats::pdf | Stats::total | Stats::nozero | Stats::oneline)
        ;

    m_flits_injected
        .init(m_virtual_networks)
        .name(name() + ".flits_injected")
        .flags(Stats::pdf | Stats::total | Stats::nozero | Stats::oneline)
        ;

    m_flit_network_latency
        .init(m_virtual_networks)
        .name(name() + ".flit_network_latency")
        .flags(Stats::oneline)
        ;

    m_flit_queueing_latency
        .init(m_virtual_networks)
        .name(name() + ".flit_queueing_latency")
        .flags(Stats::oneline)
        ;

    for (int i = 0; i < m_virtual_networks; i++) {
        m_flits_received.subname(i, csprintf("vnet-%i", i));
        m_flits_injected.subname(i, csprintf("vnet-%i", i));
        m_flit_network_latency.subname(i, csprintf("vnet-%i", i));
        m_flit_queueing_latency.subname(i, csprintf("vnet-%i", i));
    }

    m_avg_flit_vnet_latency
        .name(name() + ".average_flit_vnet_latency")
        .flags(Stats::oneline);
    m_avg_flit_vnet_latency = m_flit_network_latency / m_flits_received;

    m_avg_flit_vqueue_latency
        .name(name() + ".average_flit_vqueue_latency")
        .flags(Stats::oneline);
    m_avg_flit_vqueue_latency = m_flit_queueing_latency / m_flits_received;

    m_avg_flit_network_latency.name(name() + ".average_flit_network_latency");
    m_avg_flit_network_latency = sum(m_flit_network_latency) / sum(m_flits_received);

    m_avg_flit_queueing_latency.name(name() + ".average_flit_queueing_latency");
    m_avg_flit_queueing_latency = sum(m_flit_queueing_latency) / sum(m_flits_received);

    m_avg_flit_latency.name(name() + ".average_flit_latency");
    m_avg_flit_latency = m_avg_flit_network_latency + m_avg_flit_queueing_latency;


    // Hops
    m_avg_hops.name(name() + ".average_hops");
    m_avg_hops = m_total_hops / sum(m_flits_received);

    // Links
    m_average_link_utilization.name(name() + ".avg_link_utilization");

    m_average_vc_load
        .init(m_virtual_networks * m_vcs_per_vnet)
        .name(name() + ".avg_vc_load")
        .flags(Stats::pdf | Stats::total | Stats::nozero | Stats::oneline)
        ;
}

void
GarnetNetwork::collateStats()
{
    RubySystem *rs = params()->ruby_system;
    double timeelta = double(curCycle() - rs->getStartCycle());

    for (int i = 0; i < m_networklinks.size(); i++) {
        m_average_link_utilization +=
            (double(m_networklinks[i]->getLinkUtilization())) / timeelta;

        vector<unsigned int> vc_load = m_networklinks[i]->getVcLoad();
        for (int j = 0; j < vc_load.size(); j++) {
            m_average_vc_load[j] += ((double)vc_load[j] / timeelta);
        }
    }

    // Ask the routers to collate their statistics
    for (int i = 0; i < m_routers.size(); i++) {
        m_routers[i]->collateStats();
    }
}

void
GarnetNetwork::print(ostream& out) const
{
    out << "[GarnetNetwork]";
}

GarnetNetwork *
GarnetNetworkParams::create()
{
    return new GarnetNetwork(this);
}

uint32_t
GarnetNetwork::functionalWrite(Packet *pkt)
{
    uint32_t num_functional_writes = 0;

    for (unsigned int i = 0; i < m_routers.size(); i++) {
        num_functional_writes += m_routers[i]->functionalWrite(pkt);
    }

    for (unsigned int i = 0; i < m_nis.size(); ++i) {
        num_functional_writes += m_nis[i]->functionalWrite(pkt);
    }

    for (unsigned int i = 0; i < m_networklinks.size(); ++i) {
        num_functional_writes += m_networklinks[i]->functionalWrite(pkt);
    }

    return num_functional_writes;
}


//static bubble scheme

PortDirection GarnetNetwork::get_turn2outport_dirn(PortDirection m_dir, turn t)
{
  switch(m_dir)
    {
    case W_ :
      return ( get_west_port_turn2outport_dirn(t) );

    case E_ :
      return ( get_east_port_turn2outport_dirn(t) );

    case N_ :
      return ( get_north_port_turn2outport_dirn(t) );

    case S_ :
      return ( get_south_port_turn2outport_dirn(t) );

    default:
      assert(0);
      return W_;
    }
}

turn GarnetNetwork::get_outport_dirn2turn(PortDirection m_dir, PortDirection dir)
{
  switch(m_dir)
    {
    case W_ :
      return ( get_west_port_outport_dirn2turn(dir) );

    case E_ :
      return ( get_east_port_outport_dirn2turn(dir) );

    case N_ :
      return ( get_north_port_outport_dirn2turn(dir) );

    case S_ :
      return ( get_south_port_outport_dirn2turn(dir) );

    default:
      assert(0);
      return NUM_TURN_TYPES;
    }
}
