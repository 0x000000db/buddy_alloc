# Buddy Allocator

A very simple buddy allocator, it uses a 1-byte tree for bookkeeping (which wastes 6 bits per node). It uses 4 signals in the tree nodes, free (0), split (1), used (2) and allocated (3). Split means either the left tree is allocated or the right is, used means both sides are allocated.

A sample representation of the tree with F = free, S = split, U = used and A = allocated
|        F         |
|    S   |    F    |
| U  | F |     
|A|A |