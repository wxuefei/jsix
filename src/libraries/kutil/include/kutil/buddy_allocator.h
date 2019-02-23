#pragma once
/// \file buddy_allocator.h
/// Helper base class for buddy allocators with external node storage.

#include <stdint.h>
#include "kutil/assert.h"
#include "kutil/linked_list.h"
#include "kutil/slab_allocator.h"

namespace kutil {

struct buddy_region;


template<
	unsigned size_min,
	unsigned size_max,
	typename region_type = buddy_region>
class buddy_allocator
{
public:
	/// Constructor.
	/// \arg start   Initial address in the managed range
	/// \arg length  Size of the managed range, in bytes
	buddy_allocator(uintptr_t start, size_t length)
	{
		uintptr_t p = start;
		unsigned size = size_max;

		while (size >= size_min) {
			size_t chunk_size = 1ull << size;

			while (p + chunk_size <= start + length) {
				region_node *r = m_alloc.pop();
				r->size = size_max;
				r->address = p;

				free_bucket(size).sorted_insert(r);
				p += chunk_size;
			}

			size--;
		}
	}

	/// Allocate address space from the managed area.
	/// \arg length  The amount of memory to allocate, in bytes
	/// \returns     The address of the start of the allocated area, or 0 on
	///              failure
	uintptr_t allocate(size_t length)
	{
		unsigned size = size_min;
		while ((1ull << size) < length)
			if (size++ == size_max)
				return 0;

		unsigned request = size;
		while (free_bucket(request).empty())
			if (request++ == size_max)
				return 0;

		region_node *r = nullptr;
		while (request > size) {
			r = free_bucket(request).pop_front();
			region_node *n = split(r);
			request--;

			free_bucket(request).sorted_insert(n);
			if (request != size)
				free_bucket(request).sorted_insert(r);
		}

		if (r == nullptr) r = free_bucket(size).pop_front();
		used_bucket(size).sorted_insert(r);

		return r->address;
	}

	/// Mark a region as allocated.
	/// \arg start   The start of the region
	/// \arg length  The size of the region, in bytes
	/// \returns     The address of the start of the allocated area, or 0 on
	///              failure. This may be less than `start`.
	uintptr_t mark(uintptr_t start, size_t length)
	{
		uintptr_t end = start + length;
		region_node *found = nullptr;

		for (unsigned i = size_max; i >= size_min && !found; --i) {
			for (auto *r : free_bucket(i)) {
				if (start >= r->address && end <= r->end()) {
					found = r;
					break;
				}
			}
		}

		kassert(found, "buddy_allocator::mark called for unknown region");
		if (!found)
			return 0;

		while (found->size > size_min) {
			// Split if the request fits in the second half
			if (start >= found->half()) {
				region_node *other = split(found);
				free_bucket(found->size).sorted_insert(found);
				found = other;
			}

			// Split if the request fits in the first half
			else if (start + length < found->half()) {
				region_node *other = split(found);
				free_bucket(other->size).sorted_insert(other);
			}

			// If neither, we've split as much as possible
			else
				break;
		}

		used_bucket(found->size).sorted_insert(found);
		return found->address;
	}

	/// Free a previous allocation.
	/// \arg p  An address previously retuned by allocate()
	void free(uintptr_t p)
	{
		region_node *found = find(p, true);

		kassert(found, "buddy_allocator::free called for unknown region");
		if (!found)
			return;

		used_bucket(found->size).remove(found);
		free_bucket(found->size).sorted_insert(found);

		while (auto *bud = get_buddy(found)) {
			region_node *eld = found->elder() ? found : bud;
			region_node *other = found->elder() ? bud : found;

			free_bucket(other->size).remove(other);
			m_alloc.push(other);

			free_bucket(eld->size).remove(eld);
			eld->size++;
			free_bucket(eld->size).sorted_insert(eld);

			found = eld;
		}
	}

protected:

	using region_node = list_node<region_type>;
	using region_list = linked_list<region_type>;

	/// Split a region of the given size into two smaller regions, returning
	/// the new latter half
	region_node * split(region_node *reg)
	{
		region_node *other = m_alloc.pop();

		other->size = --reg->size;
		other->address = reg->address + (1ull << reg->size);
		return other;
	}

	/// Find a node with the given address
	region_node * find(uintptr_t p, bool used = true)
	{
		for (unsigned size = size_max; size >= size_min; --size) {
			auto &list = used ? used_bucket(size) : free_bucket(size);
			for (auto *f : list) {
				if (f->address < p) continue;
				if (f->address == p) return f;
				break;
			}
		}
		return nullptr;
	}

	/// Helper to get the buddy for a node, if it's adjacent
	region_node * get_buddy(region_node *r)
	{
		region_node *b = r->elder() ? r->next() : r->prev();
		if (b && b->address == r->buddy())
			return b;
		return nullptr;
	}

	region_list & used_bucket(unsigned size) { return m_used[size - size_min]; }
	region_list & free_bucket(unsigned size) { return m_free[size - size_min]; }

	static const unsigned buckets = (size_max - size_min);

	region_list m_free[buckets];
	region_list m_used[buckets];
	slab_allocator<region_type> m_alloc;
};


struct buddy_region
{
	inline int compare(const buddy_region *o) const {
		return address > o->address ? 1 : \
			address < o->address ? -1 : 0;
	}

	inline uintptr_t end() const { return address + (1ull << size); }
	inline uintptr_t half() const { return address + (1ull << (size - 1)); }
	inline uintptr_t buddy() const { return address ^ (1ull << size); }
	inline bool elder() const { return address < buddy(); }

	uint16_t size;
	uintptr_t address;
};

} // namespace kutil
