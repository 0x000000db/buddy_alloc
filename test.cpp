#include <memory>
#include "buddy_alloc.h"

void dump_r(const uint8_t* bookkeeping, size_t index, size_t width, size_t depth, size_t max_depth)
{
	for (size_t i = 0; i < width; i++)
	{
		printf(" ");
	}
	switch (bookkeeping[index])
	{
	case 0: printf("F"); break; // Free
	case 1: printf("S"); break; // Split
	case 2: printf("U"); break; // Used
	case 3: printf("A"); break; // Allocated
	}
	bool has_children = (bookkeeping[index] == 1 || bookkeeping[index] == 2);
	if (depth != max_depth && has_children)
	{
		printf("\n");
		dump_r(bookkeeping, index * 2 + 1, width + 1, depth + 1, max_depth);
		printf("\n");
		dump_r(bookkeeping, index * 2 + 2, width + 1, depth + 1, max_depth);
	}
}
void dump(buddy_alloc& alloc)
{
	dump_r(alloc.get_tree(), 0, 0, 0, alloc.get_levels());
	printf("\n\n\n");
}

void* alloc(buddy_alloc& alloc, size_t len)
{
	void* p = alloc.alloc(len);
	dump(alloc);
	return p;
}

void release(buddy_alloc& alloc, void* p)
{
	alloc.free(p);
	dump(alloc);
}

int main()
{
	buddy_alloc ba(128, 5);
	dump(ba);
	void* a = alloc(ba, 24);
	void* b = alloc(ba, 21);
	void* c = alloc(ba, 19);
	void* d = alloc(ba, 7);
	void* e = alloc(ba, 7);
	void* f = alloc(ba, 7); 
	void* g = alloc(ba, 1); 
	void* h = alloc(ba, 1); 
	void* j = alloc(ba, 1); // j = nullptr;
	release(ba, h);
	release(ba, g);
	release(ba, f);
	release(ba, e);
	release(ba, d);
	release(ba, c);
	release(ba, b);
	release(ba, a);
}