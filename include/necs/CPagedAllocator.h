// //////////////////////////////////////////////////////////////////////////////////////////
// FILE: CPagedAllocator.h
// 
// AUTHOR: Kirichenko Stanislav
// 
// DATE: 30 jan 2025
// 
// LICENSE: BSD-2
// Copyright (c) 2025, Kirichenko Stanislav
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions, and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions, and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// //////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <vector>
#include <cassert>
#include <stdexcept>

#include "IAlignedAllocator.h"

#include "clow/freelist.h"

#include "IPagedAllocator.h"

/**
 * /brief Each allocation is max_align_t aligned.
 */
template<class IAlignedAllocator_T>
class CPagedAllocator final : public IPagedAllocator
{
	static_assert(std::is_base_of<IAlignedAllocator, IAlignedAllocator_T>::value);
public:
	CPagedAllocator(const uint64_t maxNumOfElementsPerSlab, const uint64_t elementSize) : IPagedAllocator(maxNumOfElementsPerSlab, elementSize), _maxNumElementsPerSlab(maxNumOfElementsPerSlab), _slabBytes(maxNumOfElementsPerSlab* (_elementSize + freelist_alloc_overhead())), _elementSize(elementSize) { assert(_slabBytes > 0); }
	CPagedAllocator(CPagedAllocator&& other) noexcept : IPagedAllocator(0,0), _maxNumElementsPerSlab(other._maxNumElementsPerSlab), _slabBytes(other._slabBytes), _elementSize(other._elementSize), _slabs(std::move(other._slabs)), _fullBuckets(std::move(other._fullBuckets)) {
		assert(_slabBytes > 0);
	}
	CPagedAllocator(const CPagedAllocator&) = delete;
	CPagedAllocator& operator=(const CPagedAllocator& other) = delete;

	virtual ~CPagedAllocator()
	{
		for (auto& slab : _slabs)
		{
			_alignedAllocator.Free(freelist_get_buffer(&slab));
			freelist_reset(&slab);
		}
	}

	void* Allocate() override
	{
		const auto allocatorIndex{ _getFreeAllocatorIndex() };
		auto& allocator{ _slabs[allocatorIndex] };

		freelist_verify_corruption(&allocator);
		void* allocation{ freelist_malloc(&allocator, _elementSize) };
		assert(freelist_get_allocation_size(&allocator, allocation) == _elementSize);

		// If full mark as full in the bitset
		void* nextAlloc{ freelist_malloc(&allocator, _elementSize) };
		if (nextAlloc == nullptr)
		{
			_fullBuckets[allocatorIndex] = true;
		}
		else
			freelist_free(&allocator, nextAlloc);

		freelist_verify_corruption(&allocator);


		return allocation;
	}

	void Free(void* ptr) override {
		if (!_slabs.size())
			return;

		auto comp = [](freelist& allocator, void* ptr) {
			return reinterpret_cast<std::uintptr_t>(freelist_get_buffer(&allocator)) + allocator.buffer_size < reinterpret_cast<std::uintptr_t>(ptr);
			};

		// Use std::lower_bound with a custom comparator
		if (_slabs.size() > 1)
		{
			const std::vector<freelist>::iterator it{ std::lower_bound(_slabs.begin(), _slabs.end(), (void*)ptr, comp) };
			// Do nothing if ptr is not contained
			if (it != _slabs.end())
			{
				freelist_free(&(*it), (void*)ptr);
				// Deleting an element makes the allocator non full, so mark as non full
				const auto allocatorIndex{ std::distance(_slabs.begin(), it) };
				_fullBuckets[allocatorIndex] = false;
			}
		}
		else
		{
			freelist_free(&(*_slabs.begin()), (void*)ptr);
			_fullBuckets[0] = false;
		}
	}

	uint64_t GetFixedBlockSize()const override { return _elementSize; };

private:
	const uint64_t _maxNumElementsPerSlab;
	const uint64_t _elementSize;
	const uint64_t _slabBytes;
	std::vector<freelist> _slabs;
	std::vector<bool> _fullBuckets;
	IAlignedAllocator_T _alignedAllocator;

	friend class CPagedAllocatorFixture;

	uint64_t _getFreeAllocatorIndex()
	{
		// Find the first non full bucket
		for (uint64_t i{}; i < _fullBuckets.size(); i++)
		{
			if (_fullBuckets[i] == false)
			{
				return i;
			}
		}
		// If all buckets are full allocate a new bucket
		_fullBuckets.push_back(false);
		void* buffer{ _alignedAllocator.Allocate(_slabBytes, alignof(std::max_align_t)) };
		if (!buffer)
			throw std::bad_alloc{};

		freelist slab{};
		freelist_initialize(&slab, buffer, _slabBytes);
		_slabs.emplace_back(std::move(slab));
		return _slabs.size() - 1;
	}
};
