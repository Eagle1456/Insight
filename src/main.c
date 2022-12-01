#include "debug.h"
#include "fs.h"
#include "heap.h"
#include "render.h"
#include "frogger_game.h"
#include "timer.h"
#include "wm.h"
#include "controller.h"
#include "input.h"

/*
Set of pre-defined control schemes
0: Keyboard: arrow keys
1: Controller: dpad
2: Keyboard and controller with fallthrough
3: Keyboard: WASD
4: Controller: circle buttons
5: Controller: Left joystick
6: Wheel (UP: A button, DOWN: B button, LEFT: Joystick left, RIGHT: Joystick right 
*/
input_t* create_input_test(int num, map_t* map, heap_t* heap, wm_window_t* wm) {
	input_t* input;
	map = NULL;
	switch (num) {
		case 6:
			map = heap_alloc(heap, sizeof(map), 8);
			map->input_mappings[0] = k_controller_button_down;
			map->input_mappings[1] = k_controller_button_right;
			map->input_mappings[2] = k_LX_negative;
			map->input_mappings[3] = k_LX_positive;
			input = input_create(heap, wm, k_controller_type, map, NULL, false);
			break;
		case 5:
			map = heap_alloc(heap, sizeof(map), 8);
			map->input_mappings[0] = k_LY_positive;
			map->input_mappings[1] = k_LY_negative;
			map->input_mappings[2] = k_LX_negative;
			map->input_mappings[3] = k_LX_positive;
			input = input_create(heap, wm, k_controller_type, map, NULL, false);
			break;
		case 4:
			map = heap_alloc(heap, sizeof(map), 8);
			map->input_mappings[0] = k_controller_button_up;
			map->input_mappings[1] = k_controller_button_down;
			map->input_mappings[2] = k_controller_button_left;
			map->input_mappings[3] = k_controller_button_right;
			input = input_create(heap, wm, k_controller_type, map, NULL, false);
			break;
		case 3:
			map = heap_alloc(heap, sizeof(map), 8);
			map->input_mappings[0] = k_key_w;
			map->input_mappings[1] = k_key_s;
			map->input_mappings[2] = k_key_a;
			map->input_mappings[3] = k_key_d;
			input = input_create(heap, wm, k_keyboard_type, NULL, map, false);
			break;
		case 2:
			input = input_create(heap, wm, k_controller_type, NULL, NULL, true);
			break;
		case 1:
			input = input_create(heap, wm, k_controller_type, NULL, NULL, false);
			break;
		default:
			input = input_create(heap, wm, k_keyboard_type, NULL, NULL, false);
			break;
	}
	return input;
}

int main(int argc, const char* argv[])
{
	debug_set_print_mask(k_print_info | k_print_warning | k_print_error);
	debug_install_exception_handler();

	timer_startup();

	heap_t* heap = heap_create(2 * 1024 * 1024);
	fs_t* fs = fs_create(heap, 8);
	wm_window_t* window = wm_create(heap);
	render_t* render = render_create(heap, window);
	map_t* map = NULL;
	input_t* input = create_input_test(0, map, heap, window);
	frogger_t* game = frogger_create(heap, fs, window, render, input);
	
	while (!wm_pump(window))
	{
		input_pump(input);
		frogger_update(game);
	}
	/* XXX: Shutdown render before the game. Render uses game resources. */
	if (map) { heap_free(heap, map); }
	render_destroy(render);
	frogger_destroy(game);
	input_destroy(input);
	wm_destroy(window);
	fs_destroy(fs);
	heap_destroy(heap);

	return 0;
}
