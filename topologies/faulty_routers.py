# Copyright, Georgia Institute of Technology

# Authors: Aniruddh Ramrakhyani

# NOTE: This fault model only works for a 8*8 mesh

# This is the fault model used to demonstrate the applicability of static bubble scheme to resiliency domain. 
# For a given no. of faults to be injected into the network, the program generates random numbers to select the 
# router-id of the routers to be disbaled. 

# Compute nodes have been attached to all the routers in the network. If a node sends a message to an unreachable node,
# the message is dropped in the InputUnit since path to the destination doesn't exist in the routing table. The corresponding
# stats are updated.

#for generating random no. 
import random

from m5.params import *
from m5.objects import *

from BaseTopology import SimpleTopology


class faulty_routers(SimpleTopology):
    description='Fault model for routers'


    def __init__(self, controllers):
        self.nodes = controllers

    def makeTopology(self, options, network, IntLink, ExtLink, Router):
        nodes = self.nodes

        num_routers = options.num_cpus
        num_rows = options.num_rows
 
        cntrls_per_router, remainder = divmod(len(nodes), num_routers)
        assert(num_rows <= num_routers)
        num_columns = int(num_routers / num_rows)
        assert(num_columns * num_rows == num_routers)

        # Create the routers in the mesh
        routers = [Router(router_id=i) for i in range(num_routers)]
        network.routers = routers

        # link counter to set unique link ids
        link_count = 0

        # Add all but the remainder nodes to the list of nodes to be uniformly
        # distributed across the network.
        network_nodes = []
        remainder_nodes = []
        for node_index in xrange(len(nodes)):
            if node_index < (len(nodes) - remainder):
                network_nodes.append(nodes[node_index])
            else:
                remainder_nodes.append(nodes[node_index])

        # Connect each node to the appropriate router
        ext_links = []
        for (i, n) in enumerate(network_nodes):
            cntrl_level, router_id = divmod(i, num_routers)
            assert(cntrl_level < cntrls_per_router)
            ext_links.append(ExtLink(link_id=link_count, ext_node=n,
                                    int_node=routers[router_id]))
            link_count += 1

        # Connect the remainding nodes to router 0.  These should only be
        # DMA nodes.
        for (i, node) in enumerate(remainder_nodes):
            assert(node.type == 'DMA_Controller')
            assert(i < remainder)
            ext_links.append(ExtLink(link_id=link_count, ext_node=node,
                                    int_node=routers[0]))
            link_count += 1

        network.ext_links = ext_links

        #Get seed from cmd line: to seed the random no. generator (req. to generate the same faulty topology)
        seed = options.seed
        random.seed(seed)

        #Get no of faults from cmd line
        num_faults = options.num_faults
        
        max_faults = 49  #only a max. of 48 routers out of 64 can be power-gated

        assert(num_faults < max_faults)

        start_link_id = link_count

        #randomly select faulty routers
        faulty_routers = []

        while (len(faulty_routers) < num_faults): 
            frouter_id = random.randint(0, num_routers - 1)
            if(frouter_id not in faulty_routers):
                #print "faulty_router_id = %d" % frouter_id
                faulty_routers.append(frouter_id)

        faulty_link_ids = []
        #find the link-ids corresponding to faulty routers
        for i in range(0, num_faults):
            router_id = faulty_routers[i]
            row_id, col_id = divmod(router_id, 8)

            #East link: not present in terminal nodes of rows
            if(col_id != 7):
                faulty_link_ids.append(link_count + row_id*7 + col_id)

            #West link: not present in starting nodes of rows
            if(col_id != 0):
                faulty_link_ids.append(link_count + row_id*7 + col_id - 1)

            #North link: not present in terminal row of mesh
            if(row_id != 7):
                faulty_link_ids.append(link_count + 56 + row_id*8 + col_id)

            #South link: not present in first row of mesh
            if(row_id != 0):
                faulty_link_ids.append(link_count + 56 + (row_id-1)*8 + col_id)

        

        # Create the mesh links.  First row (east-west) links then column
        # (north-south) links
        int_links = []
        print "%d" % len(faulty_link_ids)
        for row in xrange(num_rows):
            for col in xrange(num_columns):
                if (col + 1 < num_columns):
                    east_id = col + (row * num_columns)
                    west_id = (col + 1) + (row * num_columns)
                    if(link_count not in faulty_link_ids):
                        #print "active_link = %d" % (link_count - start_link_id)
                        int_links.append(IntLink(link_id=link_count,
                                                 node_a=routers[east_id],
                                                 node_b=routers[west_id],
                                                 node_a_port=3, # east port
                                                 node_b_port=1, # west port
                                                 weight=1))
                    link_count += 1

        for row in xrange(num_columns):
            for col in xrange(num_rows):
                if (row + 1 < num_rows):
                    north_id = col + (row * num_columns)
                    south_id = col + ((row + 1) * num_columns)
                    if(link_count not in faulty_link_ids):
                        #print "active_link = %d" % (link_count - start_link_id)
                        int_links.append(IntLink(link_id=link_count,
                                                 node_a=routers[north_id],
                                                 node_b=routers[south_id],
                                                 node_a_port=4, # north port
                                                 node_b_port=2, # south port
                                                 weight=1))
                    link_count += 1

        network.int_links = int_links

