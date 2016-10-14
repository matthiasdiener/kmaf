# The kernel Memory Affinity Framework (kMAF)

KMAF is a mechanism to perform automatic thread and data mapping in shared-memory systems.

## Installation
Install kMAF as a new kernel, e.g.:

     $ make
     $ make install
     
## Execution

Rename your binary that you want kMAF to handle so that it ends with ```.x```:

     $ ./app.x
     
Everything else is automatic.


## Publications

The main publications regarding kMAF are:

- Matthias Diener, Eduardo H. M. Cruz, Philippe O. A. Navaux, Anselm Busse, Hans-Ulrich Heiß. “kMAF: Automatic Kernel-Level Management of Thread and Data Affinity.” International Conference on Parallel Architectures and Compilation Techniques (PACT), 2014.
- Matthias Diener, Eduardo H. M. Cruz, Marco A. Z. Alves, Philippe O. A. Navaux, Anselm Busse, Hans-Ulrich Heiss. “Kernel-Based Thread and Data Mapping for Improved Memory Affinity.” IEEE Transactions on Parallel and Distributed Systems (TPDS), 2015.
