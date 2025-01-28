// //////////////////////////////////////////////////////////////////////////////////////////
// FILE: IIDGenerator.h
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

#include <set>
#include <queue>
#include <stdexcept>

template<typename ID_T>
class IIdGenerator
{
public:
	virtual ID_T Generate() = 0;
	virtual void Release(ID_T id) = 0;
	virtual bool IsUsed(ID_T id)const = 0;
	virtual ID_T GetMaxId()const = 0;
};

template<typename ID_T>
class IDGenerator final : public IIdGenerator<ID_T> {
private:
	std::queue<ID_T> released_ids; // Stores IDs that can be reused
	std::set<ID_T> in_use; // Keeps track of IDs currently in use
	ID_T next_id; // Counter to generate new IDs
	ID_T max_id; // Maximum possible ID to avoid overflow

public:
	// Constructor: Initializes the generator with a maximum possible ID
	IDGenerator(ID_T max_id_limit = std::numeric_limits<ID_T>::max())
		: next_id(0), max_id(max_id_limit) {}

	// Function to generate a new ID (or reuse an old one)
	ID_T Generate() override {
		ID_T id;

		if (!released_ids.empty()) {
			// Reuse an ID if available
			id = released_ids.front();
			released_ids.pop();
		}
		else {
			// Generate a new ID
			if (next_id > max_id) {
				throw std::runtime_error("ID limit exceeded!");
			}
			id = next_id++;
		}

		in_use.insert(id); // Mark ID as in use
		return id;
	}

	// Function to release an ID, allowing it to be reused
	void Release(ID_T id) override {
		if (in_use.find(id) == in_use.end()) {
			throw std::invalid_argument("ID not currently in use");
		}
		in_use.erase(id);
		released_ids.push(id);
	}

	// Function to check if an ID is currently in use
	bool IsUsed(ID_T id) const override {
		return in_use.find(id) != in_use.end();
	}

	ID_T GetMaxId()const override {
		return next_id;
	}
};