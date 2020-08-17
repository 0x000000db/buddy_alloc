#include <memory>
#include "buddy_alloc.h"

int main()
{
	buddy_alloc ba(128, 5);
	void* a = ba.alloc(24);
	void* b = ba.alloc(21);
	void* c = ba.alloc(19);
	void* d = ba.alloc(7);
	void* e = ba.alloc(7);
	void* f = ba.alloc(7); 
	void* g = ba.alloc(1); 
	void* h = ba.alloc(1); 
	void* j = ba.alloc(1); // j = nullptr;
	ba.free(h);
	ba.free(g);
	ba.free(f);
	ba.free(e);
	ba.free(d);
	ba.free(c);
	ba.free(b);
	ba.free(a);
}