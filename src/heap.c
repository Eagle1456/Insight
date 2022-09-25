#include "heap.h"

#include "debug.h"
#include "mutex.h"
#include "tlsf/tlsf.h"

#include <stddef.h>
#include <stdio.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dbghelp.h>

#define STACK_COUNT 50

typedef struct arena_t
{
	pool_t pool;
	struct arena_t *next;
} arena_t;

typedef struct memblock_t
{
	void *address;
	PVOID *backtrace;
	size_t block_size;
	USHORT num_frames;
	struct memblock_t *next;
} memblock_t;

typedef struct heap_t
{
	tlsf_t tlsf;
	size_t grow_increment;
	arena_t *arena;
	memblock_t *last_block;
	mutex_t *mutex;
} heap_t;

heap_t *heap_create(size_t grow_increment)
{
	heap_t *heap = VirtualAlloc(NULL, sizeof(heap_t) + tlsf_size(),
								MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (!heap)
	{
		debug_print(
			k_print_error,
			"OUT OF MEMORY!\n");
		return NULL;
	}

	heap->mutex = mutex_create();
	heap->grow_increment = grow_increment;
	heap->tlsf = tlsf_create(heap + 1);
	heap->arena = NULL;
	heap->last_block = NULL;
	return heap;
}

void *heap_alloc(heap_t *heap, size_t size, size_t alignment)
{
	mutex_lock(heap->mutex);

	void *address = tlsf_memalign(heap->tlsf, alignment, size);
	if (!address)
	{
		size_t arena_size =
			__max(heap->grow_increment, size * 2) +
			sizeof(arena_t);
		arena_t *arena = VirtualAlloc(NULL,
									  arena_size + tlsf_pool_overhead(),
									  MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
		if (!arena)
		{
			debug_print(
				k_print_error,
				"OUT OF MEMORY!\n");
			return NULL;
		}

		arena->pool = tlsf_add_pool(heap->tlsf, arena + 1, arena_size);

		arena->next = heap->arena;
		heap->arena = arena;

		address = tlsf_memalign(heap->tlsf, alignment, size);
	}
	memblock_t *memblock = VirtualAlloc(NULL,
										sizeof(memblock_t), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (!memblock)
	{
		debug_print(k_print_error, "Tried to capture stack trace but ran out of memory...\n");
		return address;
	}

	memblock->address = address;
	memblock->block_size = tlsf_block_size(address);
	memblock->backtrace = VirtualAlloc(NULL, STACK_COUNT * sizeof(PVOID), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

	// Start capturing the stack backtrace to the frame before this one
	memblock->num_frames = CaptureStackBackTrace(1, STACK_COUNT, memblock->backtrace, NULL);

	memblock->next = heap->last_block;
	heap->last_block = memblock;

	mutex_unlock(heap->mutex);

	return address;
}

void heap_free(heap_t *heap, void *address)
{
	mutex_lock(heap->mutex);
	tlsf_free(heap->tlsf, address);
	memblock_t *current_block = heap->last_block;
	memblock_t *last_block = NULL;
	while (current_block)
	{
		if (address == current_block->address)
		{
			VirtualFree(current_block->backtrace, 0, MEM_RELEASE);
			if (!last_block)
			{
				heap->last_block = current_block->next;
			}
			else
			{
				last_block->next = current_block->next;
			}
			VirtualFree(current_block, 0, MEM_RELEASE);
			return;
		}
		else
		{
			last_block = current_block;
			current_block = current_block->next;
		}
	}
	debug_print(k_print_warning, "Address to free not found!\n");
	mutex_unlock(heap->mutex);
}

void heap_destroy(heap_t *heap)
{
	tlsf_destroy(heap->tlsf);

	arena_t *arena = heap->arena;
	while (arena)
	{
		arena_t *next = arena->next;
		VirtualFree(arena, 0, MEM_RELEASE);
		arena = next;
	}

	if (heap->last_block)
	{
		SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
		HANDLE handle = GetCurrentProcess();
		SymInitialize(handle, NULL, TRUE);
		PIMAGEHLP_SYMBOL symbol = VirtualAlloc(NULL, sizeof(IMAGEHLP_SYMBOL64) + 255 * sizeof(TCHAR), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
		if (symbol)
		{
			symbol->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL64);
			symbol->MaxNameLength = 256;
		}
		memblock_t *block = heap->last_block;

		while (block)
		{
			debug_print(k_print_warning, "Memory leak of size %u bytes\n", (unsigned int)block->block_size);
			if (symbol)
			{
				size_t frames = block->num_frames;
				for (size_t i = 0; i < frames; i++)
				{
					DWORD64 addr = (DWORD64) * (block->backtrace + i);
					if (SymGetSymFromAddr64(handle, addr, 0, symbol))
					{
						debug_print(k_print_warning, "[%u] %s\n", (unsigned)frames - i - 1, symbol->Name);
					}
				}
			}

			memblock_t *next = block->next;
			VirtualFree(block, 0, MEM_RELEASE);
			block = next;
		}
		if (symbol)
			VirtualFree(symbol, 0, MEM_RELEASE);
		SymCleanup(handle);
	}

	mutex_destroy(heap->mutex);

	VirtualFree(heap, 0, MEM_RELEASE);
}
