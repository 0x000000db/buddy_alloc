#pragma once

#include <memory>

struct buddy_default_allocator
{
	void* alloc(size_t n) { return new uint8_t[n]; }
	void free(void* p) { delete[](uint8_t*)p; }
};

/*
Bookkeeping indexes look like this
|        0       |
|   1    |   2   |
| 3 | 4  | 5 | 6 |

Each level contains 2^n blocks, so the total size required is 2^0 + 2^1 + 2^2.. etc or for short 2^(n+1)-1


*/
template<typename allocator>
class buddy_alloc_core
{
	inline size_t next_pot(size_t x) // Next power of two
	{
		// Non-fixed size..
		size_t n = 1;
		while (n < x)
		{
			n <<= 1;
		}
		return n;
	}

	static constexpr uint8_t BLOCK_FREE = 0; // A block is free 
	static constexpr uint8_t BLOCK_SPLIT = 1; // A block has sub-allocated (either from left or right)
	static constexpr uint8_t BLOCK_USED = 2; // Both left and right children are allocated
	static constexpr uint8_t BLOCK_ALLOC = 3; // This block was used for an allocation

	buddy_default_allocator _allocator;
	uint8_t* _memory;
	uint8_t* _bookkeeping; // 1-byte blocks..
	size_t _arena_size;
	size_t _smallest_block;
	size_t _levels;

	static inline bool in_use(uint8_t u) // Check if a block is allocated or both children are allocated
	{
		return u == BLOCK_USED || u == BLOCK_ALLOC;
	}

	void* alloc_r(size_t n, size_t index, size_t block_size, size_t mem_offset) // Recurse tree
	{
		if (n == block_size) // Since n is rounded to the next POT (and rounded to the smallest block)
							// this condition is guaranteed before recursing out of bound
		{
			if (_bookkeeping[index] == BLOCK_FREE) 
			{
				_bookkeeping[index] = BLOCK_ALLOC; // Take this block
				return _memory + mem_offset;
			}
			return nullptr; // Block is either split or already in use, move back up the tree
		}

		if (_bookkeeping[index] == BLOCK_FREE) // Split block
		{
			size_t c0 = index * 2 + 1; // Next row, left
			size_t c1 = index * 2 + 2; // Next row, right
			_bookkeeping[index] = BLOCK_SPLIT;
			_bookkeeping[c0] = BLOCK_FREE;
			_bookkeeping[c1] = BLOCK_FREE;
		}

		if (_bookkeeping[index] == BLOCK_SPLIT)
		{
			size_t c0 = index * 2 + 1; // Next row, left
			size_t c1 = index * 2 + 2; // Next row, right
			size_t child_block_size = block_size >> 1;
			void* r = alloc_r(n, c0, child_block_size, mem_offset); // First try left, if null try right
			if (r == nullptr)
			{
				r = alloc_r(n, c1, child_block_size, mem_offset + child_block_size); 
			}
			if (in_use(_bookkeeping[c0]) && in_use(_bookkeeping[c1]))
			{
				_bookkeeping[index] = BLOCK_USED; // Signal for future calls there is no free space here
			}
			return r;
		}

		return nullptr;
	}
	bool free_r(void* p, size_t index, size_t block_size, size_t mem_offset)
	{
		void* this_block = (void*)((size_t)_memory + mem_offset);
		if (p == this_block)
		{
			if (_bookkeeping[index] == BLOCK_ALLOC) // Must have this flag to be our block, left children share an address
			{
				_bookkeeping[index] = BLOCK_FREE;
				return true;
			}
		}
		// p > thisBlock
		size_t c0 = index * 2 + 1; // Next row, left side
		size_t c1 = index * 2 + 2; // Next row, right side
		size_t child_block_size = block_size >> 1;
		size_t d = (size_t)p - (size_t)this_block;
		bool freed = false;
		if (d < child_block_size)
		{
			freed = free_r(p, c0, child_block_size, mem_offset);
		}
		else
		{
			freed = free_r(p, c1, child_block_size, mem_offset + child_block_size); // Right side
		}
		if (freed)
		{
			if (_bookkeeping[c0] == BLOCK_FREE && _bookkeeping[c1] == BLOCK_FREE) // If left and right are now both free, mark this block free
			{
				_bookkeeping[index] = BLOCK_FREE; // split->free
			}
			else // either left or right is still in use, go from used->split
			{
				_bookkeeping[index] = BLOCK_SPLIT;
			}
			return true;
		}
		return false;
	}
public:
	buddy_alloc_core(size_t arena_size, size_t levels)
	{
		arena_size = next_pot(arena_size); // Must be POT

		// Init bookkeeping
		size_t bookkeeping_size = (1 << (levels+1)) - 1;
		_bookkeeping = (uint8_t*)_allocator.alloc(bookkeeping_size);
		_bookkeeping[0] = BLOCK_FREE;
		_smallest_block = arena_size >> levels;
		_levels = levels;

		// Init arena
		_arena_size = arena_size;
		_memory = (uint8_t*)_allocator.alloc(arena_size);
	}

	buddy_alloc_core(const buddy_alloc_core&) = delete;
	buddy_alloc_core(buddy_alloc_core&&) = delete;
	buddy_alloc_core& operator=(const buddy_alloc_core&) = delete;
	buddy_alloc_core& operator=(buddy_alloc_core&&) = delete;

	~buddy_alloc_core()
	{
		_allocator.free(_memory);
		_memory = nullptr;
		_allocator.free(_bookkeeping);
		_bookkeeping = nullptr;
	}

	void* alloc(size_t n)
	{
		n = next_pot(n); // Round up to power of two
		if (n < _smallest_block)
		{
			n = _smallest_block; // Ensure the block size is one available
		}
		if (n <= _arena_size)
		{
			return alloc_r(n, 0, _arena_size, 0);
		}
		return nullptr;
	}

	void free(void* p)
	{
		free_r(p, 0, _arena_size, 0);
	}

	size_t get_levels() const { return _levels; }
	const uint8_t* get_tree() const { return _bookkeeping; }
};

typedef buddy_alloc_core<buddy_default_allocator> buddy_alloc;