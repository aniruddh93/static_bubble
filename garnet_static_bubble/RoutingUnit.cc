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

#include "mem/ruby/network/garnet2.0/RoutingUnit.hh"

#include "base/cast.hh"
#include "mem/ruby/network/garnet2.0/InputUnit.hh"
#include "mem/ruby/network/garnet2.0/Router.hh"
#include "mem/ruby/slicc_interface/Message.hh"

#include <iostream>

using namespace std;

RoutingUnit::RoutingUnit(Router *router)
{
    m_router = router;
    m_routing_table.clear();
    m_weight_table.clear();
}

void
RoutingUnit::addRoute(const NetDest& routing_table_entry)
{
    m_routing_table.push_back(routing_table_entry);
}

void
RoutingUnit::addWeight(int link_weight)
{
    m_weight_table.push_back(link_weight);
}

int
RoutingUnit::lookupRoutingTable(NetDest msg_destination)
{
    int output_link = -1;
    int min_weight = INFINITE_;

    for (int link = 0; link < m_routing_table.size(); link++) {
        if (msg_destination.intersectionIsNotEmpty(m_routing_table[link])) {
            if (m_weight_table[link] >= min_weight)
                continue;
            output_link = link;
            min_weight = m_weight_table[link];
        }
    }

    if (output_link == -1) {
        fatal("Fatal Error:: No Route exists from this Router.");
        exit(0);
    }

    return output_link;
}


void
RoutingUnit::addInDirection(PortDirection inport_dirn, int inport_idx)
{
    m_inports_dirn2idx[inport_dirn] = inport_idx;
    m_inports_idx2dirn[inport_idx]  = inport_dirn;
}

void
RoutingUnit::addOutDirection(PortDirection outport_dirn, int outport_idx)
{
    m_outports_dirn2idx[outport_dirn] = outport_idx;
    m_outports_idx2dirn[outport_idx]  = outport_dirn;
}

int
RoutingUnit::outportCompute(RouteInfo route, int inport, PortDirection inport_dirn, int vc)
{
    int outport = -1;

    if (route.dest_router == m_router->get_id())
    {
        // Multiple NIs may be connected to this router, all with dirn = L_
        // Get exact outport id from table
        outport = lookupRoutingTable(route.net_dest);
        return outport;
    }

    RoutingAlgorithm routing_algorithm = (RoutingAlgorithm) m_router->get_net_ptr()->getRoutingAlgorithm();

    switch(routing_algorithm)
    {
        case TABLE_:  outport = lookupRoutingTable(route.net_dest); break;
        case XY_:     outport = outportComputeXY(route, inport, inport_dirn); break;
    case RANDOM_: outport = outportComputeRandom(route, inport, inport_dirn, vc); break;
        case TURN_MODEL_: outport = outportComputeTurnModel(route, inport, inport_dirn); break;
        // any custom algorithm
        //case CUSTOM_: outportComputeCustom(); break;
        default: outport = lookupRoutingTable(route.net_dest); break;
    }

    assert(outport != -1);
    return outport;
}

int
RoutingUnit::outportComputeXY(RouteInfo route,
                              int inport,
                              PortDirection inport_dirn)
{
    PortDirection outport_dirn = UNKNOWN_;

    int num_rows = m_router->get_net_ptr()->getNumRows();
    int num_cols = m_router->get_net_ptr()->getNumCols();
    assert(num_rows > 0 && num_cols > 0);

    int my_id = m_router->get_id();
    int my_x = my_id % num_cols;
    int my_y = my_id / num_cols;

    int dest_id = route.dest_router;
    int dest_x = dest_id % num_cols;
    int dest_y = dest_id / num_cols;

    int x_hops = abs(dest_x - my_x);
    int y_hops = abs(dest_y - my_y);

    bool x_dirn = (dest_x >= my_x);
    bool y_dirn = (dest_y >= my_y);

    // already checked that in outportCompute() function
    assert(!(x_hops == 0 && y_hops == 0));

    if (x_hops > 0)
    {
        if (x_dirn)
        {
            assert(inport_dirn == L_ || inport_dirn == W_);
            outport_dirn = E_;
        }
        else
        {
            assert(inport_dirn == L_ || inport_dirn == E_);
            outport_dirn = W_;
        }
    }
    else if (y_hops > 0)
    {
        if (y_dirn)
        {
            assert(inport_dirn != N_); // L_ or S_ or W_ or E_
            outport_dirn = N_;
        }
        else
        {
            assert(inport_dirn != S_); // L_ or N_ or W_ or E_
            outport_dirn = S_;
        }
    }
    else
    {
        // x_hops == 0 and y_hops == 0
        // this is not possible
        // already checked that in outportCompute() function
        assert(0);
    }

    return m_outports_dirn2idx[outport_dirn];
}

int
RoutingUnit::outportComputeRandom(RouteInfo route,
                                  int inport,
                                  PortDirection inport_dirn, int vc)
{
    PortDirection outport_dirn = UNKNOWN_;

    int num_rows = m_router->get_net_ptr()->getNumRows();
    int num_cols = m_router->get_net_ptr()->getNumCols();
    assert(num_rows > 0 && num_cols > 0);

    int my_id = m_router->get_id();
    int my_x = my_id % num_cols;
    int my_y = my_id / num_cols;

    int dest_id = route.dest_router;
    int dest_x = dest_id % num_cols;
    int dest_y = dest_id / num_cols;

    int x_hops = abs(dest_x - my_x);
    int y_hops = abs(dest_y - my_y);

    bool x_dirn = (dest_x >= my_x);
    bool y_dirn = (dest_y >= my_y);

    // already checked that in outportCompute() function
    assert(!(x_hops == 0 && y_hops == 0));

    int escapeVCenabled=m_router->get_net_ptr()->getEscapeVC();
    
    int temp=m_router->get_net_ptr()->getVCsPerVnet();
    int temp1=vc%temp;

    //    cout<<"\nhello"<<escapeVCenabled;

    if((temp1==(temp-1))&&(escapeVCenabled==1)) 
      return (outportComputeTurnModel(route, inport, inport_dirn));

    if (x_hops == 0)
    {
        if (y_dirn > 0)
            outport_dirn = N_;
        else
            outport_dirn = S_;
    }
    else if (y_hops == 0)
    {
        if (x_dirn > 0)
            outport_dirn = E_;
        else
            outport_dirn = W_;
    }
    else
    {
        int rand = random() % 2;

        if (x_dirn && y_dirn) // Quadrant I
            outport_dirn = rand ? E_ : N_;
        else if (!x_dirn && y_dirn) // Quadrant II
            outport_dirn = rand ? W_ : N_;
        else if (!x_dirn && !y_dirn) // Quadrant III
            outport_dirn = rand ? W_ : S_;
        else // Quadrant IV
            outport_dirn = rand ? E_ : S_;

    }

    return m_outports_dirn2idx[outport_dirn];
}


int
RoutingUnit::outportComputeTurnModel(RouteInfo route,
                                  int inport,
                                  PortDirection inport_dirn)
{
    PortDirection outport_dirn = UNKNOWN_;

    //////////////////////////////////////////////
    // Interconnection Networks Lab 3
    // Add your code here




    int num_rows = m_router->get_net_ptr()->getNumRows();
    int num_cols = m_router->get_net_ptr()->getNumCols();
    assert(num_rows > 0 && num_cols > 0);

    int my_id = m_router->get_id();
    int my_x = my_id % num_cols;
    int my_y = my_id / num_cols;

    int dest_id = route.dest_router;
    int dest_x = dest_id % num_cols;
    int dest_y = dest_id / num_cols;

    int x_hops = abs(dest_x - my_x);
    int y_hops = abs(dest_y - my_y);

     assert(!(x_hops == 0 && y_hops == 0));

    if((dest_x<my_x)&&(dest_y==my_y)) assert((inport_dirn !=S_)&&(inport_dirn !=N_));

    if(dest_x<my_x)
      {
	assert(inport_dirn ==E_||inport_dirn ==L_);
	  outport_dirn=W_;  
	  //	  cout<<"\n"<<my_id<<flush;
      }
    else if(dest_x==my_x)
      {
	if(dest_y<my_y)
	  {
	    assert(inport_dirn !=S_);
	    outport_dirn=S_;
	  }
	else
	  {
	    assert(inport_dirn !=N_);
	    outport_dirn=N_;
	  }
      }
    else
      {
	 assert(inport_dirn !=E_);
	 outport_dirn=E_;
      }

    //    cout<<"\n"<<my_id<<"->"<<inport_dirn<<"->"<<outport_dirn<<flush;

    /////////////////////////////////////////////

    return m_outports_dirn2idx[outport_dirn];
}


//static bubble scheme

int RoutingUnit::get_outport_dirn2idx(PortDirection dir)
{
  return m_outports_dirn2idx[dir];
}

int RoutingUnit::get_inport_dirn2idx(PortDirection dir)
{
  return m_inports_dirn2idx[dir];
}

PortDirection RoutingUnit::get_inport_idx2dirn(int id)
{
  return m_inports_idx2dirn[id];
}

PortDirection RoutingUnit::get_outport_idx2dirn(int id)
{
  return m_outports_idx2dirn[id];
}
