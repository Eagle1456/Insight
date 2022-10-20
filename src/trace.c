#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "trace.h"
#include "fs.h"
#include "timer.h"
#include "atomic.h"
#include "debug.h"

#include <stddef.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

//Each event that the caller calls
typedef struct trace_event_t {
	char* name;
	char type;
	unsigned int thread_id;
	uint64_t nanoseconds;
} trace_event_t;

//A structure for queues for each thread
typedef struct thread_queue_t {
	unsigned int thread_id;
	char** items;
	int tail_index;
	int capacity;
} thread_queue_t;

typedef struct trace_t {
	heap_t* heap;
	trace_event_t** events;
	thread_queue_t** current_threads;
	int max_capacity;
	int current_capacity;
	int thread_num;
	bool isCapturing;
	char* path;
} trace_t;

//Finds or creates a queue for each thread
thread_queue_t* get_thread_queue(trace_t* trace, unsigned int thread_id) {
	int current_thread_num = trace->thread_num;
	for (int i = 0; i < current_thread_num; i++) {
		if (*(trace->current_threads + i) &&( * (trace->current_threads + i))->thread_id == thread_id) {
			return *(trace->current_threads + i);
		}
	}

	int num = atomic_increment(&(trace->thread_num));
	thread_queue_t* new_queue = heap_alloc(trace->heap, sizeof(thread_queue_t), 8);
	new_queue->thread_id = GetCurrentThreadId();
	new_queue->items = heap_alloc(trace->heap, sizeof(char*) * trace->max_capacity, 8);
	new_queue->tail_index = 0;
	*(trace->current_threads + num) = new_queue;
	return new_queue;
}

// Frees all of the queues for threads
void free_threads(trace_t* trace) {
	for (int i = 0; i < trace->current_capacity; i++) {
		trace_event_t* event = *(trace->events + i);
		heap_free(trace->heap, event->name);
		heap_free(trace->heap, event);
	}
	heap_free(trace->heap, trace->events);

	for (int i = 0; i < trace->thread_num; i++) {
		thread_queue_t* thread_q = *(trace->current_threads + i);

		for (int j = 0; j < thread_q->tail_index; j++) {
			heap_free(trace->heap, *(thread_q->items + j));
		}
		heap_free(trace->heap, thread_q->items);
		heap_free(trace->heap, thread_q);
	}
	heap_free(trace->heap, trace->current_threads);
	heap_free(trace->heap, trace->path);
}

trace_t* trace_create(heap_t* heap, int event_capacity) {
	trace_t* trace = heap_alloc(heap, sizeof(trace_t), 8);
	trace->heap = heap;
	trace->max_capacity = event_capacity;
	trace->isCapturing = false;
	trace->path = NULL;
	return trace;
}

void trace_destroy(trace_t* trace) {
	if (trace->isCapturing) {
		free_threads(trace);
	}
	heap_free(trace->heap, trace);
}

void trace_duration_push(trace_t* trace, const char* name) {
	if (trace->isCapturing){
		int old_cap = atomic_increment(&(trace->current_capacity));
		if (old_cap < trace->max_capacity) {
			unsigned int thread_id = GetCurrentThreadId();
			uint64_t currentTime = timer_ticks_to_us(timer_get_ticks());

			thread_queue_t* thread_q = get_thread_queue(trace, thread_id);
			unsigned int namelen = (unsigned int) strlen(name) + 1;
			*(thread_q->items + thread_q->tail_index) = heap_alloc(trace->heap, (size_t)namelen, 8);
			strcpy_s(*(thread_q->items + thread_q->tail_index), namelen, name);
			thread_q->tail_index++;

			trace_event_t* event = heap_alloc(trace->heap, sizeof(trace_event_t), 8);
			event->thread_id = thread_id;
			event->nanoseconds = currentTime;
			event->type = 'B';
			event->name = heap_alloc(trace->heap, (size_t)namelen, 8);
			strcpy_s(event->name, namelen, name);
			*(trace->events + old_cap) = event;
		}
	}
}

void trace_duration_pop(trace_t* trace) {
	if (trace->isCapturing) {
		unsigned int thread_id = GetCurrentThreadId();
		thread_queue_t* thread_q = get_thread_queue(trace, thread_id);
		if (thread_q->tail_index > 0) {
			char* name = *(thread_q->items + (thread_q->tail_index - 1));
			int old_cap = atomic_increment(&(trace->current_capacity));

			if (old_cap < trace->max_capacity) {
				uint64_t currentTime = timer_ticks_to_us(timer_get_ticks());
				trace_event_t* event = heap_alloc(trace->heap, sizeof(trace_event_t), 8);
				unsigned int namelen = (unsigned int) strlen(name) + 1;
				event->thread_id = thread_id;
				event->nanoseconds = currentTime;
				event->type = 'E';
				event->name = heap_alloc(trace->heap, (size_t)namelen, 8);
				strcpy_s(event->name, namelen, name);
				*(trace->events + old_cap) = event;
			}

			heap_free(trace->heap, name);
			thread_q->tail_index--;
		}
		else {
			debug_print(k_print_error, "Error: The trace queue was empty on thread %u", GetCurrentThreadId());
		}
	}
}

void trace_capture_start(trace_t* trace, const char* path) {
	trace->isCapturing = true;
	trace->current_capacity = 0;
	trace->current_threads = heap_alloc(trace->heap, sizeof(thread_queue_t*) * trace->max_capacity, 8);
	trace->events = heap_alloc(trace->heap, sizeof(trace_event_t*) * trace->max_capacity, 8);
	size_t pathsize = strlen(path) + 1;
	char* new_path = heap_alloc(trace->heap, pathsize, 8);
	strcpy_s(new_path, pathsize, path);
	trace->path = new_path;
}

void trace_capture_stop(trace_t* trace) {
	trace->isCapturing = false;
	char* buffer = "";
	for (int i = 0; i < trace->current_capacity; i++) {
		trace_event_t* event = *(trace->events + i);
		char* old_buffer = buffer;
		size_t old_len = strlen(old_buffer);

		char* string = heap_alloc(trace->heap, 4096, 8);
		if (i < trace->current_capacity - 1) {
			sprintf_s(string, 4096, "{\"name\": \"%s\",\"ph\": \"%c\",\"pid\":0,\"tid\":\"%u\",\"ts\":%u},", 
				event->name, event->type, event->thread_id, (unsigned int)event->nanoseconds);
		}
		else {
			sprintf_s(string, 4096, "{\"name\": \"%s\",\"ph\": \"%c\",\"pid\":0,\"tid\":\"%u\",\"ts\":%u}", 
				event->name, event->type, event->thread_id, (unsigned int)event->nanoseconds);
		}
		size_t new_len = old_len + strlen(string) + 1;
		char* new_buffer = heap_alloc(trace->heap, new_len, 8);
		strcpy_s(new_buffer, new_len, old_buffer);
		strcat_s(new_buffer, new_len, string);
		heap_free(trace->heap, string);
		buffer = new_buffer;
		if (old_len > 0) {
			heap_free(trace->heap, old_buffer);
		}
	}
	size_t old_len = strlen(buffer);
	char* string = heap_alloc(trace->heap, 4096, 8);
	sprintf_s(string, 4096, "{\"displayTimeUnit\": \"ns\", \"traceEvents\" : [%s]}", buffer);
	size_t string_len = strlen(string);
	if (old_len > 0) {
		heap_free(trace->heap, buffer);
	}

	fs_t* fs = fs_create(trace->heap, trace->max_capacity);
	fs_work_t* work = fs_write(fs, trace->path, string, string_len, false);
	fs_work_destroy(work);
	fs_destroy(fs);
	heap_free(trace->heap, string);

	free_threads(trace);
}