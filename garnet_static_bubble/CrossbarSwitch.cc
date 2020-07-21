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

#include "mem/ruby/network/garnet2.0/CrossbarSwitch.hh"

#include "base/stl_helpers.hh"
#include "debug/RubyNetwork.hh"
#include "mem/ruby/network/garnet2.0/OutputUnit.hh"
#include "mem/ruby/network/garnet2.0/Router.hh"

#include <iostream>

using m5::stl_helpers::deletePointers;

CrossbarSwitch::CrossbarSwitch(Router *router)
    : Consumer(router)
{
    m_router = router;
    m_num_vcs = m_router->get_num_vcs();
    m_crossbar_activity = 0;
}

CrossbarSwitch::~CrossbarSwitch()
{
    deletePointers(m_switch_buffer);
}

void
CrossbarSwitch::init()
{
    m_output_unit = m_router->get_outputUnit_ref();

    m_num_inports = m_router->get_num_inports();
    m_switch_buffer.resize(m_num_inports);
    for (int i = 0; i < m_num_inports; i++) {
        m_switch_buffer[i] = new flitBuffer();
    }
}

void
CrossbarSwitch::wakeup()
{
  /*  if( (m_router->get_id() == 20) && (m_router->curCycle() == 385) && (!m_switch_buffer[5]->isEmpty()) )
    {   
      std::cout<<"\nCrossbar Switch "<<m_router->get_id()<<" wakeup in cycle 385"<<" time of disable is "
	       <<m_switch_buffer[5]->peekTopFlit()->get_time()<<" and type is "<<m_switch_buffer[5]->peekTopFlit()->get_type()
	       <<" and no of flits in the crossbar buffer are: "<<m_switch_buffer[5]->get_size()<<std::flush;

      print_buffer(5);
      }*/

    DPRINTF(RubyNetwork, "CrossbarSwitch at Router %d woke up at time: %lld\n",
            m_router->get_id(), m_router->curCycle());

    for (int inport = 0; inport < m_num_inports; inport++) 
      {
        if (!m_switch_buffer[inport]->isReady(m_router->curCycle()))
            continue;
	//static bubble scheme: each input port may have more than one probes that want to use different outports

	while( !(m_switch_buffer[inport]->isEmpty()) )
	  {
	    flit *t_flit = m_switch_buffer[inport]->peekTopFlit();
	    if (t_flit->is_stage(ST_, m_router->curCycle())) 
	      {
		int outport = t_flit->get_outport();
		t_flit->advance_stage(LT_, m_router->curCycle());
		t_flit->set_time(m_router->curCycle());
	    
		// This will take care of waking up the Network Link
		m_output_unit[outport]->insert_flit(t_flit);
		m_switch_buffer[inport]->getTopFlit();
		m_crossbar_activity++;

		//   if( (t_flit->get_type() == DISABLE_ ) && (t_flit->get_source_id() == 46) && (m_router->curCycle() == 385) )
		// std::cout<<"\n disable in crossbar switch in cycle 385 "<<std::flush;
	      }
	  }
      }
}

uint32_t
CrossbarSwitch::functionalWrite(Packet *pkt)
{
   uint32_t num_functional_writes = 0;

   for (uint32_t i = 0; i < m_switch_buffer.size(); ++i) {
       num_functional_writes += m_switch_buffer[i]->functionalWrite(pkt);
   }

   return num_functional_writes;
}

//static bubble scheme

void CrossbarSwitch::init_sb_scheme()
{
  m_num_vcs = m_router->get_num_vcs();
}

void CrossbarSwitch::print_buffer(int inport)
{
  for(int i=0; i<m_switch_buffer[inport]->get_size(); i++)
    {
      std::cout<<"\ntype of flit "<<i<<" is "<<m_switch_buffer[inport]->peekflit(i)->get_type()<<std::flush;
    }
}
