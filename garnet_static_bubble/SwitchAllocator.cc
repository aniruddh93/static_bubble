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

#include "mem/ruby/network/garnet2.0/SwitchAllocator.hh"

#include "debug/RubyNetwork.hh"
#include "mem/ruby/network/garnet2.0/GarnetNetwork.hh"
#include "mem/ruby/network/garnet2.0/InputUnit.hh"
#include "mem/ruby/network/garnet2.0/OutputUnit.hh"
#include "mem/ruby/network/garnet2.0/Router.hh"
#include "mem/ruby/network/garnet2.0/flit.hh"
#include "mem/ruby/network/garnet2.0/RoutingUnit.hh"

#include<iostream>

int reserve_count=0;

SwitchAllocator::SwitchAllocator(Router *router)
    : Consumer(router)
{
    m_router = router;
    m_num_vcs = m_router->get_num_vcs();
    m_vc_per_vnet = m_router->get_vc_per_vnet();

    m_input_arbiter_activity = 0;
    m_output_arbiter_activity = 0;
}

void
SwitchAllocator::init()
{
    m_input_unit = m_router->get_inputUnit_ref();
    m_output_unit = m_router->get_outputUnit_ref();

    m_num_inports = m_router->get_num_inports();
    m_num_outports = m_router->get_num_outports();
    m_round_robin_inport.resize(m_num_outports);
    m_round_robin_invc.resize(m_num_inports);
    m_port_requests.resize(m_num_outports);
    m_vc_winners.resize(m_num_outports);

    if(m_router->get_id() == 0)
      std::cout<<" \nthe no of output ports at router 20 are: "<<m_num_outports;

    for (int i = 0; i < m_num_inports; i++) {
        m_round_robin_invc[i] = 0;
    }

    for (int i = 0; i < m_num_outports; i++) {
        m_port_requests[i].resize(m_num_inports);
        m_vc_winners[i].resize(m_num_inports);

        m_round_robin_inport[i] = 0;

        for (int j = 0; j < m_num_inports; j++) {
            m_port_requests[i][j] = false; // [outport][inport]
        }
    }

    //initialize all outport reservations to false: req. for static bubble scheme

    outport_reserved.resize( m_num_outports );

    for( int i=0; i < m_num_outports; i++)
      {
	outport_reserved[i] = false;
      }
}

void
SwitchAllocator::wakeup()
{


  //  std::cout<<"\nSwitch allocator "<<m_router->get_id()<<" wakeup"<<std::flush;

  //static bubble scheme

  if(m_router->get_net_ptr()->getStaticBubbleSchemeEnabled())
    {
      if( (m_router->get_id()==28) && (m_router->curCycle() == 1848) )
	std::cout<<"\nSwitchAllocator of Router 28 wakesup at cycle 1848"<<std::flush;

      //first do allocation for check probes, then disable, then enable, then probes, then the cache coherence msgs
      allocate_check_probes();
      allocate_disables();
      allocate_enables();
      allocate_probes();
    }

    
      arbitrate_inports(); // First stage of allocation
      arbitrate_outports(); // Second stage of allocation
    

    clear_request_vector();
    check_for_wakeup();
    clear_outport_reserve_vector();
}

void
SwitchAllocator::arbitrate_inports()
{
    // Select a VC from each input in a round robin manner
    // Independent arbiter at each input port
    for (int inport = 0; inport < m_num_inports; inport++) {


      //static bubble scheme: in case of deadlock allow the deadlocked vnet of the input port to have highest priority

      if( (m_router->get_net_ptr()->getStaticBubbleSchemeEnabled()) && (m_router->is_deadlock()) && (inport == m_router->get_io_pbuffer_inport())  )
	{
	  int d_invc = get_invc_sb();
	  
	  if(d_invc >= 0)
	    {
	      int  outport = m_input_unit[inport]->get_outport(d_invc);
	      m_input_arbiter_activity++;
	      m_port_requests[outport][inport] = true;
	      m_vc_winners[outport][inport]= d_invc;

	      if( (m_router->get_id() == 37) && (m_router->is_deadlock()) )
		std::cout<<" vc request forwarded at router 37 at cycle "<<m_router->curCycle()<<" is "<<d_invc<<std::flush;
	    }
	  continue;
	}

        int invc = m_round_robin_invc[inport];

        // Select next round robin vc candidate within valid vnet
        int next_round_robin_invc = invc;
        next_round_robin_invc++;

	if (next_round_robin_invc >= m_num_vcs)
            next_round_robin_invc = 0;
        m_round_robin_invc[inport] = next_round_robin_invc;

        for (int invc_iter = 0; invc_iter < m_num_vcs; invc_iter++) {

            if (m_input_unit[inport]->need_stage(invc, SA_, m_router->curCycle()))
            {
                int  outport = m_input_unit[inport]->get_outport(invc);
                int  outvc   = m_input_unit[inport]->get_outvc(invc);
                bool make_request = send_allowed(inport, invc, outport, outvc);
                if (make_request)
                {
                    m_input_arbiter_activity++;
                    m_port_requests[outport][inport] = true;
                    m_vc_winners[outport][inport]= invc;
                    break; // got one vc winner for this port
                }
            }

            invc++;
            if (invc >= m_num_vcs)
                invc = 0;
        }
    }
}

void
SwitchAllocator::arbitrate_outports()
{
    // Now there are a set of input vc requests for output vcs.
    // Again do round robin arbitration on these requests
    // Independent arbiter at each output port
    for (int outport = 0; outport < m_num_outports; outport++) {

      int inport;

      //static bubble scheme: give highest priority to deadlocked inport 
      if( (m_router->get_net_ptr()->getStaticBubbleSchemeEnabled()) && (m_router->is_deadlock()) )
	{
	  inport = m_router->get_io_pbuffer_inport();
	}
      else
	{
	  inport = m_round_robin_inport[outport];
	  m_round_robin_inport[outport]++;

	  if (m_round_robin_inport[outport] >= m_num_inports)
            m_round_robin_inport[outport] = 0;
	}

        for (int inport_iter = 0; inport_iter < m_num_inports; inport_iter++) {

            // inport has a request this cycle for outport
            if (m_port_requests[outport][inport]) {

                // grant this outport to this inport
                int invc = m_vc_winners[outport][inport];
                int outvc = m_input_unit[inport]->get_outvc(invc);
                if (outvc == -1)
                {
                    // VC Allocation - select any free VC from outport
                    outvc = vc_allocate(outport, inport, invc);
                }

                // remove flit from Input VC
                flit *t_flit = m_input_unit[inport]->getTopFlit(invc);

                DPRINTF(RubyNetwork, "SwitchAllocator at Router %d \
                                      granted outvc %d at outport %d \
                                      to invc %d at inport %d at time: %lld\n",
                        m_router->get_id(), outvc,
                        m_router->getPortDirectionName(
                            m_output_unit[outport]->get_direction()),
                        invc,
                        m_router->getPortDirectionName(
                            m_input_unit[inport]->get_direction()),
                        m_router->curCycle());


                // flit ready for Switch Traversal
                // the outport was already updated in the flit
                // in the InputUnit (after route_compute)
                t_flit->advance_stage(ST_, m_router->curCycle());

                // update outport field in switch
                // (used by switch code to send it out of correct outport
                t_flit->set_outport(outport);

                // set outvc (i.e., invc for next hop) in flit
                t_flit->set_vc(outvc);
                m_output_unit[outport]->decrement_credit(outvc);

                m_router->grant_switch(inport, t_flit);
                m_output_arbiter_activity++;

                if ( (t_flit->get_type() == TAIL_) || (t_flit->get_type() == HEAD_TAIL_) ) 
		  {
                    // This Input VC should now be empty
                    assert(m_input_unit[inport]->isReady(invc,
                        m_router->curCycle()) == false);

		    //static bubble scheme: if static bubble idle, change counter state
		    if( (m_router->get_net_ptr()->getStaticBubbleSchemeEnabled()) && (m_router->get_counter_state() == S_STATIC_BUBBLE_ACTIVE) 
			&& ( inport == m_router->get_io_pbuffer_inport() ) && (get_vnet(invc) == m_router->get_io_pbuffer_vnet())  )
		      {
			assert(m_router->is_deadlock());

			//if static bubble was not freed: free it
			if( (invc%m_vc_per_vnet) != (m_vc_per_vnet - 1) )
			  {
			    int sb_vnet = m_router->get_io_pbuffer_vnet();
			    int sb_vc = sb_vnet*m_vc_per_vnet + m_vc_per_vnet - 1;
			    assert(m_input_unit[inport]->get_vc_state(sb_vc) == ACTIVE_);
			    m_input_unit[inport]->copy_vc(sb_vc,invc);
			    m_input_unit[inport]->set_vc_active(invc, m_router->curCycle());
			  }

			//set static bubble off
			m_router->switch_off_sb();

			if( m_input_unit[inport]->check_inport(m_router->get_io_pbuffer_vnet(), m_router->get_io_pbuffer_outport()) )
			  {
			    m_router->set_counter(ZERO, m_router->get_counter_direction(), m_router->get_counter_vc_num(), S_CHECK_PROBE, DD);
			    m_router->send_check_probe();
			  }
			else
			  {
			    m_router->set_is_deadlock(false);
			    m_router->set_counter(ZERO, m_router->get_counter_direction(), m_router->get_counter_vc_num(), S_ENABLE, DE);
			    m_router->send_enable();
			  }
		      }
		    else
		      {
			
			// Free this VC
			m_input_unit[inport]->set_vc_idle(invc, m_router->curCycle());

			// Send a credit back
			// along with the information that this VC is now idle
			m_input_unit[inport]->increment_credit(invc, true,
							       m_router->curCycle());
		      }
		    
		    //  if( (m_router->get_id() == 37) && (m_router->is_deadlock()) )
		    //std::cout<<"\n flit "<<t_flit->get_id()<<" leaves node 37 from outport "<<t_flit->get_outport()<<" at cycle "<<m_router->curCycle()<<std::flush;


                } else {
                    // Send a credit back
                    // but do not indicate that the VC is idle

		  if(invc<0) std::cout<<"\nhello world"<<std::flush;

		  assert( (t_flit->get_type() == HEAD_ ) || (t_flit->get_type() == BODY_ ) );
                    m_input_unit[inport]->increment_credit(invc, false,
                        m_router->curCycle());
                }

                // remove this request
                m_port_requests[outport][inport] = false;


		//static bubble scheme : increment round robin counter pointer
		if((m_router->get_net_ptr()->getStaticBubbleSchemeEnabled()) && (m_router->is_static_bubble_node()) 
		   && (m_router->get_counter_state() == S_DEADLOCK_DETECTION))
		  {
		    incr_cntr_pointer();
		  }


                break; // got a input winner for this outport
            }

            inport++;
            if (inport >= m_num_inports)
                inport = 0;
        }
    }
}

bool
SwitchAllocator::send_allowed(int inport, int invc, int outport, int outvc)
{

  //check if port already reserved for probe/enable/disable msgs
  if(outport_reserved[outport])
    return false;

  //static bubble scheme: send flits according to priority during deadlock
  if( ( m_router->get_net_ptr()->getStaticBubbleSchemeEnabled() ) && ( m_router->is_deadlock() ) )
    {
      if( (outport == m_router->get_io_pbuffer_outport()) && (inport != m_router->get_io_pbuffer_inport()) )
	return false;

      //stop injection from the node
      if(m_router->getRoutingUnit_ref()->get_inport_idx2dirn(inport) == L_ )
	return false;
    }

    PortDirection inport_dirn  = m_input_unit[inport]->get_direction();
    PortDirection outport_dirn = m_output_unit[outport]->get_direction();

    // Check if outvc needed
    // Check if credit needed (for multi-flit packet)
    // Check if ordering violated (in ordered vnet)

    int vnet = get_vnet(invc);
    bool has_outvc = (outvc != -1);
    bool has_credit = false;

    RouteInfo route=m_input_unit[inport]->peekTopFlit(invc)->get_route();

    if (has_outvc == false) // needs outvc
    {
      if (m_output_unit[outport]->has_free_vc(vnet, inport_dirn, outport_dirn, invc, route))
        {
            has_outvc = true;
            has_credit = true; // each VC has at least one buffer, so no need for additional credit check
        }
    }
    else
    {
        has_credit = m_output_unit[outport]->has_credit(outvc);
    }

    if (!has_outvc || !has_credit)
        return false;


    // protocol ordering check
    if ((m_router->get_net_ptr())->isVNetOrdered(vnet))
    {
        Cycles t_enqueue_time = m_input_unit[inport]->get_enqueue_time(invc);
        int vc_base = vnet*m_vc_per_vnet;
        for (int vc_offset = 0; vc_offset < m_vc_per_vnet; vc_offset++) {
            int temp_vc = vc_base + vc_offset;
            if (m_input_unit[inport]->need_stage(temp_vc, SA_,
                                                 m_router->curCycle()) &&
               (m_input_unit[inport]->get_outport(temp_vc) == outport) &&
               (m_input_unit[inport]->get_enqueue_time(temp_vc) <
                    t_enqueue_time)) {
                return false;
            }
        }
    }

    return true;
}

int
SwitchAllocator::vc_allocate(int outport, int inport, int invc)
{
    PortDirection inport_dirn  = m_input_unit[inport]->get_direction();
    PortDirection outport_dirn = m_output_unit[outport]->get_direction();

    RouteInfo route=m_input_unit[inport]->peekTopFlit(invc)->get_route();

    // Select a free VC from the output port
    int outvc = m_output_unit[outport]->select_free_vc(get_vnet(invc), inport_dirn, outport_dirn,invc, route );

    if( (outvc % m_vc_per_vnet) == (m_vc_per_vnet -1) )
    std::cout<<"\n static bubble allocated at node: "<<m_router->get_id()<<std::flush;

    assert(outvc != -1); // has to get a valid VC since it checked before performing SA
    m_input_unit[inport]->grant_outvc(invc, outvc);
    return outvc;
}

void
SwitchAllocator::check_for_wakeup()
{
    Cycles nextCycle = m_router->curCycle() + Cycles(1);

    for (int i = 0; i < m_num_inports; i++) {
        for (int j = 0; j < m_num_vcs; j++) {
            if (m_input_unit[i]->need_stage(j, SA_, nextCycle)) {
                m_router->schedule_wakeup(Cycles(1));
                return;
            }
        }
    }
}

int
SwitchAllocator::get_vnet(int invc)
{
    int vnet = invc/m_vc_per_vnet;
    assert(vnet < m_router->get_num_vnets());
    return vnet;
}

void
SwitchAllocator::clear_request_vector()
{
    for (int i = 0; i < m_num_outports; i++) {
        for (int j = 0; j < m_num_inports; j++) {
            m_port_requests[i][j] = false;
        }
    }
}


//  static bubble scheme 

void SwitchAllocator::init_sb_scheme()
{
  m_num_vcs = m_router->get_num_vcs();
  m_vc_per_vnet = m_router->get_vc_per_vnet();
}

void SwitchAllocator::allocate_check_probes()
{
  if(m_router->getCheck_probeQueue()->isEmpty())
    return;

  assert(m_router->is_deadlock());
  assert(m_router->getCheck_probeQueue()->get_size() == 1); //can be involved in only one deadlock: so can receive only one check_probe

  flit *flt = m_router->getCheck_probeQueue()->getTopFlit();
  assert( flt->get_type() == CHECK_PROBE_ );

  flt->advance_stage(ST_, m_router->curCycle());
  m_router->grant_switch(flt->get_inport(), flt);
  outport_reserved[flt->get_outport()] = true;
  assert(m_router->getCheck_probeQueue()->get_size() == 0);
}

void SwitchAllocator::allocate_disables()
{
  if(m_router->getDisableQueue()->isEmpty())
    return;

  // already in deadlock state or already sent out a disable/enable, drop all disable msgs
  if( m_router->is_deadlock() )  
    {
      std::cout<<"\ndisable dropped at node: "<<m_router->get_id()<<" because already in deadlock and counter state is "
	       <<m_router->get_counter_state()<<" and disable request stored at router is from node: "<<m_router->get_io_pbuffer_node_id()<<std::flush;
      m_router->getDisableQueue()->clearQueue();
      return;
    }
 
  else if( (m_router->get_counter_state() == S_DISABLE) || (m_router->get_counter_state() == S_ENABLE) )
    {
      while( !(m_router->getDisableQueue()->isEmpty()) )
	{
	  flit *flt = m_router->getDisableQueue()->getTopFlit();
	  assert( flt->get_type() == DISABLE_ );

	  if( (flt->get_source_id() == m_router->get_id()) )
	    {
	      //assign output port
	      flt->advance_stage(ST_, m_router->curCycle());
	      m_router->grant_switch(flt->get_inport(), flt);
	      outport_reserved[flt->get_outport()] = true;   // reserve the outport for disable msg
	      m_router->getDisableQueue()->clearQueue();   //drop the remaining disable msgs
	      assert(m_router->getDisableQueue()->isEmpty());
	      return;
	    }
	  else
	    {
	      std::cout<<"\ndisable from node "<<flt->get_source_id()<<" dropped at node "<<m_router->get_id()
		       <<" because in S_disable/en state, cycle "<<m_router->curCycle()<<std::flush;
	      delete flt; //drop the disable
	    }
	}
    }

  else
    {
      flit *flt = m_router->getDisableQueue()->getTopFlit();
      assert( flt->get_type() == DISABLE_ );
      m_router->process_disable_msg(flt);

      //assign output port
      flt->advance_stage(ST_, m_router->curCycle());
      m_router->grant_switch(flt->get_inport(), flt);
      outport_reserved[flt->get_outport()] = true;   // reserve the outport for disable msg
      m_router->getDisableQueue()->clearQueue();   //drop the remaining disable msgs
      assert(m_router->getDisableQueue()->isEmpty());

      //if( (m_router->get_id()==20) && (m_router->curCycle()==385) && (flt->get_source_id() == 46) )
      //std::cout<<"\nSwitchAllocator of Router 20 allocated outport at cycle 385 for outport "<<flt->get_outport()<<" an inport "<<flt->get_inport()<<std::flush;

      return;
    }
  
}

void SwitchAllocator::allocate_enables()
{
  if(m_router->getEnableQueue()->isEmpty())
    return;

  if(m_router->is_deadlock())
    {
      if(m_router->getEnableQueue()->peekTopFlit()->get_source_id() == m_router->get_io_pbuffer_node_id())
	{
	  flit *flt = m_router->getEnableQueue()->getTopFlit();
	  assert( flt->get_type() == ENABLE_ );
	  m_router->process_enable_msg(flt);

	  assert(!outport_reserved[flt->get_outport()]);   // disable and enable from the same node cannot arrive at the same time
	  
	  flt->advance_stage(ST_, m_router->curCycle());
	  m_router->grant_switch(flt->get_inport(), flt);
	  outport_reserved[flt->get_outport()] = true;   // reserve the outport for enable msg
	}

      //drop other enables if in deadlock: this is to allow packets to use ports
      m_router->getEnableQueue()->clearQueue();
      assert(m_router->getEnableQueue()->isEmpty());
      return;
    }

  while( !(m_router->getEnableQueue()->isEmpty()) )
    {

      flit *flt = m_router->getEnableQueue()->getTopFlit();
      assert( flt->get_type() == ENABLE_ );
      if(outport_reserved[flt->get_outport()])
	{
	  std::cout<<"\nenable from node "<<flt->get_source_id()<<" dropped at node: "<<m_router->get_id()
		   <<" because outport reserved, cycle "<<m_router->curCycle()<<std::flush;
	  delete flt;   //drop the enable, if outport is already reserved
	}
      else
	{
	  flt->advance_stage(ST_, m_router->curCycle());
	  m_router->grant_switch(flt->get_inport(), flt);
	  outport_reserved[flt->get_outport()] = true;   // reserve the outport for enable msg
	  reserve_count++;
	}
    }

  assert(m_router->getEnableQueue()->isEmpty());
}


void SwitchAllocator::allocate_probes()
{
  if(m_router->getProbeQueue()->isEmpty())
    {
      if( (m_router->get_id()==28) && (m_router->curCycle() == 1848) )
	std::cout<<"\nSwitchAllocator of Router 28 has empty probe queue at cycle 1848"<<std::flush;
      return;
    }

  if(m_router->is_deadlock())
    {

      if( (m_router->get_id()==28) && (m_router->curCycle() == 1848) )
	std::cout<<"\nSwitchAllocator of Router 28 is in deadlock at cycle 1848"<<std::flush;
      //drop all the probes when in deadlock: to allow the packets to use the links
      m_router->getProbeQueue()->clearQueue();
      assert(m_router->getProbeQueue()->isEmpty());
      return;
    }

  while( !(m_router->getProbeQueue()->isEmpty()) )
      {	
	flit *flt = m_router->getProbeQueue()->getTopFlit();
	assert( flt->get_type() == PROBE_ );
	if(outport_reserved[flt->get_outport()])
	  {
	    delete flt;   //drop the probe, if outport is already reserved
	    if( (m_router->get_id()==28) && (m_router->curCycle() == 1848) && (flt->get_source_id() == 37) )
	      std::cout<<"\nSwitchAllocator of Router 28 dropped probe outport dirn "<<m_router->getRoutingUnit_ref()->get_outport_idx2dirn(flt->get_outport())
	    	       <<" and inport dirn "<<m_router->getRoutingUnit_ref()->get_outport_idx2dirn(flt->get_inport())
		       <<" because outport reserved at cycle "<<m_router->curCycle()<<std::flush;
	  }
	else
	  {
	    flt->advance_stage(ST_, m_router->curCycle());
	    m_router->grant_switch(flt->get_inport(), flt);
	    outport_reserved[flt->get_outport()] = true;   // reserve the outport for probe msg

	    if( (m_router->get_id()==28) && (m_router->curCycle() == 1848) )
	      std::cout<<"\nSwitchAllocator of Router 28 allocated outport dirn "<<m_router->getRoutingUnit_ref()->get_outport_idx2dirn(flt->get_outport())
	    	       <<" and inport dirn "<<m_router->getRoutingUnit_ref()->get_outport_idx2dirn(flt->get_inport())
		       <<" for probe from "<<flt->get_source_id()<<" at cycle "<<m_router->curCycle()<<std::flush;
	  }
      }

    assert(m_router->getProbeQueue()->isEmpty());

}


void SwitchAllocator::clear_outport_reserve_vector()
{
      for( int i=0; i < m_num_outports; i++)
      {
	outport_reserved[i] = false;
      }
}


void SwitchAllocator::incr_cntr_pointer()
{
  std::pair<PortDirection, int> cntr_next_pntr = m_router->get_cntr_next_pointer();
  
  counter_state cntr_state;

  if(cntr_next_pntr.second == -1)
    cntr_state = S_OFF;
  else
    cntr_state = S_DEADLOCK_DETECTION;

  m_router->set_counter(ZERO, cntr_next_pntr.first, cntr_next_pntr.second, cntr_state, DD);
}

int SwitchAllocator::get_invc_sb()
{
  int vnet = m_router->get_io_pbuffer_vnet();
  int inport = m_router->get_io_pbuffer_inport();
  int start = vnet*m_vc_per_vnet;
  int end = start + m_vc_per_vnet -2;

  if(m_router->is_static_bubble_node())
    {
      if( m_router->get_counter_state() == S_STATIC_BUBBLE_ACTIVE )
	{
	  end++;
	}	  
    }

  //give chance to the static bubble first

  for(int i=end; i >= start; i--)
    {
      if( (m_input_unit[inport]->get_vc_state(i) == ACTIVE_ ) && (m_input_unit[inport]->get_num_flits(i) != 0) )
	{
	  int outport = m_input_unit[inport]->get_outport(i);
	  int  outvc   = m_input_unit[inport]->get_outvc(i);

	  // PortDirection dirr = m_router->getRoutingUnit_ref()->get_outport_idx2dirn(outport);
	  if( (m_router->get_id() ==35) && (inport == m_router->get_io_pbuffer_inport()) && ( (i % m_vc_per_vnet) == (m_vc_per_vnet -1) )  )
	    std::cout<<"\ntrying for sb vc "<<i<<" cycle "<<m_router->curCycle()<<std::flush;
  

	  std::cout<<"\nhello nonsense"<<std::flush;
	  if(send_allowed(inport, i, outport, outvc))
	    {
	      return i;
	    }
	}
    }


  for(int i=0; i<m_num_vcs; i++)
    {
      if( (i % m_vc_per_vnet) == (m_vc_per_vnet -1) )
	continue;

      if( (m_input_unit[inport]->get_vc_state(i) == ACTIVE_ ) && (m_input_unit[inport]->get_num_flits(i) != 0) )
	{
	  int outport = m_input_unit[inport]->get_outport(i);
	  int  outvc   = m_input_unit[inport]->get_outvc(i);

	  std::cout<<"\nhello charlie"<<std::flush;
	  if(send_allowed(inport, i, outport, outvc))
	    {
	      return i;
	    }
	}
    }

  return -1;

}
