#include "controller.h"
#include "heap.h"
#include "debug.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <Xinput.h>

// These aren't already linked for some reason
#pragma comment(lib,"XInput.lib")
#pragma comment(lib,"Xinput9_1_0.lib")

#define DEADZONE 8000

// Array of player enums
static player_type_t player_array[] = {k_first_player, k_second_player, k_third_player, k_fourth_player};

typedef struct controller_t {
	unsigned int packetnum;
	uint32_t button_mask;
	short LX_axis;
	short LY_axis;
	short RX_axis;
	short RY_axis;
	unsigned char left_trigger;
	unsigned char right_trigger;
} controller_t;

typedef struct control_t {
	heap_t* heap;
	uint32_t player_mask;
	controller_t** controllers;
	int max_player_num;
} control_t;

//Function for querying controllers
//Used for checking which controllers are currently active
void query_controllers(control_t* control, int max_inputs) {
	unsigned int result;
	for (int i = 0; i < max_inputs; i++) {
		XINPUT_STATE state;
		ZeroMemory(&state, sizeof(XINPUT_STATE));

		result = XInputGetState(i, &state);

		if (result == ERROR_SUCCESS) {
			control->player_mask |= player_array[i];
			controller_t* controller = control->controllers[i];
			controller->packetnum = state.dwPacketNumber;

			XINPUT_GAMEPAD gamepad = state.Gamepad;
			controller->button_mask = gamepad.wButtons;
			controller->LX_axis = gamepad.sThumbLX;
			if (abs(controller->LX_axis) > DEADZONE) {
				if (controller->LX_axis > 0) {
					controller->button_mask |= k_LX_positive;
				}
				else {
					controller->button_mask |= k_LX_negative;
				}
			}
			controller->LY_axis = gamepad.sThumbLY;
			if (abs(controller->LY_axis) > DEADZONE) {
				if (controller->LY_axis > 0) {
					controller->button_mask |= k_LY_positive;
				}
				else {
					controller->button_mask |= k_LY_negative;
				}
			}
			controller->RX_axis = gamepad.sThumbRX;
			if (abs(controller->RX_axis) > DEADZONE) {
				if (controller->RX_axis > 0) {
					controller->button_mask |= k_RX_positive;
				}
				else {
					controller->button_mask |= k_RX_negative;
				}
			}
			controller->RY_axis = gamepad.sThumbRY;
			if (abs(controller->RY_axis) > DEADZONE) {
				if (controller->RY_axis > 0) {
					controller->button_mask |= k_RY_positive;
				}
				else {
					controller->button_mask |= k_RY_negative;
				}
			}

			controller->left_trigger = gamepad.bLeftTrigger;
			if (controller->left_trigger) {
				controller->button_mask |= k_LT_pressed;
			}
			controller->right_trigger = gamepad.bRightTrigger;
			if (controller->right_trigger) {
				controller->button_mask |= k_RT_pressed;
			}
		}
		else {
			control->player_mask &= ~(player_array[i]);
		}
	}
}

control_t* controller_create(heap_t* heap, int player_num) {
	control_t* control = heap_alloc(heap, sizeof(control_t), 8);
	control->heap = heap;
	control->max_player_num = player_num;
	control->player_mask = 0;
	control->controllers = heap_alloc(heap, sizeof(controller_t*), 8);
	for (int i = 0; i < player_num; i++) {
		control->controllers[i] = heap_alloc(heap, sizeof(controller_t), 8);
		control->controllers[i]->button_mask = 0;
		control->controllers[i]->left_trigger = 0;
		control->controllers[i]->right_trigger = 0;
	}
	query_controllers(control, player_num);
	return control;
}

// Queries inputs of currently connected controllers
// If any controllers are not connected, adjust player mask and return controllers
uint32_t controller_pump(control_t* control) {
	uint32_t mask = control->player_mask;
	uint32_t return_mask = 0;
	int index = 0;
	do {
		if ((mask & 0x01) != 0) {
			unsigned int result;
			XINPUT_STATE state;
			ZeroMemory(&state, sizeof(XINPUT_STATE));

			result = XInputGetState(index, &state);
			if (result == ERROR_SUCCESS) {
				controller_t* controller = control->controllers[index];
				if (state.dwPacketNumber != controller->packetnum) {
					controller->packetnum = state.dwPacketNumber;

					XINPUT_GAMEPAD gamepad = state.Gamepad;
					controller->button_mask = gamepad.wButtons;
					controller->LX_axis = gamepad.sThumbLX;
					if (abs(controller->LX_axis) > DEADZONE) {
						if (controller->LX_axis > 0) {
							controller->button_mask |= k_LX_positive;
						}
						else {
							controller->button_mask |= k_LX_negative;
						}
					}
					controller->LY_axis = gamepad.sThumbLY;
					if (abs(controller->LY_axis) > DEADZONE) {
						if (controller->LY_axis > 0) {
							controller->button_mask |= k_LY_positive;
						}
						else {
							controller->button_mask |= k_LY_negative;
						}
					}
					controller->RX_axis = gamepad.sThumbRX;
					if (abs(controller->RX_axis) > DEADZONE) {
						if (controller->RX_axis > 0) {
							controller->button_mask |= k_RX_positive;
						}
						else {
							controller->button_mask |= k_RX_negative;
						}
					}
					controller->RY_axis = gamepad.sThumbRY;
					if (abs(controller->RY_axis) > DEADZONE) {
						if (controller->RY_axis > 0) {
							controller->button_mask |= k_RY_positive;
						}
						else {
							controller->button_mask |= k_RY_negative;
						}
					}

					controller->left_trigger = gamepad.bLeftTrigger;
					if (controller->left_trigger) {
						controller->button_mask |= k_LT_pressed;
					}
					controller->right_trigger = gamepad.bRightTrigger;
					if (controller->right_trigger) {
						controller->button_mask |= k_RT_pressed;
					}
				}
			}
			// Controller disconnected, adjust player_mask and return which index
			else {
				control->player_mask &= ~(player_array[index]);
				return_mask |= player_array[index];
			}
		}
		mask >>= 1;
		index++;
	} while (mask != 0);
	return return_mask;
}


uint32_t controller_query(control_t* control) {
	query_controllers(control, control->max_player_num);
	return control->player_mask;
}

void controller_destroy(control_t* control) {
	for (int i = 0; i < control->max_player_num; i++) {
		heap_free(control->heap, control->controllers[i]);
	}
	heap_free(control->heap, control->controllers);
	heap_free(control->heap, control);
}

uint32_t get_current_players(control_t* control) {
	return control->player_mask;
}

uint32_t controller_get_button_mask(control_t* control, player_type_t player) {
	for (int i = 0; i < control->max_player_num; i++) {
		if (player_array[i] == player) {
			return control->controllers[i]->button_mask;
		}
	}
	return 0xFF;
}

void controller_get_axes(control_t* control, player_type_t player, short* lx, short* ly, short* rx, short* ry) {
	for (int i = 0; i < control->max_player_num; i++) {
		if (player_array[i] == player) {
			controller_t* controller = control->controllers[i];
			*lx = controller->LX_axis;
			*ly = controller->LY_axis;
			*rx = controller->RX_axis;
			*ry = controller->RY_axis;
			return;
		}
	}
}

void controller_get_triggers(control_t* control, player_type_t player, unsigned char* lt, unsigned char* rt) {
	for (int i = 0; i < control->max_player_num; i++) {
		if (player_array[i] == player) {
			controller_t* controller = control->controllers[i];
			*lt = controller->left_trigger;
			*rt = controller->right_trigger;
			return;
		}
	}
}