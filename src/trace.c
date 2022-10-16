#include <stdbool.h>
#include <string.h>

#include "trace.h"
#include "queue.h"
#include "fs.h"
#include "timer.h"

#include <stddef.h>

typedef struct trace_t {
	heap_t* heap;
	queue_t* duration_queue;
	fs_t* fs;
	int capacity;
	bool isCapturing;
	char* path;
};

trace_t* trace_create(heap_t* heap, int event_capacity) {
	trace_t* trace = heap_alloc(heap, sizeof(trace_t), 8);
	trace->heap = heap;
	trace->capacity = event_capacity;
	trace->duration_queue = queue_create(heap, event_capacity);
	//trace->fs = fs_create(heap, event_capacity);
	trace->isCapturing = false;
	trace->path = NULL;
	return trace;
}

void trace_destroy(trace_t* trace) {
	fs_destroy(trace->fs);
	queue_destroy(trace->duration_queue);
	if (trace->path != NULL) {
		heap_free(trace->heap, trace->path);
	}
	heap_free(trace->heap, trace);
}

void trace_duration_push(trace_t* trace, const char* name) {
	unsigned int namelen = strnlen(name, 256);
	char* name = heap_alloc(trace->heap, (size_t)namelen, 8);
	queue_push(trace->duration_queue, name);

	if (trace->isCapturing) {
		// Write to JSON
}
}

void trace_duration_pop(trace_t* trace) {
	char* name = (char*) queue_pop(trace->duration_queue);

	if (trace->isCapturing) {
		// Write to JSON
}
	heap_free(trace->heap, name);
}
