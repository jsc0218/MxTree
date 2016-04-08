MxTree
======

A metric space index based on the M-tree, implemented according to the following paper:

Shichao Jin, Okhee Kim, Wenya Feng:
MX-tree: A Double Hierarchical Metric Index with Overlap Reduction. ICCSA (5) 2013: 574-589

* C++ implementation
* Object oriented programming
* based on the Gist library (http://gist.cs.berkeley.edu/) 

1. deduplicate the building dataset before constructing the index.

2. set the super node's maximum size X10 of a usual node's size.
