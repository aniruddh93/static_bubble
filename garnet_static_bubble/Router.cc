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

#include "mem/ruby/network/garnet2.0/Router.hh"

#include "base/stl_helpers.hh"
#include "debug/RubyNetwork.hh"
#include "mem/ruby/network/garnet2.0/CreditLink.hh"
#include "mem/ruby/network/garnet2.0/CrossbarSwitch.hh"
#include "mem/ruby/network/garnet2.0/GarnetNetwork.hh"
#include "mem/ruby/network/garnet2.0/InputUnit.hh"
#include "mem/ruby/network/garnet2.0/NetworkLink.hh"
#include "mem/ruby/network/garnet2.0/OutputUnit.hh"
#include "mem/ruby/network/garnet2.0/RoutingUnit.hh"
#include "mem/ruby/network/garnet2.0/SwitchAllocator.hh"
#include "mem/ruby/network/garnet2.0/flit.hh"

#include <cstdlib>    //for malloc

using namespace std;
using m5::stl_helpers::deletePointers;

Router::Router(const Params *p)
    : BasicRouter(p), Consumer(this)
{

  
    m_virtual_networks = p->virt_nets;
    m_vc_per_vnet = p->vcs_per_vnet;
    m_num_vcs = m_virtual_networks * m_vc_per_vnet;

    m_routing_unit = new RoutingUnit(this);
    m_sw_alloc = new SwitchAllocator(this);
    m_switch = new CrossbarSwitch(this);

    

    m_input_unit.clear();
    m_output_unit.clear();

    //static bubbble scheme: create counter, IO priority buffer, enable, disable and probe queues
    m_counter = (counter *)malloc(sizeof(counter));
    m_io_priority_buffer = (IO_priority_buffer *)malloc(sizeof(IO_priority_buffer));
    probeQueue = new flitBuffer();
    disableQueue = new flitBuffer();
    enableQueue = new flitBuffer();
    check_probeQueue = new flitBuffer();
}

Router::~Router()
{
    deletePointers(m_input_unit);
    deletePointers(m_output_unit);
    delete m_routing_unit;
    delete m_sw_alloc;
    delete m_switch;
}

void
Router::init()
{
    BasicRouter::init();

    m_sw_alloc->init();
    m_switch->init();

    	//static bubble scheme initialization 

	if(m_network_ptr->getStaticBubbleSchemeEnabled())
	  {

	    m_counter->state = S_OFF;
	    m_counter->vc_port_direction = m_input_unit[0]->get_direction();
	    m_counter->vc_num = 0;
	    m_is_deadlock = false;
	    m_direction_bit_buffer_valid = false;
	    
    	    //increment vc count per input port per v_network
	    m_num_vcs+=m_virtual_networks;
	    m_vc_per_vnet++;

	    //init for IO priority buffer
	    m_io_priority_buffer->node_id = -1;
	    m_io_priority_buffer->vnet = -1;
	    m_io_priority_buffer->inport = -1;
	    m_io_priority_buffer->outport = -1;

	    //re-initialize units 
	    
	    for(int i=0;i<m_input_unit.size();i++)
	      {
		m_input_unit[i]->init_sb_scheme();
	      }

	    for(int i=0;i<m_output_unit.size();i++)
	      {
		m_output_unit[i]->init_sb_scheme();
	      }

	    m_sw_alloc->init_sb_scheme();
	    m_switch->init_sb_scheme();

	    int num_rows=m_network_ptr->getNumRows();
	    int num_cols=m_network_ptr->getNumCols();
    
	    int my_x=m_id%num_cols;
	    int my_y=m_id/num_cols;

	    int max_y=num_rows-1;
	    int max_x=num_cols-1;

	    int my_sum=my_x+my_y;
	    int max_sum=max_x+max_y;

	    bool alternate=false;
	    m_is_static_bubble_node = false;
	

	    if((my_x!=0)&&(my_y!=max_y))
	      {
		for(int i=max_y;i>0;i=i-2)
		  {
		    if(my_sum==i)
		      {
			if(alternate)
			  {
			    if(my_x%2!=0)
			      {
				m_is_static_bubble_node=true;
				//cout<<"\nhello: "<<m_id;
				break;
			      }
			  }
			else
			  {
			    m_is_static_bubble_node=true;
			    //cout<<"\nhello: "<<m_id;
			    break;
			  }
		      }
		    alternate= !alternate;
		  }

		alternate=false;

		for(int i=max_y;i<max_sum;i=i+2)
		  {
		    if(my_sum==i)
		      {
			if(alternate)
			  {
			    if(my_x%2!=0)
			      {
				m_is_static_bubble_node=true;
				break;
			      }
			  }
			else
			  {
			    m_is_static_bubble_node=true;
			    break;
			  }
		      }
		    alternate= !alternate;
		  }
	      }
	    
	  }
}

void
Router::wakeup()
{
    DPRINTF(RubyNetwork, "Router %d woke up\n", m_id);


    //static bubble scheme
    if(m_network_ptr->getStaticBubbleSchemeEnabled())
      {
	if(m_is_static_bubble_node)
	  {
	    // increment counter if on
	    if( (m_counter->state != S_OFF) && (m_counter->state != S_STATIC_BUBBLE_ACTIVE) )
	      {
		bool expired= check_counter();     // check if counter has expired
		
		if(expired)
		  {
		    switch(m_counter->state)
		      {
		      case  S_DEADLOCK_DETECTION: 
			{
			  //		  int tempoo_inport = m_routing_unit->get_inport_dirn2idx( m_counter->vc_port_direction);
			  //		  cout<<"\n Inside Router.cc counter vc state is"<<m_input_unit[tempoo_inport]->get_vc_state(m_counter->vc_num)<<flush;
			  send_probe();
			  std::pair<PortDirection, int> cntr_next_pntr = get_cntr_next_pointer();
			  assert(cntr_next_pntr.second != -1);

			  //&& (cntr_next_pntr.first == W_ ) && (cntr_next_pntr.second == 0) 
			  //if( (m_id==37) )
			  // cout<<"\ncntr_next_pntr.first at node 37 is "<<cntr_next_pntr.first<<" cycle: "<<curCycle()<<flush;
			      
			  
			  set_counter(ZERO, cntr_next_pntr.first, cntr_next_pntr.second, S_DEADLOCK_DETECTION, DD);
			  break;
			}
			
		      case  S_DISABLE:
			{
			  set_counter(ZERO, m_counter->vc_port_direction, m_counter->vc_num, S_ENABLE, DE);   //change state to enable and start counter
			  send_enable();
			  break;
			}

		      case S_ENABLE:
			{
			  set_counter(ZERO, m_counter->vc_port_direction, m_counter->vc_num, S_ENABLE, DE);   //reset and start counter again
			  send_enable();
			  break;
			}

		      case S_CHECK_PROBE:
			{
			  m_is_deadlock = false;
			  set_counter(ZERO, m_counter->vc_port_direction, m_counter->vc_num, S_ENABLE, DE);   //change state to enable and start counter
			  send_enable();
			  break;
			}

		      default: 
			{
			  assert(0);
			  break;
			}
			
		      }
		  }
		else
		  {
		    increment_counter();  //increment counter and schedule wakeup for next cycle 
		  }
	      }
	  }

	/*if( (m_id == 28) )
	  {
	    std::cout<<"\n counter state at node 28 at cycle "<<curCycle()<<" is "<<m_counter->state;
	     int tempooo= m_routing_unit->get_inport_dirn2idx(E_);
	    int outport1 = m_input_unit[tempooo]->get_outport(0);
	    int outport2 = m_input_unit[tempooo]->get_outport(1);
	    std::cout<<"\n at node: "<<m_id<<" vc0, vc1 state is: "<<m_input_unit[tempooo]->get_vc_state(0)<<" , "
		     <<m_input_unit[tempooo]->get_vc_state(1)<<" outport direction is "<<m_routing_unit->get_outport_idx2dirn(outport1)
		     <<" , "<<m_routing_unit->get_outport_idx2dirn(outport2)<<" cycle: "<<curCycle()<<std::flush;
	  }*/
      }





    // check for incoming flits
    for (int inport = 0; inport < m_input_unit.size(); inport++)
    {
      m_input_unit[inport]->wakeup();
    }

    // check for incoming credits
    // Note: the credit update is happening before SA
    // buffer turnaround time =
    //     credit traversal (1-cycle) + SA (1-cycle) + Link Traversal (1-cycle)
    // if we want the credit update to take place after SA, this loop should
    // be moved after the SA request
    for (int outport = 0; outport < m_output_unit.size(); outport++)
      {
        m_output_unit[outport]->wakeup();
      }

    // Switch Allocation
    m_sw_alloc->wakeup();

    // Switch Traversal
    m_switch->wakeup();
    

}


void
Router::addInPort(PortDirection inport_dirn,
                  NetworkLink *in_link, CreditLink *credit_link)
{
    int port_num = m_input_unit.size();
    InputUnit *input_unit = new InputUnit(port_num, inport_dirn, this);

    input_unit->set_in_link(in_link);
    input_unit->set_credit_link(credit_link);
    in_link->setLinkConsumer(this);
    credit_link->setSourceQueue(input_unit->getCreditQueue());

    m_input_unit.push_back(input_unit);

    m_routing_unit->addInDirection(inport_dirn, port_num);
}

void
Router::addOutPort(PortDirection outport_dirn,
                   NetworkLink *out_link,
                   const NetDest& routing_table_entry, int link_weight,
                   CreditLink *credit_link)
{
    int port_num = m_output_unit.size();
    OutputUnit *output_unit = new OutputUnit(port_num, outport_dirn, this);

    output_unit->set_out_link(out_link);
    output_unit->set_credit_link(credit_link);
    credit_link->setLinkConsumer(this);
    out_link->setSourceQueue(output_unit->getOutQueue());

    m_output_unit.push_back(output_unit);

    m_routing_unit->addRoute(routing_table_entry);
    m_routing_unit->addWeight(link_weight);
    m_routing_unit->addOutDirection(outport_dirn, port_num);
}

PortDirection
Router::getOutportDirection(int outport)
{
    return m_output_unit[outport]->get_direction();
}

PortDirection
Router::getInportDirection(int inport)
{
    return m_input_unit[inport]->get_direction();
}

int
Router::route_compute(RouteInfo route, int inport, PortDirection inport_dirn, int vc)
{
  return m_routing_unit->outportCompute(route, inport, inport_dirn, vc);
}

void
Router::grant_switch(int inport, flit *t_flit)
{
  if( (t_flit->get_type() == DISABLE_ ) && (t_flit->get_source_id() == 46) && (curCycle() == 385) )
    std::cout<<"\n disable granted switch in router.cc in cycle 385 at router 46"<<std::flush;
    m_switch->update_sw_winner(inport, t_flit);
}

void
Router::schedule_wakeup(Cycles time)
{
    // wake up after time cycles
    scheduleEvent(time);
}

std::string
Router::getPortDirectionName(PortDirection direction)
{
    switch(direction)
    {
        case L_: return "Local"; break;
        case N_: return "North"; break;
        case E_: return "East"; break;
        case S_: return "South"; break;
        case W_: return "West"; break;
        default: return "NoName"; break;
    };
}

void
Router::regStats()
{
    m_buffer_reads
        .name(name() + ".buffer_reads")
        .flags(Stats::nozero)
    ;

    m_buffer_writes
        .name(name() + ".buffer_writes")
        .flags(Stats::nozero)
    ;

    m_crossbar_activity
        .name(name() + ".crossbar_activity")
        .flags(Stats::nozero)
    ;

    m_sw_input_arbiter_activity
        .name(name() + ".sw_input_arbiter_activity")
        .flags(Stats::nozero)
    ;

    m_sw_output_arbiter_activity
        .name(name() + ".sw_output_arbiter_activity")
        .flags(Stats::nozero)
    ;
}

void
Router::collateStats()
{
    for (int j = 0; j < m_virtual_networks; j++) {
        for (int i = 0; i < m_input_unit.size(); i++) {
            m_buffer_reads += m_input_unit[i]->get_buf_read_activity(j);
            m_buffer_writes += m_input_unit[i]->get_buf_write_activity(j);
        }
    }

    m_sw_input_arbiter_activity = m_sw_alloc->get_input_arbiter_activity();
    m_sw_output_arbiter_activity = m_sw_alloc->get_output_arbiter_activity();
    m_crossbar_activity = m_switch->get_crossbar_activity();
}

void
Router::resetStats()
{
    for (int j = 0; j < m_virtual_networks; j++) {
        for (int i = 0; i < m_input_unit.size(); i++) {
            m_input_unit[i]->resetStats();
        }
    }
}

void
Router::printFaultVector(ostream& out)
{
    int temperature_celcius = BASELINE_TEMPERATURE_CELCIUS;
    int num_fault_types = m_network_ptr->fault_model->number_of_fault_types;
    float fault_vector[num_fault_types];
    get_fault_vector(temperature_celcius, fault_vector);
    out << "Router-" << m_id << " fault vector: " << endl;
    for (int fault_type_index = 0; fault_type_index < num_fault_types;
         fault_type_index++){
        out << " - probability of (";
        out <<
        m_network_ptr->fault_model->fault_type_to_string(fault_type_index);
        out << ") = ";
        out << fault_vector[fault_type_index] << endl;
    }
}

void
Router::printAggregateFaultProbability(std::ostream& out)
{
    int temperature_celcius = BASELINE_TEMPERATURE_CELCIUS;
    float aggregate_fault_prob;
    get_aggregate_fault_probability(temperature_celcius,
                                    &aggregate_fault_prob);
    out << "Router-" << m_id << " fault probability: ";
    out << aggregate_fault_prob << endl;
}

uint32_t
Router::functionalWrite(Packet *pkt)
{
    uint32_t num_functional_writes = 0;
    num_functional_writes += m_switch->functionalWrite(pkt);

    for (uint32_t i = 0; i < m_input_unit.size(); i++) {
        num_functional_writes += m_input_unit[i]->functionalWrite(pkt);
    }

    for (uint32_t i = 0; i < m_output_unit.size(); i++) {
        num_functional_writes += m_output_unit[i]->functionalWrite(pkt);
    }

    return num_functional_writes;
}

Router *
GarnetRouterParams::create()
{
    return new Router(this);
}


//static bubble scheme 

void Router::set_counter(unsigned counter_count, PortDirection counter_vc_port_dir, int counter_vc_number, counter_state cntr_state, counter_threshold thresh)
{
  m_counter->m_counter_count = counter_count;
  m_counter->vc_port_direction = counter_vc_port_dir;
  m_counter->vc_num = counter_vc_number;
  m_counter->state = cntr_state;
  m_counter->threshold = thresh;

  if(cntr_state != S_OFF && cntr_state != S_STATIC_BUBBLE_ACTIVE)
    schedule_wakeup(Cycles(1));  //schedule wakeup to increment counter in the next cycle
}

void Router::increment_counter()
{
  m_counter->m_counter_count++;
  schedule_wakeup(Cycles(1));
}

bool Router::check_counter()
{
  if(m_counter->m_counter_count > m_counter->threshold)
    return true;

  return false;
}

void Router::send_probe()
{

  int inport = m_routing_unit->get_inport_dirn2idx(m_counter->vc_port_direction);
  int outport = m_input_unit[inport]->get_outport(m_counter->vc_num);

  assert(m_counter->state == S_DEADLOCK_DETECTION );
  assert(m_input_unit[inport]->get_vc_state(m_counter->vc_num) == ACTIVE_ );
  assert(!m_is_deadlock);

  int vnet = m_input_unit[inport]->peekTopFlit(m_counter->vc_num)->get_vnet();

  flit *probe= new flit(PROBE_ , m_id, outport, inport, curCycle(), vnet, this); 

  probeQueue->insert(probe, true);
  probe->advance_stage(SA_, curCycle());

  if(m_id==37)
    cout<<"\n"<<"probe sent from node: "<<m_id<<" for outport direction "<<m_routing_unit->get_outport_idx2dirn(outport)
	<<" inport direction "<<m_counter->vc_port_direction<<" cycle "<<curCycle()<<flush;
}

void Router::send_disable()
{
  cout<<"\n"<<"disable sent from node: "<<m_id<<" cycle "<<curCycle()<<flush;

  //get the output port from the IO priority buffer
  flit *disable= new flit(DISABLE_ , m_id, m_io_priority_buffer->outport, m_io_priority_buffer->inport, 
			  curCycle(), m_io_priority_buffer->vnet, this); 
  copy_turns(disable);
  disableQueue->insert(disable, true);
  disable->advance_stage(SA_, curCycle());
}

void Router::send_enable()
{
  cout<<"\n"<<"enable sent from node: "<<m_id<<" cycle "<<curCycle()<<flush;

  //get the output port from the IO priority buffer
  flit *enable= new flit(ENABLE_ , m_id, m_io_priority_buffer->outport, m_io_priority_buffer->inport, 
			 curCycle(), m_io_priority_buffer->vnet, this);
  copy_turns(enable);
  enableQueue->insert(enable, true);
  enable->advance_stage(SA_, curCycle());
}

void Router::process_disable_msg(flit *flt)
{
  if(flt->get_source_id() == m_id)
    return;

  if( (m_id==19) && (curCycle()==387) )
    std::cout<<"\nDisable processed at Router 19 allocated outport at cycle 387"<<std::flush;

  m_is_deadlock = true;
  int inport = flt->get_inport();
  int outport = flt->get_outport();
  PortDirection output_port_dirn = m_routing_unit->get_outport_idx2dirn(outport);
  PortDirection input_port_dirn = m_routing_unit->get_inport_idx2dirn(inport);
  set_IO_priority_buffer(flt->get_source_id(), flt->get_vnet(), input_port_dirn, inport, output_port_dirn, outport);

  //  std::cout<<"\ndisable processed at node: "<<m_id<<" with outport direction: "<<output_port_dirn<<std::flush;

  //switch off the counter if static bubble node
  
  if(m_is_static_bubble_node)
    {
      set_counter(ZERO, m_counter->vc_port_direction, m_counter->vc_num, S_OFF, DD);
    }
}

void Router::process_enable_msg(flit *flt)
{
    if(flt->get_source_id() == m_id)
    return;

  m_is_deadlock = false;

  //switch on counter if static bubble node

  if(m_is_static_bubble_node)
    {
      if( (m_counter->state != S_DISABLE) && (m_counter->state != S_ENABLE) )
	{
	  std::pair<PortDirection, int> cntr_next_pntr = get_cntr_next_pointer();
      
	  counter_state cntr_state;
      
	  if(cntr_next_pntr.second == -1)
	    cntr_state = S_OFF;
	  else
	    cntr_state = S_DEADLOCK_DETECTION;
	  set_counter(ZERO, cntr_next_pntr.first, cntr_next_pntr.second, cntr_state, DD);
	}
    }
}

void Router::latch_deadlock_path(flit *probe)
{
  m_deadlock_path.clear();
  int size = probe->get_num_turns();
  std::cout<<"\ndeadlocked path latched at node "<<m_id<<" is: "<<flush;

  for( int i=0; i<size; i++)
    {
      turn t = probe->get_top_turn();
      m_deadlock_path.push_back(t);
      std::cout<<t<<std::flush;
    }
}



void Router::set_IO_priority_buffer(int node_id, int vnet, PortDirection input_port_dirn, int inport, PortDirection output_port_dirn, int outport)
{
  m_io_priority_buffer->node_id = node_id;
  m_io_priority_buffer->vnet = vnet;
  m_io_priority_buffer->input_port_dirn = input_port_dirn;
  m_io_priority_buffer->inport = inport;
  m_io_priority_buffer->output_port_dirn = output_port_dirn;
  m_io_priority_buffer->outport = outport;
}

void Router::copy_turns(flit *flt)
{
  for(int i=0; i<m_deadlock_path.size(); i++)
    {
      flt->append_turn(m_deadlock_path[i]);
    }
}

std::pair<PortDirection, int> Router::get_cntr_next_pointer()
{
  std::pair<PortDirection, int> next_pointer;
  int vc_num = m_counter->vc_num + 1;
  PortDirection dir = m_counter->vc_port_direction;
  int inport = m_routing_unit->get_inport_dirn2idx(dir);
  int num_inports = m_input_unit.size();

  assert(dir != L_ );

  for(int iter=0; iter<num_inports; iter++)
    {
      if(dir == L_ )
	{
	  vc_num = 0;
	  inport++;

	  if(inport >= num_inports)
	    inport=0;

	  dir = m_routing_unit->get_inport_idx2dirn(inport);
	  continue;
	}
      
      while(vc_num < m_num_vcs)
	{
	  if( m_input_unit[inport]->get_vc_state(vc_num) == ACTIVE_ )
	    {
	      next_pointer.first = dir;
	      next_pointer.second = vc_num;
	      return next_pointer;
	    }
	  
	  vc_num++;
	}
      
      vc_num = 0;
      inport++;

      if(inport >= num_inports)
	inport=0;

      dir = m_routing_unit->get_inport_idx2dirn(inport);
    }

  
  vc_num = m_counter->vc_num + 1;
  dir = m_counter->vc_port_direction;
  inport = m_routing_unit->get_inport_dirn2idx(dir);

  for(int i=0; i<vc_num; i++)
    {
      if( m_input_unit[inport]->get_vc_state(i) == ACTIVE_ )
	{
	  next_pointer.first = dir;
	  next_pointer.second = i;
	  return next_pointer;
	}
    }

  next_pointer.first = dir;
  next_pointer.second = -1;

  return next_pointer;
}

void Router::switch_off_sb()
{

  assert(m_is_deadlock);
  
  int vnet = m_io_priority_buffer->vnet ;
  int vc = vnet*m_vc_per_vnet + m_vc_per_vnet -1;
  int inport = m_io_priority_buffer->inport;

  cout<<"\n static bubble "<<vc<<" of router "<<m_id<<" switched off at cycle "<<curCycle()<<flush;

  m_input_unit[inport]->set_vc_off(vc, curCycle());

  m_input_unit[inport]->increment_credit_sb(vc, curCycle());
  
}

flitBuffer* Router::getProbeQueue() 
{ 
  return probeQueue; 
}

flitBuffer* Router::getDisableQueue() 
{ 
  return disableQueue; 
}

flitBuffer* Router::getEnableQueue() 
{
  return enableQueue; 
}

void Router::send_check_probe()
{
  cout<<"\n"<<"check probe sent from node: "<<m_id<<" cycle "<<curCycle()<<flush;

  //get the output port from the IO priority buffer
  flit *check_probe = new flit(CHECK_PROBE_ , m_id, m_io_priority_buffer->outport, m_io_priority_buffer->inport, 
			 curCycle(), m_io_priority_buffer->vnet, this);
  copy_turns(check_probe);
  check_probeQueue->insert(check_probe, true);
  check_probe->advance_stage(SA_, curCycle());  
}

flitBuffer* Router::getCheck_probeQueue()
{
  return check_probeQueue;
}
