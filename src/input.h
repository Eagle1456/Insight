#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "wm.h"
#include "controller.h"

// Input system
//
// Object is input_t which represents the current input configuration
// Input should be pumped every frame

// Input handler
typedef struct input_t input_t;

// Array for mapping of four values, [up, down, left, right]
typedef struct map_t {
	int input_mappings[4];
}map_t;

typedef struct heap_t heap_t;


// Enums for each button pressed
// Obtained using input_get_key_mask
typedef enum input_button_t {
	k_button_up = 1 << 0,
	k_button_down = 1 << 1,
	k_button_left = 1 << 2,
	k_button_right = 1 << 3,
} input_button_t;

// Enums for the controller currently connected
// Obtained using get_input_type
typedef enum control_type_t{
	k_keyboard_type = 1 << 0,
	k_controller_type = 1 << 1,
	k_wheel_type = 1 << 2,
} control_type_t;

// Structure for mapping arrow keys to input keys
const map_t standard_keyboard_map;

// Structure for mapping controller keys to input keys
const map_t standard_controller_map;

// Creates an input_t object
// Setting fallthrough to false will mean that input stops if an input is disconnected
// Setting fallthrough to true will mean that the next available type will be used
// In this order: k_wheel_type -> k_controller_type -> k_keyboard_type
input_t* input_create(heap_t* heap, wm_window_t* window, control_type_t controller_type, map_t* controller_map, map_t* key_map, bool fallthrough);

// Destroys a previously created input object
void input_destroy(input_t* input);

// Updates the state of the input
// If controller_type is k_controller_type or k_wheel_type and fallthrough is true,
// it checks for controllers over a set interval of time
void input_pump(input_t* input);

// Get a mask of the currently held buttons
uint32_t input_get_key_mask(input_t* input);

// Get a mask of the current input axes
void input_get_axes(input_t* input, int* x, int* y);

// Get the current input type
uint32_t get_input_type(input_t* input);

