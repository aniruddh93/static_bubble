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

#ifndef __MEM_RUBY_NETWORK_GARNET_NETWORKHEADER_HH__
#define __MEM_RUBY_NETWORK_GARNET_NETWORKHEADER_HH__

#include "mem/ruby/common/NetDest.hh"

#define ZERO 0

//static bubble scheme constants
#define Deadlock_detection_threshold 130
#define Disable_enable_threshold 130 //it takes 2 cycle per hop: one in router and other in link
#define Static_bubble_threshold 5

#define MAX_TURNS 63

enum flit_type {HEAD_, BODY_, TAIL_, HEAD_TAIL_, DISABLE_, ENABLE_, PROBE_, CHECK_PROBE_, NUM_FLIT_TYPE_}; //added new flit types for static bubble implementation
enum VC_state_type {IDLE_, VC_AB_, ACTIVE_, OFF_, NUM_VC_STATE_TYPE_}; //new state OFF_ added for static bubble scheme
enum VNET_type {CTRL_VNET_, DATA_VNET_, NULL_VNET_, NUM_VNET_TYPE_};
enum flit_stage {I_, VA_, SA_, ST_, LT_, NUM_FLIT_STAGE_};
enum port_direction_type {L_ = 0, W_ = 1, S_ = 2, E_ = 3, N_ = 4, UNKNOWN_ = 5, NUM_PORT_DIRECTION_TYPE_};
enum RoutingAlgorithm { TABLE_ = 0, XY_ = 1, RANDOM_ = 2, TURN_MODEL_ = 3, NUM_ROUTING_ALGORITHM_};

//static bubble scheme 
enum counter_state {S_OFF, S_DEADLOCK_DETECTION, S_DISABLE, S_ENABLE, S_CHECK_PROBE, S_STATIC_BUBBLE_ACTIVE, NUM_COUNTER_STATES};
enum counter_threshold {DD = Deadlock_detection_threshold , DE = Disable_enable_threshold , SB = Static_bubble_threshold , NUM_THRESHOLDS};
enum turn {LEFT, RIGHT, STRAIGHT, NUM_TURN_TYPES};

struct RouteInfo
{
    // destination format for table-based routing
    NetDest net_dest;

    // destination format for topology-specific routing
    int dest_ni;
    int dest_router;
    int hops;
};



#define INFINITE_ 10000

#endif // __MEM_RUBY_NETWORK_GARNET_NETWORKHEADER_HH__
