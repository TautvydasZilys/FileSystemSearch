#pragma once

class ScopedStackAllocator
{
private:
	static const size_t kFirstBlockSize = 4096;
	static const size_t kAlignment = 4096;

	struct BlockHeader
	{
	private:
		uint8_t* m_PtrBegin;
		uint8_t* m_PtrEnd;
		uint8_t* m_CurrentPtr;

	public:
		inline BlockHeader(uint8_t* ptrBegin, uint8_t* ptrEnd) :
			m_PtrBegin(ptrBegin),
			m_PtrEnd(ptrEnd),
			m_CurrentPtr(ptrBegin)
		{
		}

		inline bool HasFreeSpace(size_t bytes) const
		{
			return static_cast<size_t>(m_PtrEnd - m_CurrentPtr) >= bytes;
		}

		inline bool IsMyPtr(uint8_t* ptr) const
		{
			return ptr >= m_PtrBegin && ptr <= m_PtrEnd;
		}

		inline uint8_t* AllocatePtr(size_t bytes)
		{
			Assert(HasFreeSpace(bytes));
			auto ptr = m_CurrentPtr;
			m_CurrentPtr += bytes;
			return ptr;
		}

		inline void FreePtr(uint8_t* ptr)
		{
			Assert(IsMyPtr(ptr));
			m_CurrentPtr = ptr;
		}

		inline bool IsEmpty()
		{
			return m_PtrBegin == m_CurrentPtr;
		}
	};

	std::vector<BlockHeader*> m_Blocks;
	size_t m_NextBlockSize;
	size_t m_CurrentBlock;
	BlockHeader* m_CurrentBlockPtr;

	static inline size_t AlignSize(size_t bytes)
	{
		return (bytes + (kAlignment - 1)) & ~(kAlignment - 1);
	}

	inline void AllocateBlock(size_t bytes)
	{
		auto allocationSize = std::max(m_NextBlockSize, bytes + sizeof(BlockHeader));
		allocationSize = AlignSize(allocationSize);

		auto memory = VirtualAlloc(nullptr, allocationSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
		auto blockHeader = new (memory)BlockHeader(static_cast<uint8_t*>(memory) + sizeof(BlockHeader), static_cast<uint8_t*>(memory) + allocationSize);
		m_Blocks.push_back(blockHeader);
		m_CurrentBlockPtr = m_Blocks[m_CurrentBlock];

		m_NextBlockSize = allocationSize * 2;
	}

	inline void EnsureMemoryIsAvailable(size_t bytes)
	{
		for (;;)
		{
			if (m_CurrentBlockPtr->HasFreeSpace(bytes))
				return;

			m_CurrentBlock++;

			if (m_CurrentBlock < m_Blocks.size())
			{
				m_CurrentBlockPtr = m_Blocks[m_CurrentBlock];
			}
			else
			{
				break;
			}
		}

		AllocateBlock(bytes);
	}

	inline void Free(uint8_t* ptr)
	{
		m_CurrentBlockPtr->FreePtr(ptr);

		if (m_CurrentBlockPtr->IsEmpty() && m_CurrentBlock > 0)
		{
			m_CurrentBlock--;
			m_CurrentBlockPtr = m_Blocks[m_CurrentBlock];
		}
	}

public:
	struct ScopedMemory
	{
	private:
		ScopedStackAllocator& m_Allocator;
		void* m_Ptr;

	public:
		inline ScopedMemory(ScopedStackAllocator& allocator, void* ptr) :
			m_Allocator(allocator),
			m_Ptr(ptr)
		{

		}

		ScopedMemory(const ScopedMemory&) = delete;
		ScopedMemory& operator=(const ScopedMemory&) = delete;

		inline ScopedMemory(ScopedMemory&& other) :
			m_Allocator(other.m_Allocator),
			m_Ptr(other.m_Ptr)
		{
			other.m_Ptr = nullptr;
		}

		inline ~ScopedMemory()
		{
			if (m_Ptr != nullptr)
				m_Allocator.Free(static_cast<uint8_t*>(m_Ptr));
		}

		template <typename T>
		inline operator T*() const
		{
			return static_cast<T*>(m_Ptr);
		}
	};

	inline ScopedStackAllocator() :
		m_NextBlockSize(kFirstBlockSize),
		m_CurrentBlock(0)
	{
		AllocateBlock(kFirstBlockSize);
	}

	ScopedStackAllocator(const ScopedStackAllocator&) = delete;
	ScopedStackAllocator& operator=(const ScopedStackAllocator&) = delete;

	inline ~ScopedStackAllocator()
	{
		for (auto block : m_Blocks)
			VirtualFree(block, 0, MEM_RELEASE);
	}

	inline ScopedMemory Allocate(size_t bytes)
	{
		EnsureMemoryIsAvailable(bytes);
		return ScopedMemory(*this, m_CurrentBlockPtr->AllocatePtr(bytes));
	}
};
