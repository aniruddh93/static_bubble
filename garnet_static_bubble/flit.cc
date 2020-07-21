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

#include "mem/ruby/network/garnet2.0/flit.hh"

flit::flit(int id, int  vc, int vnet, RouteInfo route, int size, MsgPtr msg_ptr,
    Cycles curTime)
{
    m_size = size;
    m_msg_ptr = msg_ptr;
    m_enqueue_time = curTime;
    m_time = curTime;
    m_id = id;
    m_vnet = vnet;
    m_vc = vc;
    m_route = route;
    m_stage.first = I_;
    m_stage.second = m_time;

    if (size == 1) {
        m_type = HEAD_TAIL_;
        return;
    }
    if (id == 0)
        m_type = HEAD_;
    else if (id == (size - 1))
        m_type = TAIL_;
    else
        m_type = BODY_;
}

flit::flit(int vc, bool is_free_signal, Cycles curTime)
{
    m_id = 0;
    m_vc = vc;
    m_is_free_signal = is_free_signal;
    m_is_sb_signal = false;
    m_time = curTime;
}

//static bubble scheme

flit::flit(int vc, Cycles curTime)
{
    m_id = 0;
    m_vc = vc;
    m_is_sb_signal = true;
    m_is_free_signal = false;
    m_time = curTime;
}

flit::flit(flit_type f_type, int node_id, int output_port, int input_port, Cycles curTime, int vnet, Router *curRouter)
{
  m_enqueue_time = curTime;
  m_time = curTime;
  m_type = f_type;
  m_source_id = node_id;
  m_turns.clear();
  m_vnet = vnet;
  m_outport = output_port;
  m_inport = input_port;
  m_curr_router = curRouter;
  m_source_outport = output_port;
  m_source_inport = input_port;
}

//copy constructor

flit::flit(flit *flt)
{
  m_enqueue_time = flt->get_enqueue_time();
  m_time = flt->get_time();
  m_type = flt->get_type();
  m_source_id = flt->get_source_id();
  m_turns.clear();

  for(int i=0; i < flt->get_num_turns(); i++)
    {
      m_turns.push_back(flt->peek_turn(i));
    }

  m_vnet = flt->get_vnet();
  m_outport = flt->get_outport();
  m_inport = flt->get_inport();
  m_curr_router = flt->get_curr_router();
  m_source_outport = flt->get_source_outport();
  m_source_inport = flt->get_source_inport();
}

void
flit::print(std::ostream& out) const
{
    out << "[flit:: ";
    out << "Id=" << m_id << " ";
    out << "Type=" << m_type << " ";
    out << "Vnet=" << m_vnet << " ";
    out << "VC=" << m_vc << " ";
    out << "Dest NI=" << m_route.dest_ni << " ";
    out << "Dest Router=" << m_route.dest_router << " ";
    out << "Enqueue Time=" << m_enqueue_time << " ";
    out << "]";
}

bool
flit::functionalWrite(Packet *pkt)
{
    Message *msg = m_msg_ptr.get();
    return msg->functionalWrite(pkt);
}


//static bubble scheme: function for creating priority queues for enable, probe and disable

bool flit::sb_greater(flit* n1, flit* n2)
{
  switch(n1 -> m_type)
    {
    case  DISABLE_:
      return( n1->compareDisable(n1, n2) ); // compare on the basis of source_id

    case ENABLE_:
      return( n1->compareEnable(n1, n2) ); //

    case PROBE_:
      return( n1->compareProbe(n1, n2) ); //

    default: 
      assert(0);
      break;
    }

  return false;
}

bool flit::compareDisable(flit* n1, flit* n2)
{
  if((n1->m_source_id) > (n2->m_source_id))
    return true;
  
  else
    return false;
}


bool flit::compareProbe(flit* n1, flit* n2)
{
  if((n1->m_source_id) > (n2->m_source_id))
    return true;
  
  else
    return false;
}

bool flit::compareEnable(flit* n1, flit* n2)
{
  if(m_curr_router->is_deadlock())
    {
      if((n1->m_source_id) == (m_curr_router->get_io_pbuffer_node_id()))
	 return true;
	
      else if((n2->m_source_id) == (m_curr_router->get_io_pbuffer_node_id()))
	 return false;
	
       else
	 {
	   if((n1->m_source_id) > (n2->m_source_id))
	     return true;
	   
	   else
	     return false;
	 }

    }
     
  else
    {
      if((n1->m_source_id) > (n2->m_source_id))
	return true;
      
      else
	return false;
    }

      
}

turn flit::get_top_turn()
{
  turn t = m_turns[0];
  for(int i=0; i < m_turns.size()-1; i++ )
    {
      m_turns[i] = m_turns[i+1];
    }

  m_turns.pop_back();

  return t;
}

int flit::get_num_turns()
{
  return m_turns.size();
}
