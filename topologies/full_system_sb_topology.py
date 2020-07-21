# Copyright, Georgia Institute of Technology

# Authors: Aniruddh Ramrakhyani

# Topology No 2

# Topology created for full system simulation with 4 faults.
# Run this topology with static bubble enabled.

#for generating random no. 
import random

from m5.params import *
from m5.objects import *

from BaseTopology import SimpleTopology


class sb_t2(SimpleTopology):
    description='Full System static bubble topology 2'

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


        #faulty links
        faulty_link_ids = [(link_count + 107), (link_count + 106), (link_count + 6), (link_count + 9)]

        # Create the mesh links.  First row (east-west) links then column
        # (north-south) links
        int_links = []
        for row in xrange(num_rows):
            for col in xrange(num_columns):
                if (col + 1 < num_columns):
                    east_id = col + (row * num_columns)
                    west_id = (col + 1) + (row * num_columns)
                    if(link_count not in faulty_link_ids):
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
                        int_links.append(IntLink(link_id=link_count,
                                                 node_a=routers[north_id],
                                                 node_b=routers[south_id],
                                                 node_a_port=4, # north port
                                                 node_b_port=2, # south port
                                                 weight=1))
                    link_count += 1

        network.int_links = int_links
