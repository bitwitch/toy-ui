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

#include "element.h"

struct Window {
	Element element;
	uint32_t *bits; // The bitmap image of the window's content.
	int width, height; // The size of the area of the window we can draw onto.

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
// Test element messages
//////////////////////////////////////////////////////////////////////////////

int ElementAMessageClass(Element *element, Message message, int di, void *dp) {
	(void) element;
	printf("A class: %d, %d, %p\n", message, di, dp);
	return message == MSG_USER + 1;
}

int ElementAMessageUser(Element *element, Message message, int di, void *dp) {
	(void) element;
	printf("A user: %d, %d, %p\n", message, di, dp);
	return message == MSG_USER + 2;
}

int ElementBMessageClass(Element *element, Message message, int di, void *dp) {
	(void) element;
	printf("B class: %d, %d, %p\n", message, di, dp);
	return 0;
}

int main() {
	platform_init();
	Window *window = platform_create_window("Hello, world", 300, 200);
	Element *elementA = element_create(sizeof(Element), &window->element, 0, ElementAMessageClass);
	elementA->message_user = ElementAMessageUser;
	Element *elementB = element_create(sizeof(Element), elementA, 0, ElementBMessageClass);
	printf("%d\n", element_message(elementA, MSG_USER + 1, 1, NULL));
	printf("%d\n", element_message(elementA, MSG_USER + 2, 2, NULL));
	printf("%d\n", element_message(elementA, MSG_USER + 3, 3, NULL));
	printf("%d\n", element_message(elementB, MSG_USER + 1, 1, NULL));
	printf("%d\n", element_message(elementB, MSG_USER + 2, 2, NULL));
	printf("%d\n", element_message(elementB, MSG_USER + 3, 3, NULL));
	return platform_message_loop();
}
