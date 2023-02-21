#pragma once

#include <stdint.h>

// Basic, intro-level linear allocator
// Never needed anything fancier for private projects ^_^'

class Memory
{
	static char* block;
	static char* blockStart;
	static constexpr uint64_t initial_alloc = 100000000; // About 100MB

	template<typename TypeAllocating>
	static TypeAllocating* AllocateRange(uint32_t alignment = 4, uint32_t elementsInRange = 1)
	{
		// Alignment
		////////////

		uint64_t offs = reinterpret_cast<uint64_t>(block);
		uint64_t toAlign = alignment - (offs % alignment);
		toAlign = (toAlign != alignment) ? toAlign : 0; // Align starting address to size
		// If an address is perfectly aligned already the logic in toAlign will offset it by [alignment] unnecessarily,
		// possibly creating fragmentation -

		uint64_t footprint = sizeof(TypeAllocating) * elementsInRange;
		uint64_t toAlignFootprint = alignment - (footprint % alignment);
		footprint += (toAlignFootprint != alignment) ? toAlignFootprint : 0;

		// Allocation offset
		block += toAlign;

		// Allocation
		TypeAllocating* addr = reinterpret_cast<TypeAllocating*>(block);
		block += footprint;
		return addr;
	}

	public:
		static void Init();
		static void DeInit();

		template<typename TypeAllocating>
		static TypeAllocating* AllocateSingle(int32_t alignment = 4)
		{
			return AllocateRange<TypeAllocating>(alignment, 1);
		}

		template<typename TypeAllocating>
		static TypeAllocating* AllocateArray(uint32_t arrayLen, uint32_t alignment = 4)
		{
			return AllocateRange<TypeAllocating>(alignment, arrayLen);
		}

		// Linear allocator - we can free all bytes back to some pointer, but not arbitrary data
		// This means you can't release arbitrarily! Basically only short-term loans that sit on top of the allocator can be freed outside of shutdown
		// (and on shutdown the whole block is permanently freed anyway, so the order of any pointer shuffles before that is irrelevant)
		static void FreeToAddress(void* destAddr);
};

