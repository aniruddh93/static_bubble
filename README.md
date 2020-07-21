# Static Bubble

This repository contains the code for *Static Bubble : A framework for Deadlock Freedom*
[paper](https://ieeexplore.ieee.org/abstract/document/7920830) that was presented at *2017 IEEE
International Symposium on High Performance Computer Architecture (HPCA)* conference held at Austin, TX,
USA.

The architectural and micro-architectural details of Static Bubble are implemented in
[garnet](https://www.gem5.org/documentation/general_docs/ruby/garnet-2/) (part of
[gem5](https://www.gem5.org) simulator).

Code is organized as follows:

**garnet_static_bubble:** Contains the implementation of Static Bubble (in C++) and baseline routing
algorithms (like minimal routing, adaptive routing, XY, etc.) for comparison.

**topologies:** Contains implementation (in Python) of (a) Mesh topology where only active links are the
one which exist in a Breadth First search tree (constructed with a random root node),
(b) Mesh topology with specified number of faulty links at random locations, and
(c) Faulty Mesh topology with static bubbles.

To run simulations, code in this repro will have to be integrated with rest of gem5 code.

Sample run cmd with options:

       gem5/build/ALPHA_Network_test/gem5.opt gem5/configs/example/ruby_network_test.py
       --network=garnet2.0
       --num-cpus=<No. of cpus in topology>
       --num-dirs=<No. of memory controllers in topology>
       --topology=<One of mesh or fault_model>
       --num-rows=<No. of rows in Mesh topology>
       --vcs-per-vnet=<No. of virtual channels per virtual network>
       --sim-cycles=<No. of simulation cycles>
       --injectionrate=<packet injection rate>
       --synthetic=<traffic pattern like uniform random, transpose, etc.>
       --routing-algorithm=<routing algorithm to be used like minimal, adaptive>
       --enable-sb-fault-model=<1 to enable Static Bubble fault model, 0 to disable it>
       --enable-static-bubble=<1 to enable Static Bubble scheme>
       --dd-thresh=<deadlock detection threshold for static bubble>
       --seed=<seed for random number generator>
       --num-faults=<No. of faults to simulate in topology>
       