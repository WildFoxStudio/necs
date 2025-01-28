// //////////////////////////////////////////////////////////////////////////////////////////
// FILE: IEntityFactory.h
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

#include <string>

#include "necs/IWorldObjectCDO.h"

class CWorldObject;
class CWorld;

/**
 * /brief Handles entity allocations by class name.
 */
class IEntityFactory {
public:
	virtual ~IEntityFactory() = default;

	// Create an instance based on the type name
	virtual CWorldObject* PlacementNewFromTypename(void* memory,
		IWorldObjectPendingDestroyNotifier* const pendingDestroyNotifier, const std::string& typeName) = 0;

	virtual const IWorldObjectCDO& GetCDOFromTypename(const std::string& typeName) const = 0;
};

/**
 * /brief Handles allocation and instantiation and handling of entities.
 */
struct IWorldObjectManager
{
	virtual ~IWorldObjectManager() = default;

	template<typename T> T* SpawnWorldObject(const std::string& typeName) {
		static_assert(std::is_base_of<CWorldObject, T>::value, "Must derived from CWorldObject");
		return reinterpret_cast<T*>(SpawnWorldObject(typeName));
	}

	virtual CWorldObject* SpawnWorldObject(const std::string& typeName) = 0;
};
