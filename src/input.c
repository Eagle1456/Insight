#include "input.h"

#include "debug.h"
#include "heap.h"
#include "timer.h"


#define INPUT_NUM 4
#define TIMER_CHECK 2000

typedef struct input_t {
	heap_t* heap;
	wm_window_t* window;
	control_t* controller;
	uint32_t key_mask;
	control_type_t type_mask;
	control_type_t last_control_type;
	map_t* controller_map;
	map_t* keyboard_map;
	bool fallthrough;
	int x_axis;
	int y_axis;
	uint32_t last_check;
}	input_t;

// Data structure for mappings
static input_button_t inputs[INPUT_NUM] = { k_button_up, k_button_down, k_button_left, k_button_right };

// Structure for mapping arrow keys to input keys
const map_t standard_keyboard_map = { {k_key_up, k_key_down, k_key_left, k_key_right} };

// Structure for mapping controller keys to input keys
const map_t standard_controller_map = { {k_dpad_up, k_dpad_down, k_dpad_left, k_dpad_right} };

control_type_t control_type_array[] = { k_keyboard_type, k_controller_type};

input_t* input_create(heap_t* heap, wm_window_t* window, control_type_t controller_type, map_t* controller_map, map_t* key_map, bool fallthrough) {
	input_t* input = heap_alloc(heap, sizeof(input_t), 8);
	input->heap = heap;
	input->key_mask = 0;
	input->type_mask = controller_type;
	input->x_axis = 0;
	input->y_axis = 0;
	input->window = window;
	input->fallthrough = fallthrough;
	input->keyboard_map = key_map;
	input->controller_map = controller_map;
	input->last_control_type = controller_type;
	
	if (controller_type == k_controller_type) {
		input->controller = controller_create(input->heap, 1);
		input->last_check = timer_ticks_to_ms(timer_get_ticks());
	}
	return input;
}

void input_destroy(input_t* input) {
	if (input->type_mask == k_controller_type) {
		controller_destroy(input->controller);
	}
	heap_free(input->heap, input);
}

uint32_t bind_map(uint32_t key_bind, map_t map) {
	uint32_t button_map = 0;

	for (int i = 0; i < INPUT_NUM; i++) {
		if (key_bind & map.input_mappings[i]) {
			button_map |= inputs[i];
		}
	}
	return button_map;
}

void controller_check_query(input_t* input) {
	uint32_t currentTime = timer_ticks_to_ms(timer_get_ticks());

	if (currentTime - input->last_check >= TIMER_CHECK) {
		input->last_check = currentTime;
		controller_query(input->controller);
	}
}

void input_pump(input_t* input) {
	uint32_t key_mask = 0;
	uint32_t current_controllers;
	input->key_mask = 0;
	if (input->type_mask == k_controller_type) {
		controller_check_query(input);
		controller_pump(input->controller);
	}

	switch (input->type_mask) {	
		case (k_controller_type):
			current_controllers = get_current_players(input->controller);
			if (current_controllers) {
				key_mask = controller_get_button_mask(input->controller, k_first_player);
				if (input->controller_map) {
					input->key_mask = bind_map(key_mask, *input->controller_map);
				}
				else {
					input->key_mask = bind_map(key_mask, standard_controller_map);
				}
				short rx;
				short ry;
				controller_get_axes(input->controller, k_first_player, (short*)&input->x_axis, (short*)&input->y_axis, &rx, &ry);
				input->last_control_type = k_controller_type;
				break;
			}
			if (!input->fallthrough) {
				break;
			}
		case (k_keyboard_type):
			key_mask = wm_get_key_mask(input->window);
			if (input->keyboard_map) {
				input->key_mask = bind_map(key_mask, *input->keyboard_map);
			}
			else {
				input->key_mask = bind_map(key_mask, standard_keyboard_map);
			}
			wm_get_mouse_move(input->window, &input->x_axis, &input->y_axis);
			input->last_control_type = k_keyboard_type;
			break;
		default:
			break;
	}
	
}

uint32_t input_get_key_mask(input_t* input) {
	return input->key_mask;
}

void input_get_axes(input_t* input, int* x, int* y) {
	*x = input->x_axis;
	*y = input->y_axis;
}

uint32_t get_input_type(input_t* input) {
	return input->last_control_type;
}