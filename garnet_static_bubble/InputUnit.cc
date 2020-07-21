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

#include "mem/ruby/network/garnet2.0/InputUnit.hh"

#include "base/stl_helpers.hh"
#include "debug/RubyNetwork.hh"
#include "mem/ruby/network/garnet2.0/Router.hh"
#include "mem/ruby/network/garnet2.0/CommonTypes.hh"
#include "mem/ruby/network/garnet2.0/RoutingUnit.hh"

using namespace std;
using m5::stl_helpers::deletePointers;

InputUnit::InputUnit(int id, PortDirection direction, Router *router)
            : Consumer(router)
{
    m_id = id;
    m_direction = direction;
    // cout<<"input unit direction "<<
    m_router = router;
    m_num_vcs = m_router->get_num_vcs();
    m_vc_per_vnet = m_router->get_vc_per_vnet();
    m_pipeline_delay = Cycles(m_router->get_net_ptr()->getNumPipeStages());

    m_num_buffer_reads.resize(m_num_vcs/m_vc_per_vnet);
    m_num_buffer_writes.resize(m_num_vcs/m_vc_per_vnet);
    for (int i = 0; i < m_num_buffer_reads.size(); i++) {
        m_num_buffer_reads[i] = 0;
        m_num_buffer_writes[i] = 0;
    }

    creditQueue = new flitBuffer();
    // Instantiating the virtual channels
    m_vcs.resize(m_num_vcs);
    for (int i=0; i < m_num_vcs; i++) {
        m_vcs[i] = new VirtualChannel(i);
    }
}

InputUnit::~InputUnit()
{
    delete creditQueue;
    deletePointers(m_vcs);
}

void
InputUnit::wakeup()
{

  if( (m_router->get_id()==28) && (m_direction==S_) && (m_router->curCycle() == 1848) )
    {
      cout<<"\n is deadlock "<<m_router->is_deadlock();
      cout<<"\noutport direction of vc0 at router 28 is "<<m_router->getRoutingUnit_ref()->get_outport_idx2dirn(m_vcs[0]->get_outport())<<flush;
      cout<<"\noutport direction of vc1 at router 28 is "<<m_router->getRoutingUnit_ref()->get_outport_idx2dirn(m_vcs[1]->get_outport())<<flush;
      cout<<"\noutport direction of vc2 at router 28 is "<<m_router->getRoutingUnit_ref()->get_outport_idx2dirn(m_vcs[2]->get_outport())<<flush;
      cout<<"\noutport direction of vc3 at router 28 is "<<m_router->getRoutingUnit_ref()->get_outport_idx2dirn(m_vcs[3]->get_outport())<<flush;
    }

  //  cout<<"\nInputUnit "<<m_router->get_id()<<" wakeup"<<flush;
    flit *t_flit;
    if (m_in_link->isReady(m_router->curCycle())) 
      {

	if( (m_router->get_id()==19) && (m_router->curCycle()==388) )
	  cout<<"\nthere exists flit in link of Router 19 at cycle 388"<<flush;

        t_flit = m_in_link->consumeLink();
        t_flit->increment_hops(); // for stats

        if ((t_flit->get_type() == HEAD_) || (t_flit->get_type() == HEAD_TAIL_)) 
	  {
	    //	    if( (m_router->get_id() == 37) && (m_router->is_deadlock()) )
	      //	    std::cout<<"\n flit "<<t_flit->get_id()<<" received at node 37 for vc "<<t_flit->get_vc()<<" at cycle "<<m_router->curCycle()<<std::flush;
	    
	    int vc = t_flit->get_vc();

            assert(m_vcs[vc]->get_state() == IDLE_);
            set_vc_active(vc, m_router->curCycle());

	    //static bubble scheme
	    if(m_router-> get_net_ptr()->getStaticBubbleSchemeEnabled())
	      {
		if( m_router->is_static_bubble_node() )
		  {
		    if( (m_router->get_counter_state() == S_OFF) && (m_direction!=L_) && (!m_router->is_deadlock()) )
		      {
			//if counter is off and the flit input port is not local, 
			// switch on the counter and make it point to this vc
			m_router->set_counter(0 , m_direction, vc, S_DEADLOCK_DETECTION, DD);
		      }
		  }
	      }


            // Route computation for this vc
            int outport = m_router->route_compute(t_flit->get_route(), m_id, m_direction, t_flit->get_vc());

            // Update output port in VC
            // All flits in this packet will use this output port
            // The output port field in the flit is updated after it wins SA
            grant_outport(vc, outport);

        } 
	else if(t_flit->get_type() == DISABLE_)
	  {
	    cout<<"\ndisable from node: "<<t_flit->get_source_id()<<" received at node: "<<m_router->get_id()<<" cycle "<<m_router->curCycle()<<flush;
	    assert(m_direction!=L_);

	    if(t_flit->get_source_id() == m_router->get_id())
	      {
		assert(m_router->get_counter_state() == S_DISABLE);

		if(m_router->get_net_ptr()->getDisableCheckEnabled())
		  {
		    if(check_inport(m_router->get_io_pbuffer_vnet(), m_router->get_io_pbuffer_outport()) )
		      {
			m_router->set_is_deadlock(true);
		
			int vnet = t_flit->get_vnet();
			int vc = vnet*m_vc_per_vnet + m_vc_per_vnet - 1;
		    
			//switch on static bubble
			cout<<"\n static bubble switched on at node: "<<m_router->get_id()<<", cycle: "<<m_router->curCycle()<<flush;
			set_vc_idle(vc, m_router->curCycle());
			increment_credit(vc, true, m_router->curCycle());

			//change counter state: to active
			m_router->set_counter(ZERO, m_router->get_counter_direction(), m_router->get_counter_vc_num(), S_STATIC_BUBBLE_ACTIVE, SB);
			delete t_flit;
		      }
		    else
		      {
			m_router->set_counter(ZERO, m_router->get_counter_direction(), m_router->get_counter_vc_num(), S_ENABLE, DE);
			m_router->send_enable();
			delete t_flit;
		      }
		  }
		else
		  {
		    m_router->set_is_deadlock(true);
		
		    int vnet = t_flit->get_vnet();
		    int vc = vnet*m_vc_per_vnet + m_vc_per_vnet - 1;
		    
		    //switch on static bubble
		    cout<<"\n static bubble switched on at node: "<<m_router->get_id()<<", cycle: "<<m_router->curCycle()<<flush;
		    set_vc_idle(vc, m_router->curCycle());
		    increment_credit(vc, true, m_router->curCycle());

		    //change counter state: to active
		    m_router->set_counter(ZERO, m_router->get_counter_direction(), m_router->get_counter_vc_num(), S_STATIC_BUBBLE_ACTIVE, SB);
		    delete t_flit;
		  }
	      }
	    else
	      {
		if(m_router->get_net_ptr()->getDisableCheckEnabled())
		  {
		    t_flit->update_curr_router(m_router);
		    t_flit->set_inport(m_router->getRoutingUnit_ref()->get_inport_dirn2idx(m_direction));
		    turn t = t_flit->get_top_turn();
		    PortDirection outport_dirn = m_router->get_net_ptr()->get_turn2outport_dirn(m_direction, t);
		    assert(outport_dirn != L_ );
		    int outport = m_router->getRoutingUnit_ref()->get_outport_dirn2idx(outport_dirn);
		    t_flit->set_outport(outport);

		    if(check_inport(t_flit->get_vnet(), outport))
		      {
			m_router->getDisableQueue()->insert(t_flit, true);
		      }
		    else
		      {
			delete t_flit; //drop the disable
		      }
		  }
		else
		  {
		    t_flit->update_curr_router(m_router);
		    t_flit->set_inport(m_router->getRoutingUnit_ref()->get_inport_dirn2idx(m_direction));
		    turn t = t_flit->get_top_turn();
		    PortDirection outport_dirn = m_router->get_net_ptr()->get_turn2outport_dirn(m_direction, t);
		    assert(outport_dirn != L_ );
		    int outport = m_router->getRoutingUnit_ref()->get_outport_dirn2idx(outport_dirn);
		    t_flit->set_outport(outport);
		    m_router->getDisableQueue()->insert(t_flit, true);
		  }
	      }
	  }
	else if(t_flit->get_type() == PROBE_)
	  {
	    
	       if( (t_flit->get_source_id() == 37) && (m_router->curCycle()>1844) && (m_router->curCycle()<1976) )
		 cout<<"\nprobe from node: "<<t_flit->get_source_id()<<" received at node: "<<m_router->get_id()
		     <<" cycle "<<m_router->curCycle()<<flush;
	    assert(m_direction!=L_);
	    
	    if(t_flit->get_source_id() == m_router->get_id())
	      {
		if(m_router->get_counter_state() == S_DEADLOCK_DETECTION)
		  {
		    cout<<"\nprobe from node: "<<t_flit->get_source_id()<<" received at node: "<<m_router->get_id()
			<<" at inport_direction :"<<m_direction<<" cycle: "<<m_router->curCycle()<<flush;

		    int my_inport = m_router->getRoutingUnit_ref()->get_inport_dirn2idx(m_direction);
		    int flit_outport = t_flit->get_source_outport();
		    PortDirection flit_outport_dir = m_router->getRoutingUnit_ref()->get_outport_idx2dirn(flit_outport);

		    if( (m_direction != flit_outport_dir) && (check_inport(t_flit->get_vnet(), flit_outport) ) )
		      {
			m_router->latch_deadlock_path(t_flit);
			m_router->set_counter(ZERO, m_router->get_counter_direction(), m_router->get_counter_vc_num(), S_DISABLE, DE);

			//set the IO priority buffer: change inport to flit_inport
			m_router->set_IO_priority_buffer(m_router->get_id(), t_flit->get_vnet(), m_direction, my_inport, flit_outport_dir, flit_outport);

			m_router->send_disable();
		      }
		      
		  }

		delete t_flit;
	      }
	    else
	      {
		fork_probe(t_flit);
	      }

	  }
	else if(t_flit->get_type() == ENABLE_)
	  {
	    cout<<"\nenable from node: "<<t_flit->get_source_id()<<" received at node: "<<m_router->get_id()
		<<" and counter state is "<<m_router->get_counter_state()<<flush;
	    assert(m_direction!=L_);

	    if(t_flit->get_source_id() == m_router->get_id())
	      {
		assert(m_router->get_counter_state() == S_ENABLE);
		assert(!m_router->is_deadlock());
		std::pair<PortDirection, int> cntr_next_pointer = m_router->get_cntr_next_pointer();
		
		counter_state cntr_state;

		if(cntr_next_pointer.second == -1)
		  cntr_state = S_OFF;
		else
		  cntr_state = S_DEADLOCK_DETECTION;

		m_router->set_counter(ZERO, cntr_next_pointer.first, cntr_next_pointer.second, cntr_state, DD);
		
		delete t_flit;
	      }
	    else
	      {
		t_flit->update_curr_router(m_router);
		t_flit->set_inport(m_router->getRoutingUnit_ref()->get_inport_dirn2idx(m_direction));
		turn t = t_flit->get_top_turn();
		PortDirection outport_dirn = m_router->get_net_ptr()->get_turn2outport_dirn(m_direction, t);
		int outport = m_router->getRoutingUnit_ref()->get_outport_dirn2idx(outport_dirn);
		t_flit->set_outport(outport);
		m_router->getEnableQueue()->insert(t_flit, true);
	      }
	  }
	else if(t_flit->get_type() == CHECK_PROBE_ )
	  {
	    if(t_flit->get_source_id() == m_router->get_id() )
	      {
		assert(m_router->get_counter_state() == S_CHECK_PROBE );
		assert(m_router->is_deadlock());
		
		//switch on sb
		int vnet = t_flit->get_vnet();
		assert(vnet == m_router->get_io_pbuffer_vnet() );
		int inport = m_router->getRoutingUnit_ref()->get_inport_dirn2idx(m_direction);
		assert(m_router->get_io_pbuffer_inport() == inport);

		int vc = (vnet*m_vc_per_vnet) + (m_vc_per_vnet - 1) ;
		set_vc_idle(vc, m_router->curCycle());
		increment_credit(vc, true, m_router->curCycle());

		//change counter state: to active
		m_router->set_counter(ZERO, m_router->get_counter_direction(), m_router->get_counter_vc_num(), S_STATIC_BUBBLE_ACTIVE, SB);
		delete t_flit;
	      }
	    else
	      {
		if(check_inport(m_router->get_io_pbuffer_vnet(), m_router->get_io_pbuffer_outport()))
		  {
		    t_flit->update_curr_router(m_router);
		    t_flit->set_inport(m_router->getRoutingUnit_ref()->get_inport_dirn2idx(m_direction));
		    turn t = t_flit->get_top_turn();
		    PortDirection outport_dirn = m_router->get_net_ptr()->get_turn2outport_dirn(m_direction, t);
		    assert(outport_dirn != L_ );
		    int outport = m_router->getRoutingUnit_ref()->get_outport_dirn2idx(outport_dirn);
		    t_flit->set_outport(outport);
		    m_router->getCheck_probeQueue()->insert(t_flit);
		  }
		else
		  {
		    delete t_flit;
		  }
	      }
	  }
	else 
	  {
	    int vc = t_flit->get_vc();
            assert(m_vcs[vc]->get_state() == ACTIVE_);
	    //static bubble scheme
	    if(m_router-> get_net_ptr()->getStaticBubbleSchemeEnabled())
	      {

		if( m_router->is_static_bubble_node() )
		  {
		    if((m_router->get_counter_state()==S_OFF) && (m_direction!=L_) && (!m_router->is_deadlock()) )
		      {
			//if counter is off and the flit input port is not local, 
			// switch on the counter and make it point to this vc
			m_router->set_counter(0 , m_direction, vc, S_DEADLOCK_DETECTION, DD);
		      }
		  }
	      }
	  }


	if((t_flit->get_type() == HEAD_) || (t_flit->get_type() == BODY_) || (t_flit->get_type() == TAIL_) || (t_flit->get_type() == HEAD_TAIL_))
	  {
	    int vc = t_flit->get_vc();

	    // Buffer the flit
	    m_vcs[vc]->insertFlit(t_flit);

	    int vnet = vc/m_vc_per_vnet;
	    // number of writes same as reads
	    // any flit that is written will be read only once
	    m_num_buffer_writes[vnet]++;
	    m_num_buffer_reads[vnet]++;

	    // This is the first-stage of the router
	    // Wait for (m_pipeline_delay - 1) cycles before
	    // performing Switch Allocation
	    Cycles wait_time = m_pipeline_delay - Cycles(1);
	    t_flit->advance_stage(SA_, m_router->curCycle() + wait_time);

	    if( (vc % m_vc_per_vnet) == (m_vc_per_vnet - 1) )
	      {
		cout<<" \nstatic bubble allocated at node "<<m_router->get_id()<<" for outport dirn "
		    <<m_router->getRoutingUnit_ref()->get_outport_idx2dirn(m_vcs[vc]->get_outport())<<" at cycle "<<m_router->curCycle();
	      }

	  }
    }
}


uint32_t
InputUnit::functionalWrite(Packet *pkt)
{
    uint32_t num_functional_writes = 0;
    for (int i=0; i < m_num_vcs; i++) {
        num_functional_writes += m_vcs[i]->functionalWrite(pkt);
    }

    return num_functional_writes;
}

void
InputUnit::resetStats()
{
    for (int j = 0; j < m_num_buffer_reads.size(); j++) {
        m_num_buffer_reads[j] = 0;
        m_num_buffer_writes[j] = 0;
    }
}


//static bubble scheme init

void InputUnit::init_sb_scheme()
{
  int old_size = m_num_vcs;
  m_num_vcs = m_router->get_num_vcs();
  m_vc_per_vnet = m_router->get_vc_per_vnet();

  //no need to resize statistical vector because no of vnets is const

  m_vcs.resize(m_num_vcs);

  for (int i=old_size; i < m_num_vcs; i++) 
    {
      m_vcs[i] = new VirtualChannel(i);
    }
  
}

void InputUnit::fork_probe(flit *t_flit)
{
  if(t_flit->get_num_turns() > MAX_TURNS)
    {
      if(m_router->get_id() == 28)
	cout<<"\nprobe from node: "<<t_flit->get_source_id()<<" dropped at node: "<<m_router->get_id()
	    <<" because out of turns, cycle "<<m_router->curCycle()<<flush;

      delete t_flit;    //drop the probe
      return;
    }
  
  if(m_router->is_static_bubble_node())
    {
      if(m_router->get_id() > t_flit->get_source_id())
	{
	 
	  if(m_router->get_id() == 28)
	    cout<<"\nprobe from node: "<<t_flit->get_source_id()<<" dropped at node: "<<m_router->get_id()
		<<" beacuse from lower source id, cycle "<<m_router->curCycle()<<flush;

	  delete t_flit;    //drop the probe
	  return;
	}
    }
  

  int start = t_flit->get_vnet()*m_vc_per_vnet;
  int end = start + m_vc_per_vnet - 2;  

  for(int i = start; i <= end; i++)
    {
      if(m_vcs[i]->get_state() == IDLE_) 
	{
	  if(m_router->get_id()==28)
	    cout<<"\nprobe from node: "<<t_flit->get_source_id()<<" dropped at node: "<<m_router->get_id()<<"because vc: "<<i<<" is in state IDLE"<<flush;

	  delete t_flit;    //drop the probe
	  return;
	}
      else if(m_vcs[i]->peekTopFlit()->get_route().dest_router == m_router->get_id())
	{
	  assert(m_vcs[i]->get_state() == ACTIVE_);

	  if(m_router->get_id()==28)
	    cout<<"\nprobe from node: "<<t_flit->get_source_id()<<" dropped at node: "<<m_router->get_id()<<"because vc: "<<i<<" is waiting for ejection"<<flush;

	  delete t_flit;    //drop the probe
	  return;
	}
    }

  for( int i = start; i<=end; i++)
    {
      assert(m_vcs[i]->get_state() == ACTIVE_);
     
      int output_port = m_vcs[i]->get_outport();
      PortDirection output_port_dirn = m_router->getRoutingUnit_ref()->get_outport_idx2dirn(output_port);

      assert(output_port_dirn != m_direction);

      flit *probe = new flit(t_flit);  //use the copy constructor to create a copy and then append your turn
      turn t = m_router->get_net_ptr()->get_outport_dirn2turn(m_direction, output_port_dirn);

      //      if( (m_router->get_id() == 38) && (m_router->is_deadlock()) )
      //std::cout<<"\nprobe received at node 38 for outport: "<<output_port<<" at cycle: "<<m_router->curCycle()<<std::flush;

      probe->append_turn(t);
      probe->set_outport(output_port);
      probe->update_curr_router(m_router);
      probe->set_inport(m_router->getRoutingUnit_ref()->get_inport_dirn2idx(m_direction));

      // if((m_router->get_id() == 28) )
      // 	cout<<"\n probe from node "<<t_flit->get_source_id()<<" put in probe-queue for outport dir "<<output_port_dirn
      // 	    <<" , cycle "<<m_router->curCycle()<<flush;

      m_router->getProbeQueue()->insert(probe, true);
    }

  delete t_flit;
}

bool InputUnit::check_inport(int vnet, int outport)
{
  int start = m_vc_per_vnet*vnet;
  int end =  start + m_vc_per_vnet - 2;

  for(int i=start; i<=end; i++)
    {
      if( (m_vcs[i]->get_state() == IDLE_ ) || (m_vcs[i]->peekTopFlit()->get_route().dest_router == m_router->get_id()) )
	{
	  return false;
	}
    }

  for(int i=start; i<=end; i++)
    {
      if(m_vcs[i]->get_outport() == outport )
	return true;
   }

  return false;
}


void InputUnit::copy_vc(int vc_from, int vc_to)
{
  std::cout<<"\nstatic bubble vc "<<vc_from<<" copied to vc "<<vc_to<<" in cycle "<<m_router->curCycle();
  
  assert(m_vcs[vc_to]->get_num_flits() == 0);

  int size = m_vcs[vc_from]->get_num_flits();
  for(int i=0; i<size; i++)
    {
      flit *flt = m_vcs[vc_from]->getTopFlit();
      m_vcs[vc_to]->insertFlit(flt);
    }

  assert(m_vcs[vc_from]->get_num_flits() == 0);

  m_vcs[vc_to]->set_outvc(m_vcs[vc_from]->get_outvc());
  m_vcs[vc_to]->set_outport(m_vcs[vc_from]->get_outport());
  m_vcs[vc_to]->set_enqueue_time(m_vcs[vc_from]->get_enqueue_time());
}

void InputUnit::increment_credit_sb(int in_vc, Cycles curTime)
{
  flit *t_flit = new flit(in_vc, curTime);
  creditQueue->insert(t_flit);
  m_credit_link->scheduleEventAbsolute(m_router->clockEdge(Cycles(1)));
}
