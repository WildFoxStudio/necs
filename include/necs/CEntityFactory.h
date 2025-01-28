// //////////////////////////////////////////////////////////////////////////////////////////
// FILE: CEntityFactory.h
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

#include <iostream>
#include <string>
#include <unordered_map>
#include <functional>
#include <memory>
#include <stdexcept>

#include "necs/CWorldObject.h"
#include "necs/IEntityFactory.h"

class CWorld;

class CEntityFactory final : public  IEntityFactory {
	friend class CEntityFactoryFixture;
public:
	CEntityFactory() = default;
	virtual ~CEntityFactory() = default;

	// new(void*) T(DWorldObjectInitielizer*const)
	using CreateFunc = std::function<CWorldObject* (void*, const DWorldObjectInitializer&)>;

	// Register a type with its associated creation function
	template <typename T>
	void RegisterEntityClass(const std::string& typeName) {
		static_assert(std::is_base_of<CWorldObject, T>::value, "Must derived from IWorldObject");

		DWorldObjectInitializer cdoInitializer{};
		cdoInitializer.ClassSize = sizeof(T);
		cdoInitializer.ClassAlignment = alignof(T);

		assert(_classNameToCreateFnAndCdo.find(typeName) == _classNameToCreateFnAndCdo.end() && "Type must not be registered");

		_classNameToCreateFnAndCdo[typeName] = std::make_pair([](void* memory, const DWorldObjectInitializer& initializer) -> T* {
			new(memory) T(initializer);
			return reinterpret_cast<T*>(memory);
			}, std::make_unique<T>(cdoInitializer));
	}

	// Create an instance based on the type name
	CWorldObject* PlacementNewFromTypename(void* memory,
		IWorldObjectPendingDestroyNotifier* const pendingDestroyNotifier, const std::string& typeName) override {
		assert(memory);
		assert(pendingDestroyNotifier);

		const auto it{ _classNameToCreateFnAndCdo.find(typeName) };
		assert(it != _classNameToCreateFnAndCdo.end() && "Type not registered");

		DWorldObjectInitializer initializer{};
		initializer.StaticClassCDO = static_cast<IWorldObjectCDO*>(it->second.second.get());
		initializer.ClassSize = initializer.StaticClassCDO->GetClassSize();
		initializer.ClassAlignment = initializer.StaticClassCDO->GetClassAlignment();
		initializer.PendingDestroyNotifier = pendingDestroyNotifier;

		assert(reinterpret_cast<uintptr_t>(memory) % initializer.StaticClassCDO->GetClassAlignment() == 0 && "Memory must be correctly aligned!");

		return it->second.first(memory, initializer); // Call the placement new function

	}

	const IWorldObjectCDO& GetCDOFromTypename(const std::string& typeName) const override
	{
		const auto it{ _classNameToCreateFnAndCdo.find(typeName) };
		assert(it != _classNameToCreateFnAndCdo.end() && "Type not registered");
		return *static_cast<IWorldObjectCDO*>(it->second.second.get());
	}

private:
	std::unordered_map<std::string, std::pair<CreateFunc, std::unique_ptr<CWorldObject>>> _classNameToCreateFnAndCdo;
};