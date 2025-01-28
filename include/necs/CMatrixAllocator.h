// //////////////////////////////////////////////////////////////////////////////////////////
// FILE: CMatrixAllocator.h
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

#include <list>

#include "IAllocator.h"
#include "CPagedAllocator.h"

/**
 * /brief A matrix allocator can allocate any size object.
 * Columns are allocation size.
 * Rows are allocator pages.
 */
template<class PagedAllocator_T>
class CMatrixAllocator final : public IAllocator
{
	friend class CMatrixAllocatorTest_ShouldAllocateCorrectPageForDifferentSizeTypes_Test;
	static_assert(std::is_base_of<IPagedAllocator, PagedAllocator_T>::value);

public:
	explicit CMatrixAllocator(const uint64_t maxElementsPerPage) :_maxElementsPerPage(maxElementsPerPage) {
		assert(_maxElementsPerPage > 0);
	}

	void* Allocate(const uint64_t bytes) override {
		return _getAllocatorBySize(bytes).Allocate();
	};

	void Free(void* ptr) override {
		// Brute force deallocation by searching all allocators
		for (auto& allocator : _perSizeAllocator)
		{
			allocator.Free(ptr);
		}
	};

private:
	const uint64_t _maxElementsPerPage;
	/**
	 * /brief Columns ordered per ascending size. TODO turn into a vector with move only if faster.
	 */
	std::list<PagedAllocator_T> _perSizeAllocator;

	/**
	 * /brief Returns the corresponding paged allocator mapped to that particular size, allocates a new if doesn't exists.
	 * /param bytes The class size = block size of the bucket.
	 * /return The bucker reference.
	 */
	PagedAllocator_T& _getAllocatorBySize(const uint64_t bytes)
	{
		assert(bytes > 0);

		// Binary search using lower_bound with O(log N) complexity.
		const auto it{ std::lower_bound(_perSizeAllocator.begin(), _perSizeAllocator.end(), bytes, [](const PagedAllocator_T& allocator, const uint64_t value) {return allocator.GetFixedBlockSize() < value; }) };
		if (it != _perSizeAllocator.end() && it->GetFixedBlockSize() == bytes)
		{
			return *it;
		}
		else
			if (it == _perSizeAllocator.end())
			{
				// Allocate bigger size pageAllocator
				PagedAllocator_T newAllocator(_maxElementsPerPage, bytes);
				return _perSizeAllocator.emplace_back(std::move(newAllocator));
			}

		// If above has failed it means that an allocator in missing in between already existing allocator so insert it gently.
		// Adding elements should be ordered with O(M � N) complexity. Avoid sorting the whole vector because it's O(M � N log N) complexity.
		PagedAllocator_T newAllocator(_maxElementsPerPage, bytes);
		return *_perSizeAllocator.emplace(it, std::move(newAllocator));
	}
};
