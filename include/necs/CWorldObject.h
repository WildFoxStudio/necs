// //////////////////////////////////////////////////////////////////////////////////////////
// FILE: CWorldObject.h
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

#include <cassert>
#include <stdint.h>
#include <set>
#include <cstddef>
#include <limits>
#include <memory>
#include <functional>
#include <string>
#include <cmath>

#include "clow/gpalloc.h"

#include "necs/IWorldObjectCDO.h"

struct IWorldObjectPendingDestroyNotifier
{
	virtual ~IWorldObjectPendingDestroyNotifier() = default;

	virtual void MarkPendingDestroy(class CWorldObject* ptr) = 0;
};

/**
 * /brief Base struct for the WorldObject initialization.
 */
struct DWorldObjectInitializer
{
	const IWorldObjectCDO* StaticClassCDO{};
	uint64_t ClassSize{};
	uint64_t ClassAlignment{};
	IWorldObjectPendingDestroyNotifier* PendingDestroyNotifier{};
};

inline bool IsPowerOfTwo(const uint64_t value)
{
	if (value >= 2)
	{
		const auto r{ log2(value) };
		return std::floor(r) == static_cast<double>(r);
	}

	return false;
}

class CWorldObjectCDO : public IWorldObjectCDO
{
public:
	CWorldObjectCDO(const bool isConstructingCDO, const uint64_t classSize, const uint64_t classAlignment) :_isConstructingCdo(isConstructingCDO), _classSize(classSize), _classAlignmemt(classAlignment) {
		assert(_classSize > 0);
	};

	template<typename T>
	void StaticRegisterNewComponent() {
		StaticRegisterNewComponentUnknown(sizeof(T), alignof(T));
	};

	void StaticRegisterNewComponentUnknown(const uint64_t sizeOfComponent, const uint64_t alignment)
	{
#if _DEBUG
		{
			assert(_isConstructingCdo);
			assert(sizeOfComponent > 0);
			assert(alignment == 1 || IsPowerOfTwo(alignment) && "Alignment must be a power of two!");
			assert(sizeOfComponent >= alignment && "Alignment can't be bigger than the type!");
		}
#endif
		CEntityComponentMetadata meta{ sizeOfComponent, alignment };
		_components.emplace_back(std::move(meta));
	}

	const std::vector<CEntityComponentMetadata>& GetCDOComponentsInfo()const override
	{
		return _components;
	}

	uint64_t GetClassSize() const override { return _classSize; };
	uint64_t GetClassAlignment() const override { return _classAlignmemt; };

	bool IsCDO()const { return _isConstructingCdo; }


	uint64_t ComputeComponentsMaxSizeForAllocation()const override
	{
		if (_components.size() == 0)
			return 0;

		// Consider worst case alignment
		uint64_t bytes{ alignof(std::max_align_t) };

		for (const auto& component : _components)
		{
			bytes += component.Size;
		}

		return bytes;
	}
private:
	std::vector<CEntityComponentMetadata> _components;
	bool _isConstructingCdo{};
	const uint64_t _classSize;
	const uint64_t _classAlignmemt;
};

class CWorldObjectArchetypesComponentsContainer
{
	friend class CWorldObjectArchetypesComponentsContainerFixture;
public:
	/**
	 * /brief Constructor.
	 * /pararm worldObject the World Object parent.
	 * /param staticClassCdo the class cdo if has any or null ptr if constructing a CDO.
	 */
	CWorldObjectArchetypesComponentsContainer(void* worldObject, const IWorldObjectCDO* const staticClassCdo) {

		if (staticClassCdo && staticClassCdo->GetCDOComponentsInfo().size() > 0)
		{
#if _DEBUG
			assert(worldObject);
			assert(staticClassCdo->GetClassSize() != 0);
			_worldObjectComponentsPtrBegin = toUintptr(worldObject) + staticClassCdo->GetClassSize();
			_worldObjectComponentsPtrEnd = toUintptr(worldObject) + staticClassCdo->GetClassSize() + staticClassCdo->ComputeComponentsMaxSizeForAllocation();
			assert(_worldObjectComponentsPtrBegin % alignof(std::max_align_t) == 0
				&& "Buffer must be properly aligned!");
#endif

			const uint64_t componentsBytes{ staticClassCdo->ComputeComponentsMaxSizeForAllocation() };
			gpalloc_initialize(&_allocator, reinterpret_cast<void*>(toUintptr(worldObject) + staticClassCdo->GetClassSize()), componentsBytes);
		}
	}


protected:
	void* MallocComponent(const uint64_t size, const uint64_t alignment) {
		assert(alignment == 1 || IsPowerOfTwo(alignment) && "Alignment must be a power of two!");
		assert(size >= alignment);

		if (_allocator.buffer_size == 0)
			return nullptr;

		void* newPtr{ gpalloc_malloc(&_allocator, size, alignment) };
		if (!newPtr)
			return nullptr;
		//sanity check, we shouldn't allocate outside the archetype's memory bounds, because we would corrupt other archetypes or worse UB.
		{
			assert(toUintptr(newPtr) >= _worldObjectComponentsPtrBegin && "Pointer must not go outside the archetype's memory bound!");
			assert(toUintptr(newPtr) < _worldObjectComponentsPtrEnd && "Pointer must not go outside the archetype's memory bound!");
			assert(toUintptr(newPtr) + size <= _worldObjectComponentsPtrEnd && "Pointer must not write outside the archetype's memory bound!");
		}
		return newPtr;
	};

	void FreeComponent(void* ptr)
	{
		if (!ptr || _allocator.buffer_size == 0)
			return;

		{
			//const auto allocationSize{ freelist_get_allocation_size(&_allocator, ptr) };
			////sanity check, we shouldn't allocate outside the archetype's memory bounds, because we would corrupt other archetypes or worse UB.
			//assert(toUintptr(ptr) > _worldObjectComponentsPtrBegin && "Pointer must not go outside the archetype's memory bound!");
			//assert(toUintptr(ptr) < _worldObjectComponentsPtrEnd && "Pointer must not go outside the archetype's memory bound!");
			//assert(toUintptr(ptr) + allocationSize <= _worldObjectComponentsPtrEnd && "Pointer must not write outside the archetype's memory bound!");
		}

		gpalloc_free(&_allocator, ptr);
	}

private:
	inline std::uintptr_t toUintptr(const CWorldObject* const worldObjectPtr)const {
		return reinterpret_cast<std::uintptr_t>(worldObjectPtr);
	}
	inline std::uintptr_t toUintptr(void* ptr)const {
		return reinterpret_cast<std::uintptr_t>(ptr);
	}

#if _DEBUG
	uintptr_t _worldObjectComponentsPtrBegin{};
	uintptr_t _worldObjectComponentsPtrEnd{};
#endif

	gpalloc_t _allocator{};
};


class CTickable
{
public:
	CTickable(const bool canEverTick) : _canEverTick(canEverTick) {}

	inline bool CanEverTick() const { return _canEverTick; };

	virtual void Tick() {};

private:
	/**
	 * /brief True if can ever tick. By default will not tick.
	 */
	const bool _canEverTick;
};





class CDestroyable
{
public:
	CDestroyable(IWorldObjectPendingDestroyNotifier* const pendingDestroyNotifier) :_pendingDestroyNotifier(pendingDestroyNotifier) {}
	bool IsPendingDestroy()const;

	virtual void SetPendingDestroy();

	inline void OnSetPendingDestroyCallback(std::function<void(void)> fn) { _onPendingDestroySetCallback = fn; };
private:
	IWorldObjectPendingDestroyNotifier* const _pendingDestroyNotifier;

	/**
	* /brief True if the entity is asking to be destroyed.
	*/
	bool PendingDestroy{};

	std::function<void(void)> _onPendingDestroySetCallback{};
};


/**
 * /brief Base world object class.
 * With archetype it means that the class is constructed with the slack, with the least enough memory, gathered from the CDO to hold the archetype's components in contiguos memory right after the world object.
 * Components created out of the constructor are allocated in a separate memory pool leading to cache miss, I encourage you to always create all the components upfront in the constructor and removing them in begin play function,
 * components defined in the CDO will always be in contiguos memory right after the archetype.
 */
class CWorldObject : public CWorldObjectCDO, public CTickable, public CDestroyable, private CWorldObjectArchetypesComponentsContainer
{
public:
	std::set<std::string> Tags;

	CWorldObject(const DWorldObjectInitializer& initializer, const bool canEverTick) : CWorldObjectCDO(initializer.StaticClassCDO == nullptr, initializer.ClassSize, initializer.ClassAlignment), CTickable(canEverTick), CDestroyable(initializer.PendingDestroyNotifier), CWorldObjectArchetypesComponentsContainer(this, initializer.StaticClassCDO) {
		// TODO remove this check
		if (initializer.StaticClassCDO)
		{
			//assert(initializer.StaticClassCDO->GetComponentsReservedSize() > 0);
		}
	};

	virtual ~CWorldObject() {
	};

	template<typename T, typename... Args>
	std::shared_ptr<std::decay_t<T>> NewComponent(Args&&... args) {

		// CDO constructor
		if (IsCDO())
		{
			StaticRegisterNewComponentUnknown(sizeof(T), alignof(T));
			// Allocate object with new
			return std::make_shared<T>(std::forward<Args>(args)...);
		}

		// Try to allocate in the archetype's reserved memory
		T* componentPtr{ reinterpret_cast<T*>(MallocComponent(sizeof(std::decay_t<T>), alignof(std::decay_t<T>))) };
		if (componentPtr)
		{
			// Always alias using placement new, otherwise dynamic type is undefined leading to UB.
			new(componentPtr) T(std::forward<Args>(args)...);
			componentPtr = std::launder(componentPtr);

			return std::shared_ptr<std::decay_t<T>>(componentPtr, [this](T* ptr) {
				/*Free from reserved pool*/
				FreeComponent(reinterpret_cast<void*>(ptr));
				});

		}

		// Allocate object with new. TODO use pool allocator memory
		return std::make_shared<T>(std::forward<Args>(args)...);
	};
};