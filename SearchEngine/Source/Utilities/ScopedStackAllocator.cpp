#include "PrecompiledHeader.h"
#include "ScopedStackAllocator.h"

ScopedStackAllocator::BlockHeader ScopedStackAllocator::s_EmptyHeader(nullptr, nullptr);