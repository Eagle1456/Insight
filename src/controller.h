#pragma once

#include <stdint.h>

// Structure which contains the data of the controllers
typedef struct control_t control_t;

typedef struct heap_t heap_t;

// Bitmasks for up to four players
typedef enum player_type_t{
	k_first_player = 1 << 0,
	k_second_player = 1 << 1,
	k_third_player = 1 << 2,
	k_fourth_player = 1 << 3,
} player_type_t;

// Bitmasks for each button
// Modeled after XInput gamepad structure
enum {
	k_dpad_up = 1 << 0,
	k_dpad_down = 1 << 1,
	k_dpad_left = 1 << 2,
	k_dpad_right = 1 << 3,
	k_button_start = 1 << 4,
	k_button_select = 1 << 5,
	k_thumb_left = 1 << 6,
	k_thumb_right = 1 << 7,
	k_shoulder_left = 1 << 8,
	k_shoulder_right = 1 << 9,
	
	k_controller_button_down = 1 << 12,
	k_controller_button_right = 1 << 13,
	k_controller_button_left = 1 << 14,
	k_controller_button_up = 1 << 15,
	k_LX_positive = 1 << 16,
	k_LX_negative = 1 << 17,
	k_LY_positive = 1 << 18,
	k_LY_negative = 1 << 19,
	k_RX_positive = 1 << 20,
	k_RX_negative = 1 << 21,
	k_RY_positive = 1 << 22,
	k_RY_negative = 1 << 23,
	k_LT_pressed = 1 << 24,
	k_RT_pressed = 1 << 25,
};

// Create controller structure
// This will try to find controllers up to player_num
control_t* controller_create(heap_t* heap, int player_num);

// Destroy controller structure
void controller_destroy(control_t* control);

// Update state of found controllers
uint32_t controller_pump(control_t* control);

// Find controllers up to player_num
// Recommended not to do this every frame
uint32_t controller_query(control_t* control);

// Return a mask of the current controllers connected
uint32_t get_current_players(control_t* control);

// Returns a mask of the buttons pressed on the specified controller
uint32_t controller_get_button_mask(control_t* control, player_type_t player);

// Returns axes on specified controller
void controller_get_axes(control_t* control, player_type_t player, short* lx, short* ly, short* rx, short* ry);

// Returns the value on the left and right trigger
void controller_get_triggers(control_t* control, player_type_t player, unsigned char* lt, unsigned char* rt);