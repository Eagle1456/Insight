#include "wm.h"

#include "debug.h"
#include "heap.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>

typedef struct wm_window_t
{
	HWND hwnd;
	heap_t* heap;
	bool quit;
	bool has_focus;
	uint32_t mouse_mask;
	uint32_t key_mask;
	int mouse_x;
	int mouse_y;
} wm_window_t;

const struct
{
	int virtual_key;
	int ga_key;
}
k_key_map[] =
{
	{ .virtual_key = VK_LEFT, .ga_key = k_key_left, },
	{ .virtual_key = VK_RIGHT, .ga_key = k_key_right, },
	{ .virtual_key = VK_UP, .ga_key = k_key_up, },
	{ .virtual_key = VK_DOWN, .ga_key = k_key_down, },
	{ .virtual_key = 0x41, .ga_key = k_key_a, },
	{ .virtual_key = 0x42, .ga_key = k_key_b, },
	{ .virtual_key = 0x43, .ga_key = k_key_c, },
	{ .virtual_key = 0x44, .ga_key = k_key_d, },
	{ .virtual_key = 0x45, .ga_key = k_key_e, },
	{ .virtual_key = 0x46, .ga_key = k_key_f, },
	{ .virtual_key = 0x47, .ga_key = k_key_g, },
	{ .virtual_key = 0x48, .ga_key = k_key_h, },
	{ .virtual_key = 0x49, .ga_key = k_key_i, },
	{ .virtual_key = 0x4A, .ga_key = k_key_j, },
	{ .virtual_key = 0x4B, .ga_key = k_key_k, },
	{ .virtual_key = 0x4C, .ga_key = k_key_l, },
	{ .virtual_key = 0x4D, .ga_key = k_key_m, },
	{ .virtual_key = 0x4E, .ga_key = k_key_n, },
	{ .virtual_key = 0x4F, .ga_key = k_key_o, },
	{ .virtual_key = 0x50, .ga_key = k_key_p, },
	{ .virtual_key = 0x51, .ga_key = k_key_q, },
	{ .virtual_key = 0x52, .ga_key = k_key_r, },
	{ .virtual_key = 0x53, .ga_key = k_key_s, },
	{ .virtual_key = 0x54, .ga_key = k_key_t, },
	{ .virtual_key = 0x55, .ga_key = k_key_u, },
	{ .virtual_key = 0x56, .ga_key = k_key_v, },
	{ .virtual_key = 0x57, .ga_key = k_key_w, },
	{ .virtual_key = 0x58, .ga_key = k_key_x, },
	{ .virtual_key = 0x59, .ga_key = k_key_y, },
	{ .virtual_key = 0x5A, .ga_key = k_key_z, },
	{ .virtual_key = VK_LSHIFT, .ga_key = k_key_lshift, },
	{ .virtual_key = VK_RSHIFT, .ga_key = k_key_rshift, },
};

static LRESULT CALLBACK _window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	wm_window_t* win = (wm_window_t*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
	if (win)
	{
		switch (uMsg)
		{
		case WM_KEYDOWN:
   			for (int i = 0; i < _countof(k_key_map); ++i)
			{
				if (k_key_map[i].virtual_key == wParam)
				{
					win->key_mask |= k_key_map[i].ga_key;
					break;
				}
			}
			break;
		case WM_KEYUP:
			for (int i = 0; i < _countof(k_key_map); ++i)
			{
				if (k_key_map[i].virtual_key == wParam)
				{
					win->key_mask &= ~k_key_map[i].ga_key;
					break;
				}
			}
			break;
		case WM_LBUTTONDOWN:
			win->mouse_mask |= k_mouse_button_left;
			break;
		case WM_LBUTTONUP:
			win->mouse_mask &= ~k_mouse_button_left;
			break;
		case WM_RBUTTONDOWN:
			win->mouse_mask |= k_mouse_button_right;
			break;
		case WM_RBUTTONUP:
			win->mouse_mask &= ~k_mouse_button_right;
			break;
		case WM_MBUTTONDOWN:
			win->mouse_mask |= k_mouse_button_middle;
			break;
		case WM_MBUTTONUP:
			win->mouse_mask &= ~k_mouse_button_middle;
			break;

		case WM_MOUSEMOVE:
			if (win->has_focus)
			{
				// Relative mouse movement in four steps:
				// 1. Get current mouse position (old_cursor).
				// 2. Move mouse back to center of window.
				// 3. Get current mouse position (new_cursor).
				// 4. Compute relative movement (old_cursor - new_cursor).

				POINT old_cursor;
				GetCursorPos(&old_cursor);

				RECT window_rect;
				GetWindowRect(hwnd, &window_rect);
				SetCursorPos(
					(window_rect.left + window_rect.right) / 2,
					(window_rect.top + window_rect.bottom) / 2);

				POINT new_cursor;
				GetCursorPos(&new_cursor);

				win->mouse_x = old_cursor.x - new_cursor.x;
				win->mouse_y = old_cursor.y - new_cursor.y;
			}
			break;

		case WM_ACTIVATEAPP:
			ShowCursor(wParam != 0);
			win->has_focus = (wParam != 0);
			break;

		case WM_CLOSE:
			win->quit = true;
			break;
		}
	}

	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

wm_window_t* wm_create(heap_t* heap)
{
	WNDCLASS wc =
	{
		.lpfnWndProc = _window_proc,
		.hInstance = GetModuleHandle(NULL),
		.lpszClassName = L"ga2022 window class",
	};
	RegisterClass(&wc);

	HWND hwnd = CreateWindowEx(
		0,
		wc.lpszClassName,
		L"Insight",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		NULL,
		NULL,
		wc.hInstance,
		NULL);

	if (!hwnd)
	{
		debug_print(
			k_print_warning,
			"Failed to create window!\n");
		return NULL;
	}

	wm_window_t* win = heap_alloc(heap, sizeof(wm_window_t), 8);
	win->has_focus = false;
	win->hwnd = hwnd;
	win->key_mask = 0;
	win->mouse_mask = 0;
	win->quit = false;
	win->heap = heap;

	SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)win);

	// Windows are created hidden by default, so we
	// need to show it here.
	ShowWindow(hwnd, TRUE);

	return win;
}

bool wm_pump(wm_window_t* window)
{
	MSG msg = { 0 };
	while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return window->quit;
}

uint32_t wm_get_mouse_mask(wm_window_t* window)
{
	return window->mouse_mask;
}

uint32_t wm_get_key_mask(wm_window_t* window)
{
	return window->key_mask;
}

void wm_get_mouse_move(wm_window_t* window, int* x, int* y)
{
	*x = window->mouse_x;
	*y = window->mouse_y;
}

void wm_destroy(wm_window_t* window)
{
	DestroyWindow(window->hwnd);
	heap_free(window->heap, window);
}

void* wm_get_raw_window(wm_window_t* window)
{
	return window->hwnd;
}
