#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef PLATFORM_WIN32
#define Rectangle W32Rectangle
#include <windows.h>
#undef Rectangle
#endif

#ifdef PLATFORM_LINUX
#define Window X11Window
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#undef Window
#endif

typedef struct Window Window;

#include "rect.h"
#include "element.h"

typedef enum {
	MOUSE_BUTTON_NONE,
	MOUSE_BUTTON_LEFT,
	MOUSE_BUTTON_MIDDLE,
	MOUSE_BUTTON_RIGHT,
} MouseButton;

struct Window {
	Element element;
	uint32_t *bits; // The bitmap image of the window's content.
	int width, height; // The size of the area of the window we can draw onto.
	Rect update_region; // area that needs to be repainted at the next 'update point'
	int mouse_x, mouse_y;
	Element *hovered;
	Element *pressed;
	MouseButton pressed_mouse_button;

#ifdef PLATFORM_WIN32
	HWND hwnd;
	bool tracking_leave; // used for mouse input
#endif

#ifdef PLATFORM_LINUX
	X11Window window;
	XImage *image;
#endif
};

typedef struct {
	Window **windows;
	size_t window_count;

#ifdef PLATFORM_LINUX
	Display *display;
	Visual *visual;
	Atom window_closed_id;
#endif
} GlobalState;

GlobalState global_state;

#define MIN(a, b) ((a) <= (b) ? (a) : (b))
#define MAX(a, b) ((a) >= (b) ? (a) : (b))

#include "platform.c"
#include "rect.c"
#include "element.c"


void string_copy(char **dest, size_t *dest_bytes, const char *source, ptrdiff_t source_bytes) {
	if (source_bytes == -1) source_bytes = strlen(source);
	*dest = realloc(*dest, source_bytes);
	*dest_bytes = source_bytes;
	memcpy(*dest, source, source_bytes);
}

void print_rect(const char *prefix, Rect x) {
	fprintf(stderr, "%s: %d->%d; %d->%d\n", prefix, x.l, x.r, x.t, x.b);
}

void test_helpers(void) {
	print_rect("make", rect_make(10, 20, 30, 40));
	print_rect("intersection", rect_intersection(rect_make(10, 20, 30, 40), rect_make(15, 25, 35, 45)));
	print_rect("bounding", rect_bounding(rect_make(10, 20, 30, 40), rect_make(15, 25, 35, 45)));
	fprintf(stderr, "valid: %d\n", rect_valid(rect_make(10, 20, 30, 40)));
	fprintf(stderr, "valid: %d\n", rect_valid(rect_make(20, 10, 30, 40)));
	fprintf(stderr, "equals: %d\n", rect_equals(rect_make(10, 20, 30, 40), rect_make(10, 20, 30, 40)));
	fprintf(stderr, "equals: %d\n", rect_equals(rect_make(10, 20, 30, 40), rect_make(15, 25, 35, 45)));
	fprintf(stderr, "contains: %d\n", rect_contains(rect_make(10, 20, 30, 40), 15, 35));
	fprintf(stderr, "contains: %d\n", rect_contains(rect_make(10, 20, 30, 40), 25, 35));

	char *dest = NULL;
	size_t dest_bytes = 0;
	string_copy(&dest, &dest_bytes, "Hello!", 6);
	fprintf(stderr, "'%.*s'\n", (int) dest_bytes, dest);
	string_copy(&dest, &dest_bytes, "World!", 6);
	fprintf(stderr, "'%.*s'\n", (int) dest_bytes, dest);
	free(dest);
}

//////////////////////////////////////////////////////////////////////////////
// Core UI Code
//////////////////////////////////////////////////////////////////////////////

// element is NULL if mouse button not being pressed
// button is the mouse button that went up OR down
void ui_window_set_pressed(Window *window, Element *element, MouseButton button) {
	Element *previous = window->pressed;
	window->pressed = element;
	window->pressed_mouse_button = button;
	if (previous) element_message(previous, MSG_UPDATE, UPDATE_PRESSED, 0);
	if (element)  element_message(element, MSG_UPDATE, UPDATE_PRESSED, 0);
}

void ui_window_input_event(Window *window, Message message, int data_int, void *data_ptr) {	
	if (window->pressed) {
		if (message == MSG_MOUSE_MOVE) {
			// Mouse move events become mouse drag messages, sent to the
			// element we pressed the mouse button down over.
			element_message(window->pressed, MSG_MOUSE_DRAG, data_int, data_ptr);
		} else if (message == MSG_MOUSE_LEFT_UP && window->pressed_mouse_button == MOUSE_BUTTON_LEFT) {
			if (window->hovered == window->pressed) {
				// If left mouse is released on the same element that it was previously down on, 
				// send a clicked message to that element.
				element_message(window->pressed, MSG_CLICKED, data_int, data_ptr);
			}
			// stop pressing the element
			element_message(window->pressed, MSG_MOUSE_LEFT_UP, data_int, data_ptr);
			ui_window_set_pressed(window, NULL, MOUSE_BUTTON_LEFT);
		} else if (message == MSG_MOUSE_MIDDLE_UP && window->pressed_mouse_button == MOUSE_BUTTON_MIDDLE) {
			// If middle mouse is released on the same element that it was previously down on,
			// stop pressing the element
			element_message(window->pressed, MSG_MOUSE_MIDDLE_UP, data_int, data_ptr);
			ui_window_set_pressed(window, NULL, MOUSE_BUTTON_MIDDLE);
		} else if (message == MSG_MOUSE_RIGHT_UP && window->pressed_mouse_button == MOUSE_BUTTON_RIGHT) {
			// If right mouse is released on the same element that it was previously down on,
			// stop pressing the element
			element_message(window->pressed, MSG_MOUSE_RIGHT_UP, data_int, data_ptr);
			ui_window_set_pressed(window, NULL, MOUSE_BUTTON_RIGHT);
		} 
	}


	if (window->pressed) {
		// While a mouse button is held, the hovered element is either the pressed element,
		// or the window element (at the root of the hierarchy).
		// Other elements are not allowed to be considered hovered until the button is released.
		// Here, we update the hovered field and send out MSG_UPDATE messages as necessary.
		bool inside = rect_contains(window->pressed->clip, window->mouse_x, window->mouse_y);
		if (inside && window->hovered == &window->element) {
			window->hovered = window->pressed;
			element_message(window->pressed, MSG_UPDATE, UPDATE_HOVERED, 0);
		} else if (!inside && window->hovered == window->pressed) {
			window->hovered = &window->element;
			element_message(window->pressed, MSG_UPDATE, UPDATE_HOVERED, 0);
		}

	} else {
		// No element is currently pressed.
		Element *hovered = element_find_by_point(&window->element, window->mouse_x, window->mouse_y);

		if (message == MSG_MOUSE_MOVE) {
			element_message(hovered, message, data_int, data_ptr);
		} else if (message == MSG_MOUSE_LEFT_DOWN) {
			// If the left mouse button is pressed, start pressing the hovered element.
			ui_window_set_pressed(window, hovered, MOUSE_BUTTON_LEFT);
			element_message(hovered, message, data_int, data_ptr);
		} else if (message == MSG_MOUSE_MIDDLE_DOWN) {
			// If the middle mouse button is pressed, start pressing the hovered element.
			ui_window_set_pressed(window, hovered, MOUSE_BUTTON_MIDDLE);
			element_message(hovered, message, data_int, data_ptr);
		} else if (message == MSG_MOUSE_RIGHT_DOWN) {
			// If the right mouse button is pressed, start pressing the hovered element.
			ui_window_set_pressed(window, hovered, MOUSE_BUTTON_RIGHT);
			element_message(hovered, message, data_int, data_ptr);
		}

		if (hovered != window->hovered) {
			Element *previous = window->hovered;
			window->hovered = hovered;
			element_message(previous, MSG_UPDATE, UPDATE_HOVERED, 0);
			element_message(hovered, MSG_UPDATE, UPDATE_HOVERED, 0);
		}
	}

	// Process any queued repaints.
	ui_update();
}

void ui_element_paint(Element *element, Painter *painter) {
	// Compute the intersection of where the element is allowed to draw, element->clip,
	// with the area requested to be drawn, painter->clip.
	Rect clip = rect_intersection(element->clip, painter->clip);

	// If the above regions do not overlap, return here,
	// and do not recurse into our descendant elements
	// (since their clip rectangles are contained within element->clip).
	if (!rect_valid(clip)) {
		return;
	}

	// Set the painter's clip and ask the element to paint itself.
	painter->clip = clip;
	element_message(element, MSG_PAINT, 0, painter);

	// Recurse into each child, restoring the clip each time.
	for (uintptr_t i = 0; i < element->child_count; i++) {
		painter->clip = clip;
		ui_element_paint(element->children[i], painter);
	}
}

void ui_update(void) {
	for (uintptr_t i = 0; i < global_state.window_count; i++) {
		Window *window = global_state.windows[i];

		// Is there anything marked for repaint?
		if (rect_valid(window->update_region)) {
			// Setup the painter using the window's buffer.
			Painter painter;
			painter.bits = window->bits;
			painter.width = window->width;
			painter.height = window->height;
			painter.clip = rect_intersection(
				rect_make(0, window->width, 0, window->height), 
				window->update_region);

			// Paint everything in the update region.
			ui_element_paint(&window->element, &painter);

			// Tell the platform layer to put the result onto the screen.
			platform_window_end_paint(window, &painter);

			// Clear the update region, ready for the next input event cycle.
			window->update_region = rect_make(0, 0, 0, 0);
		}
	}
}


/////////////////////////////////////////
// Test usage code.
/////////////////////////////////////////
Element *parentElement, *childElement;

int ParentElementMessage(Element *element, Message message, int di, void *dp) {
	if (message == MSG_PAINT) {
		draw_block((Painter *) dp, element->bounds, 0xFFCCFF);
	} else if (message == MSG_LAYOUT) {
		fprintf(stderr, "layout with bounds (%d->%d;%d->%d)\n", element->bounds.l, element->bounds.r, element->bounds.t, element->bounds.b);
		element_move(childElement, rect_make(50, 100, 50, 100), false);
	}

	return 0;
}

int ChildElementMessage(Element *element, Message message, int di, void *dp) {
	(void) di;

	if (message == MSG_PAINT) {
		draw_block((Painter *) dp, element->bounds, 0x444444);
	} else if (message == MSG_MOUSE_MOVE) {
		fprintf(stderr, "mouse move at (%d,%d)\n", element->window->mouse_x, element->window->mouse_y);
	} else if (message == MSG_MOUSE_DRAG) {
		fprintf(stderr, "mouse drag at (%d,%d)\n", element->window->mouse_x, element->window->mouse_y);
	} else if (message == MSG_UPDATE) {
		fprintf(stderr, "update %d\n", di);
	} else if (message == MSG_MOUSE_LEFT_DOWN) {
		fprintf(stderr, "left down\n");
	} else if (message == MSG_MOUSE_RIGHT_DOWN) {
		fprintf(stderr, "right down\n");
	} else if (message == MSG_MOUSE_MIDDLE_DOWN) {
		fprintf(stderr, "middle down\n");
	} else if (message == MSG_MOUSE_LEFT_UP) {
		fprintf(stderr, "left up\n");
	} else if (message == MSG_MOUSE_RIGHT_UP) {
		fprintf(stderr, "right up\n");
	} else if (message == MSG_MOUSE_MIDDLE_UP) {
		fprintf(stderr, "middle up\n");
	} else if (message == MSG_CLICKED) {
		fprintf(stderr, "clicked\n");
	}

	return 0;
}

int main() {
	platform_init();
	Window *window = platform_create_window("Hello, world", 640, 480);
	parentElement = element_create(sizeof(Element), &window->element, 0, ParentElementMessage);
	childElement = element_create(sizeof(Element), parentElement, 0, ChildElementMessage);
	return platform_message_loop();
}
