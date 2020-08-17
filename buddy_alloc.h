#include <memory>

struct buddy_default_allocator
{
	void* alloc(size_t n) { return new uint8_t[n]; }
	void free(void* p) { delete[](uint8_t*)p; }
};

template<typename allocator>
class buddy_alloc_core
{
	inline size_t next_pot(size_t x)
	{
		// Non-fixed size..
		size_t n = 1;
		while (n < x)
		{
			n <<= 1;
		}
		return n;
	}

	static constexpr uint8_t BLOCK_FREE = 0;
	static constexpr uint8_t BLOCK_SPLIT = 1;
	static constexpr uint8_t BLOCK_USED = 2;
	static constexpr uint8_t BLOCK_ALLOC = 3;

	buddy_default_allocator _allocator;
	uint8_t* _memory;
	uint8_t* _bookkeeping; // 1-byte blocks..
	size_t _arenaSize;
	size_t _smallestBlock;
	size_t _levels;

	bool in_use(uint8_t u)
	{
		return u == BLOCK_USED || u == BLOCK_ALLOC;
	}

	void* alloc_r(size_t n, size_t index, size_t blockSize, size_t memOffset)
	{
		if (n == blockSize) 
		{
			if (_bookkeeping[index] == BLOCK_FREE) // Take this block
			{
				_bookkeeping[index] = BLOCK_ALLOC;
				return _memory + memOffset;
			}
			return nullptr; // Either split or already in use
		}

		if (_bookkeeping[index] == BLOCK_FREE) // Split block
		{
			size_t c0 = index * 2 + 1;
			size_t c1 = index * 2 + 2;
			_bookkeeping[index] = BLOCK_SPLIT;
			_bookkeeping[c0] = BLOCK_FREE;
			_bookkeeping[c1] = BLOCK_FREE;
		}

		if (_bookkeeping[index] == BLOCK_SPLIT)
		{
			size_t c0 = index * 2 + 1;
			size_t c1 = index * 2 + 2;
			size_t childBlock = blockSize >> 1;
			void* r = alloc_r(n, c0, childBlock, memOffset);
			if (r == nullptr)
			{
				r = alloc_r(n, c1, childBlock, memOffset + childBlock);
			}
			if (in_use(_bookkeeping[c0]) && in_use(_bookkeeping[c1]))
			{
				_bookkeeping[index] = BLOCK_USED;
			}
			return r;
		}

		return nullptr;
	}
	bool free_r(void* p, size_t index, size_t blockSize, size_t memOffset)
	{
		void* thisBlock = (void*)((size_t)_memory + memOffset);
		if (p == thisBlock)
		{
			if (_bookkeeping[index] == BLOCK_ALLOC) // Found our block
			{
				_bookkeeping[index] = BLOCK_FREE;
				return true;
			}
		}
		// p > thisBlock
		size_t c0 = index * 2 + 1;
		size_t c1 = index * 2 + 2;
		size_t childBlock = blockSize >> 1;
		size_t d = (size_t)p - (size_t)thisBlock;
		bool freed = false;
		if (d < childBlock)
		{
			freed = free_r(p, c0, childBlock, memOffset);
		}
		else
		{
			freed = free_r(p, c1, childBlock, memOffset + childBlock); // Right side
		}
		if (freed)
		{
			if (_bookkeeping[c0] == BLOCK_FREE && _bookkeeping[c1] == BLOCK_FREE) // Join blocks
			{
				_bookkeeping[index] = BLOCK_FREE;
				return true;
			}
		}
		return false;
	}
public:
	buddy_alloc_core(size_t arenaSize, size_t levels)
	{
		arenaSize = next_pot(arenaSize); // Must be POT

		// Init bookkeeping
		size_t n = 0;
		for (size_t l = 0; l <= levels; l++)
		{
			n += (1 << l); // 2^n
		}
		_bookkeeping = (uint8_t*)_allocator.alloc(n);
		_bookkeeping[0] = BLOCK_FREE;
		_smallestBlock = arenaSize >> levels;
		_levels = levels;

		// Init arena
		_arenaSize = arenaSize;
		_memory = (uint8_t*)_allocator.alloc(arenaSize);
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
		n = next_pot(n);
		if (n < _smallestBlock)
		{
			n = _smallestBlock;
		}
		if (n <= _arenaSize)
		{
			return alloc_r(n, 0, _arenaSize, 0);
		}
		return nullptr;
	}

	void free(void* p)
	{
		free_r(p, 0, _arenaSize, 0);
	}
};

typedef buddy_alloc_core<buddy_default_allocator> buddy_alloc;