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

//////////////////////////////////////////////////////////////////////////////
// Definitions
//////////////////////////////////////////////////////////////////////////////
#define GLYPH_WIDTH  (9)
#define GLYPH_HEIGHT (16)

// element flags
#define ELEMENT_VERTICAL_FILL      (1 << 16)
#define ELEMENT_HORIZONTAL_FILL    (1 << 17)
#define ELEMENT_DESTROY            (1 << 30)
#define ELEMENT_DESTROY_DESCENDENT (1 << 31)

#define MIN(a, b) ((a) <= (b) ? (a) : (b))
#define MAX(a, b) ((a) >= (b) ? (a) : (b))

typedef struct Window Window;
typedef struct Element Element;

typedef struct {
	int l, r, t, b;
} Rect;

Rect rect_make(int l, int r, int t, int b);

typedef enum {
	MSG_NONE,
	MSG_LAYOUT,
	MSG_GET_WIDTH,         // data_int should be set to the expected height of the element, otherwise 0.
	MSG_GET_HEIGHT,        // data_int should be set to the expected width of the element, otherwise 0.
	MSG_PAINT,
	MSG_MOUSE_MOVE,
	MSG_UPDATE,
	MSG_MOUSE_LEFT_DOWN,   // Left mouse button pressed. (Sent to the element the mouse cursor is over.)
	MSG_MOUSE_LEFT_UP,     // Left mouse button released. (Sent to the element MSG_LEFT_DOWN was sent to.)
	MSG_MOUSE_MIDDLE_DOWN, // Middle mouse button pressed. (Sent to the element the mouse cursor is over.)
	MSG_MOUSE_MIDDLE_UP,   // Middle mouse button released. (Sent to the element MSG_MIDDLE_DOWN was sent to.)
	MSG_MOUSE_RIGHT_DOWN,  // Right mouse button pressed. (Sent to the element the mouse cursor is over.)
	MSG_MOUSE_RIGHT_UP,    // Right mouse button released. (Sent to the element MSG_RIGHT_DOWN was sent to.)
	MSG_MOUSE_DRAG,        // Mouse moved while holding buttons. (Sent to the element MSG_*_DOWN was sent to.)
	MSG_CLICKED,           // Left mouse button released while hovering over the element that MSG_LEFT_UP was sent to.
	MSG_DESTROY,
	MSG_USER,
} Message;

typedef enum {
	UPDATE_NONE,
	UPDATE_HOVERED,
	UPDATE_PRESSED,
} UpdateKind;

typedef int (*MessageHandler)(struct Element *element, Message message, int data_int, void *data_ptr);

struct Element {
	uint32_t flags; // First 16 bits are specific to the type of element.
					// The higher order 16 bits are common to all elements.
	uint32_t child_count;
	Rect bounds, clip;
	Element *parent;
	Element **children;
	Window *window;
	MessageHandler message_class, message_user;
	void *cp; // Context pointer (for user).
};

typedef struct {
	Element element;
	char *text;
	int text_bytes;
} Button; 

typedef struct {
	Element element;
	char *text;
	int text_bytes;
} Label; 

typedef struct {
	Element element;
	Rect padding;
	int gap; // space between each child element
} Panel; 

typedef struct {
	Rect clip;         // The rectangle the element should draw into.
	uint32_t *bits;    // The bitmap itself. bits[y * painter->width + x] gives the RGB value of pixel (x, y).
	int width, height; // The width and height of the bitmap.
} Painter;

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

// Returns true if the rectangle has a positive width and height.
bool rect_valid(Rect a);

// If the rectangles don't overlap, an invalid rectangle is returned
Rect rect_intersection(Rect a, Rect b);

// Returns the smallest rectangle containing both of the input rectangles.
Rect rect_bounding(Rect a, Rect b);

// Returns true if all sides are equal.
bool rect_equals(Rect a, Rect b);

// Returns true if the pixel with its top-left at the given coordinate is
// contained inside the rectangle.
bool rect_contains(Rect a, int x, int y);


Element *element_create(int bytes, Element *parent, uint32_t flags, MessageHandler message_class);
int element_message(Element *element, Message message, int data_int, void *data_ptr);
void element_move(Element *element, Rect bounds, bool always_layout);
void element_repaint(Element *element, Rect *region);
Element *element_find_by_point(Element *element, int x, int y);
void element_destroy(Element *element);

// buttons
Button *button_create(Element *parent, uint32_t flags, char *text, int text_bytes);

// labels
Label *label_create(Element *parent, uint32_t flags, char *text, int text_bytes);
void label_set_text(Label *label, char *text, int text_bytes);

// layout panels (horizontal and vertical)
Panel *panel_create(Element *parent, uint32_t flags);

void draw_block(Painter *painter, Rect rect, uint32_t color);
void draw_rect(Painter *painter, Rect r, uint32_t fill_color, uint32_t border_color);
void draw_string(Painter *painter, Rect bounds, char *string, int bytes, uint32_t color, bool align_center);

void platform_init(void);
Window *platform_create_window(const char *title, int width, int height);
int platform_message_loop(void);
void platform_window_end_paint(Window *window, Painter *painter);

GlobalState global_state;

//////////////////////////////////////////////////////////////////////////////
// Helper functions
//////////////////////////////////////////////////////////////////////////////
void string_copy(char **dest, int *dest_bytes, const char *source, int source_bytes) {
	if (source_bytes == -1) source_bytes = (int)strlen(source);
	*dest = realloc(*dest, source_bytes);
	*dest_bytes = source_bytes;
	memcpy(*dest, source, source_bytes);
}

void print_rect(const char *prefix, Rect x) {
	fprintf(stderr, "%s: %d->%d; %d->%d\n", prefix, x.l, x.r, x.t, x.b);
}


//////////////////////////////////////////////////////////////////////////////
// Rectangle functions
//////////////////////////////////////////////////////////////////////////////
Rect rect_make(int l, int r, int t, int b) {
	return (Rect){.l=l, .r=r, .t=t, .b=b};
}

// Returns true if the rectangle has a positive width and height.
bool rect_valid(Rect a) {
	return a.r > a.l && a.b > a.t;
}

// Returns the intersection of the rectangles. If the rectangles don't overlap,
// an invalid rectangle is returned (as per rect_valid)
Rect rect_intersection(Rect a, Rect b) {
	int l = MAX(a.l, b.l);
	int r = MIN(a.r, b.r);
	int t = MAX(a.t, b.t);
	int _b = MIN(a.b, b.b);
	return (Rect){.l=l, .r=r, .t=t, .b=_b};
}

// Returns the smallest rectangle containing both of the input rectangles.
Rect rect_bounding(Rect a, Rect b) {
	int l = MIN(a.l, b.l);
	int r = MAX(a.r, b.r);
	int t = MIN(a.t, b.t);
	int _b = MAX(a.b, b.b);
	return (Rect){.l=l, .r=r, .t=t, .b=_b};
}

// Returns true if all sides are equal.
bool rect_equals(Rect a, Rect b) {
	return a.l == b.l && a.r == b.r && a.t == b.t && a.b == b.b;
}

// Returns true if the pixel with its top-left at the given coordinate is
// contained inside the rectangle.
bool rect_contains(Rect a, int x, int y) {
	return x >= a.l && x < a.r && y >= a.t && y < a.b;
}

//////////////////////////////////////////////////////////////////////////////
// Element functions
//////////////////////////////////////////////////////////////////////////////
Element *element_create(int bytes, Element *parent, uint32_t flags, MessageHandler message_class) {
	assert(bytes >= sizeof(Element));
	Element *element = calloc(1, bytes);
	element->flags = flags;
	element->message_class = message_class;

	if (parent) {
		++parent->child_count;
		parent->children = realloc(parent->children, sizeof(Element*) * parent->child_count);
		parent->children[parent->child_count - 1] = element;
		element->parent = parent;
		element->window = parent->window;
	}
	return element;
}

int element_message(Element *element, Message message, int data_int, void *data_ptr) {
	if (message != MSG_DESTROY && (element->flags & ELEMENT_DESTROY))
		return 0;

	int result = 0;
	if (element->message_user) {
		result = element->message_user(element, message, data_int, data_ptr);
		if (result) return result;
	}
	if (element->message_class) {
		result = element->message_class(element, message, data_int, data_ptr);
	}
	return result;
}

void element_move(Element *element, Rect bounds, bool always_layout) {
	Rect old_clip = element->clip;
	element->clip = rect_intersection(element->parent->clip, bounds);

	if (!rect_equals(element->bounds, bounds) || 
		!rect_equals(element->clip, old_clip) || 
		always_layout) 
	{
		element->bounds = bounds;
		element_message(element, MSG_LAYOUT, 0, 0);
	}

	// NOTE(shaw): Currently user is required to make sure repaint is done on layout changes
	// should this always be called for them here?
	// element_repaint(element, NULL);
}

void element_repaint(Element *element, Rect *region) {
	if (!region) region = &element->bounds;

	Rect r = rect_intersection(element->clip, *region);
	if (rect_valid(r)) {
		if (rect_valid(element->window->update_region)) {
			element->window->update_region = rect_bounding(element->window->update_region, r);
		} else {
			element->window->update_region = r;
		}
	}
}

Element *element_find_by_point(Element *element, int x, int y) {
	for (uintptr_t i = 0; i < element->child_count; ++i) {
		Element *child = element->children[i];
		if (rect_contains(child->clip, x, y)) {
			return element_find_by_point(child, x, y);
		}
	}
	return element;
}

void element_destroy(Element *element) {
	// If the element is already marked for destruction, there's nothing to do.
	if (element->flags & ELEMENT_DESTROY) {
		return;
	}

	// Set the flag indicating the element needs to be destroyed in the next ui_update(),
	// that is, once the input event is finished processing.
	element->flags |= ELEMENT_DESTROY;

	// Mark the ancestors of this element with a flag so the we can find 
	// this element in ui_update() when traversing the hierarchy.
	Element *ancestor = element->parent;
	while (ancestor) {
		ancestor->flags |= ELEMENT_DESTROY_DESCENDENT;
		ancestor = ancestor->parent;
	}

	// Recurse to destroy all the descendents.
	for (uintptr_t i = 0; i < element->child_count; ++i) {
		element_destroy(element->children[i]);
	}
}

//////////////////////////////////////////////////////////////////////////////
// Buttons
//////////////////////////////////////////////////////////////////////////////
#define COLOR_BUTTON_LIGHT  0xFFFFFF
#define COLOR_BUTTON_MEDIUM 0x777777
#define COLOR_BUTTON_DARK   0x000000
int button_message(Element *element, Message message, int data_int, void *data_ptr) {
	Button *button = (Button*)element;
	if (message == MSG_PAINT) {
		Painter *painter = (Painter*)data_ptr;
		uint32_t bg_color = COLOR_BUTTON_LIGHT;
		uint32_t text_color = COLOR_BUTTON_DARK;
		if (element->window->hovered == element) {
			bg_color = COLOR_BUTTON_MEDIUM;
			if (element->window->pressed == element) {
				bg_color = COLOR_BUTTON_DARK;
				text_color = COLOR_BUTTON_LIGHT;
			}
		}
		draw_rect(painter, element->bounds, bg_color, text_color);
		draw_string(painter, element->bounds, button->text, button->text_bytes, text_color, true); 

	} else if (message == MSG_UPDATE) {
		element_repaint(element, NULL);

	} else if (message == MSG_GET_WIDTH) {
		return 30 + GLYPH_WIDTH * button->text_bytes;

	} else if (message == MSG_GET_HEIGHT) {
		return 25;
		
	} else if (message == MSG_DESTROY) {
		free(button->text);
	}

	return 0;
}

// text_bytes of -1 indicates a NULL terminated string
Button *button_create(Element *parent, uint32_t flags, char *text, int text_bytes) {
	Button *button = (Button*)element_create(sizeof(Button), parent, flags, button_message);
	string_copy(&button->text, &button->text_bytes, text, text_bytes);
	return button;
}

//////////////////////////////////////////////////////////////////////////////
// Labels
//////////////////////////////////////////////////////////////////////////////
#define LABEL_CENTER (1 << 0)
int label_message(Element *element, Message message, int data_int, void *data_ptr) {
	Label *label = (Label*)element;
	if (message == MSG_PAINT) {
		Painter *painter = (Painter*)data_ptr;
		draw_string(painter, element->bounds, label->text, label->text_bytes, 0x000000, 
			element->flags & LABEL_CENTER); 

	} else if (message == MSG_GET_WIDTH) {
		return GLYPH_WIDTH * label->text_bytes;

	} else if (message == MSG_GET_HEIGHT) {
		return GLYPH_HEIGHT;

	} else if (message == MSG_DESTROY) {
		free(label->text);
	}

	return 0;
}

// text_bytes of -1 indicates a NULL terminated string
Label *label_create(Element *parent, uint32_t flags, char *text, int text_bytes) {
	Label *label = (Label*)element_create(sizeof(Label), parent, flags, label_message);
	string_copy(&label->text, &label->text_bytes, text, text_bytes);
	return label;
}

void label_set_text(Label *label, char *text, int text_bytes) {
	string_copy(&label->text, &label->text_bytes, text, text_bytes);
}

//////////////////////////////////////////////////////////////////////////////
// Layout Panels
//////////////////////////////////////////////////////////////////////////////
#define PANEL_HORIZONTAL (1 << 0)
#define PANEL_WHITE      (1 << 1)
#define PANEL_GREY       (1 << 2)

int panel_layout(Panel *panel, Rect bounds, bool measure) {
	bool horizontal = panel->element.flags & PANEL_HORIZONTAL;
	int position = horizontal ? panel->padding.l : panel->padding.t;
	int padding_other_axis = horizontal ? panel->padding.t : panel->padding.l;
	int panel_width  = bounds.r - bounds.l - panel->padding.l - panel->padding.r;
	int panel_height = bounds.b - bounds.t - panel->padding.t - panel->padding.b;

	int count = 0;
	int num_fill = 0;
	int per_fill = 0; 
	int available = horizontal ? panel_width : panel_height;

	// if element is FILL on secondary axis, set its width to panel width
	// if element is FILL on primary axis, first get combined size of all
	// static elements, then divide remaining space evenly among fill elements

	for (uintptr_t i = 0; i < panel->element.child_count; ++i) {
		Element *child = panel->element.children[i];

		if (child->flags & ELEMENT_DESTROY) continue;

		++count;
		bool fill_vertical   = child->flags & ELEMENT_VERTICAL_FILL;
		bool fill_horizontal = child->flags & ELEMENT_HORIZONTAL_FILL;

		if (horizontal) {
			if (fill_horizontal) {
				++num_fill;
			} else if (available > 0) {
				available -= element_message(child, MSG_GET_WIDTH, panel_height, 0);
			}
		} else {
			if (fill_vertical) {
				++num_fill;
			} else if (available > 0) {
				available -= element_message(child, MSG_GET_HEIGHT, panel_width, 0);
			}
		}
	}

	if (count) 
		available -= (count - 1) * panel->gap;

	if (available > 0 && num_fill)
		per_fill = available / num_fill;


	for (uintptr_t i = 0; i < panel->element.child_count; ++i) {
		Element *child = panel->element.children[i];
		if (child->flags & ELEMENT_DESTROY) continue;

		bool fill_vertical   = child->flags & ELEMENT_VERTICAL_FILL;
		bool fill_horizontal = child->flags & ELEMENT_HORIZONTAL_FILL;
		int child_width  = 0;
		int child_height = 0;

		if (horizontal) {
			if (fill_vertical) {
				child_height = panel_height;
			} else {
				int expected_width = fill_horizontal ? per_fill : 0;
				child_height = element_message(child, MSG_GET_HEIGHT, expected_width, 0);
			}

			child_width = fill_horizontal ? per_fill : element_message(child, MSG_GET_WIDTH, child_height, 0);

			Rect new_pos = {
				.l = bounds.l + position,
				.r = bounds.l + position + child_width,
				.t = bounds.t + padding_other_axis + (panel_height - child_height)/2,
				.b = bounds.t + padding_other_axis + (panel_height + child_height)/2,
			};
			if (!measure) element_move(child, new_pos, false);
			position += child_width + panel->gap;

		} else { 
			// vertical layout
			if (fill_horizontal) {
				child_width = panel_width;
			} else {
				int expected_height = fill_vertical ? per_fill : 0;
				child_width = element_message(child, MSG_GET_WIDTH, expected_height, 0);
			}

			child_height = fill_vertical ? per_fill : element_message(child, MSG_GET_HEIGHT, child_width, 0);

			Rect new_pos = {
				.l = bounds.l + padding_other_axis + (panel_width - child_width)/2, 
				.r = bounds.l + padding_other_axis + (panel_width + child_width)/2, 
				.t = bounds.t + position,
				.b = bounds.t + position + child_height,
			};
			if (!measure) element_move(child, new_pos, false);
			position += child_height + panel->gap;
		}
	}

	// If there was at least 1 element, we added a gap to the end of position that isn't used, so remove it.
	if (count) 
		position -= panel->gap;

	// Add the space from the border at the other end of the panel on the main axis.
	position += horizontal ? panel->padding.r : panel->padding.b;

	return position;
}

// return the largest child width or height depending on if the panel is vertical or horizontal
int panel_measure(Panel *panel) {
	int dimension = 0;
	bool horizontal = panel->element.flags & PANEL_HORIZONTAL;
	for (uintptr_t i = 0; i < panel->element.child_count; ++i) {
		Element *child = panel->element.children[i];
		if (child->flags & ELEMENT_DESTROY) continue;
		Message message = horizontal ? MSG_GET_HEIGHT : MSG_GET_WIDTH;
		int child_dimension = element_message(child, message, 0, 0);
		if (child_dimension > dimension) dimension = child_dimension;
	}

	int padding = horizontal 
		? panel->padding.t + panel->padding.b 
		: panel->padding.l + panel->padding.r;

	return dimension + padding;
}

int panel_message(Element *element, Message message, int data_int, void *data_ptr) {
	Panel *panel = (Panel*) element;
	bool horizontal = element->flags & PANEL_HORIZONTAL;

	if (message == MSG_PAINT) {
		Painter *painter = (Painter*) data_ptr;
		if (element->flags & PANEL_WHITE) {
			draw_block(painter, element->bounds, 0xFFFFFF);
		} else if (element->flags & PANEL_GREY) {
			draw_block(painter, element->bounds, 0xCCCCCC);
		} else {
			// transparent background
		}

	} else if (message == MSG_LAYOUT) {
		panel_layout(panel, element->bounds, false);
		element_repaint(element, NULL);

	} else if (message == MSG_GET_WIDTH) {
		return horizontal 
			? panel_layout(panel, rect_make(0, 0, 0, data_int), true)
			: panel_measure(panel);

	} else if (message == MSG_GET_HEIGHT) {
		return horizontal
			? panel_measure(panel)
			: panel_layout(panel, rect_make(0, data_int, 0, 0), true);
	}

	return 0;
}

Panel *panel_create(Element *parent, uint32_t flags) {
	return (Panel*) element_create(sizeof(Panel), parent, flags, panel_message);
}


//////////////////////////////////////////////////////////////////////////////
// Drawing helpers
//////////////////////////////////////////////////////////////////////////////

// Taken from https://commons.wikimedia.org/wiki/File:Codepage-437.png
// Public domain.
const uint64_t _font[] = {
	0x0000000000000000UL, 0x0000000000000000UL, 0xBD8181A5817E0000UL, 0x000000007E818199UL, 0xC3FFFFDBFF7E0000UL, 0x000000007EFFFFE7UL, 0x7F7F7F3600000000UL, 0x00000000081C3E7FUL, 
	0x7F3E1C0800000000UL, 0x0000000000081C3EUL, 0xE7E73C3C18000000UL, 0x000000003C1818E7UL, 0xFFFF7E3C18000000UL, 0x000000003C18187EUL, 0x3C18000000000000UL, 0x000000000000183CUL, 
	0xC3E7FFFFFFFFFFFFUL, 0xFFFFFFFFFFFFE7C3UL, 0x42663C0000000000UL, 0x00000000003C6642UL, 0xBD99C3FFFFFFFFFFUL, 0xFFFFFFFFFFC399BDUL, 0x331E4C5870780000UL, 0x000000001E333333UL, 
	0x3C666666663C0000UL, 0x0000000018187E18UL, 0x0C0C0CFCCCFC0000UL, 0x00000000070F0E0CUL, 0xC6C6C6FEC6FE0000UL, 0x0000000367E7E6C6UL, 0xE73CDB1818000000UL, 0x000000001818DB3CUL, 
	0x1F7F1F0F07030100UL, 0x000000000103070FUL, 0x7C7F7C7870604000UL, 0x0000000040607078UL, 0x1818187E3C180000UL, 0x0000000000183C7EUL, 0x6666666666660000UL, 0x0000000066660066UL, 
	0xD8DEDBDBDBFE0000UL, 0x00000000D8D8D8D8UL, 0x6363361C06633E00UL, 0x0000003E63301C36UL, 0x0000000000000000UL, 0x000000007F7F7F7FUL, 0x1818187E3C180000UL, 0x000000007E183C7EUL, 
	0x1818187E3C180000UL, 0x0000000018181818UL, 0x1818181818180000UL, 0x00000000183C7E18UL, 0x7F30180000000000UL, 0x0000000000001830UL, 0x7F060C0000000000UL, 0x0000000000000C06UL, 
	0x0303000000000000UL, 0x0000000000007F03UL, 0xFF66240000000000UL, 0x0000000000002466UL, 0x3E1C1C0800000000UL, 0x00000000007F7F3EUL, 0x3E3E7F7F00000000UL, 0x0000000000081C1CUL, 
	0x0000000000000000UL, 0x0000000000000000UL, 0x18183C3C3C180000UL, 0x0000000018180018UL, 0x0000002466666600UL, 0x0000000000000000UL, 0x36367F3636000000UL, 0x0000000036367F36UL, 
	0x603E0343633E1818UL, 0x000018183E636160UL, 0x1830634300000000UL, 0x000000006163060CUL, 0x3B6E1C36361C0000UL, 0x000000006E333333UL, 0x000000060C0C0C00UL, 0x0000000000000000UL, 
	0x0C0C0C0C18300000UL, 0x0000000030180C0CUL, 0x30303030180C0000UL, 0x000000000C183030UL, 0xFF3C660000000000UL, 0x000000000000663CUL, 0x7E18180000000000UL, 0x0000000000001818UL, 
	0x0000000000000000UL, 0x0000000C18181800UL, 0x7F00000000000000UL, 0x0000000000000000UL, 0x0000000000000000UL, 0x0000000018180000UL, 0x1830604000000000UL, 0x000000000103060CUL, 
	0xDBDBC3C3663C0000UL, 0x000000003C66C3C3UL, 0x1818181E1C180000UL, 0x000000007E181818UL, 0x0C183060633E0000UL, 0x000000007F630306UL, 0x603C6060633E0000UL, 0x000000003E636060UL, 
	0x7F33363C38300000UL, 0x0000000078303030UL, 0x603F0303037F0000UL, 0x000000003E636060UL, 0x633F0303061C0000UL, 0x000000003E636363UL, 0x18306060637F0000UL, 0x000000000C0C0C0CUL, 
	0x633E6363633E0000UL, 0x000000003E636363UL, 0x607E6363633E0000UL, 0x000000001E306060UL, 0x0000181800000000UL, 0x0000000000181800UL, 0x0000181800000000UL, 0x000000000C181800UL, 
	0x060C183060000000UL, 0x000000006030180CUL, 0x00007E0000000000UL, 0x000000000000007EUL, 0x6030180C06000000UL, 0x00000000060C1830UL, 0x18183063633E0000UL, 0x0000000018180018UL, 
	0x7B7B63633E000000UL, 0x000000003E033B7BUL, 0x7F6363361C080000UL, 0x0000000063636363UL, 0x663E6666663F0000UL, 0x000000003F666666UL, 0x03030343663C0000UL, 0x000000003C664303UL, 
	0x66666666361F0000UL, 0x000000001F366666UL, 0x161E1646667F0000UL, 0x000000007F664606UL, 0x161E1646667F0000UL, 0x000000000F060606UL, 0x7B030343663C0000UL, 0x000000005C666363UL, 
	0x637F636363630000UL, 0x0000000063636363UL, 0x18181818183C0000UL, 0x000000003C181818UL, 0x3030303030780000UL, 0x000000001E333333UL, 0x1E1E366666670000UL, 0x0000000067666636UL, 
	0x06060606060F0000UL, 0x000000007F664606UL, 0xC3DBFFFFE7C30000UL, 0x00000000C3C3C3C3UL, 0x737B7F6F67630000UL, 0x0000000063636363UL, 0x63636363633E0000UL, 0x000000003E636363UL, 
	0x063E6666663F0000UL, 0x000000000F060606UL, 0x63636363633E0000UL, 0x000070303E7B6B63UL, 0x363E6666663F0000UL, 0x0000000067666666UL, 0x301C0663633E0000UL, 0x000000003E636360UL, 
	0x18181899DBFF0000UL, 0x000000003C181818UL, 0x6363636363630000UL, 0x000000003E636363UL, 0xC3C3C3C3C3C30000UL, 0x00000000183C66C3UL, 0xDBC3C3C3C3C30000UL, 0x000000006666FFDBUL, 
	0x18183C66C3C30000UL, 0x00000000C3C3663CUL, 0x183C66C3C3C30000UL, 0x000000003C181818UL, 0x0C183061C3FF0000UL, 0x00000000FFC38306UL, 0x0C0C0C0C0C3C0000UL, 0x000000003C0C0C0CUL, 
	0x1C0E070301000000UL, 0x0000000040607038UL, 0x30303030303C0000UL, 0x000000003C303030UL, 0x0000000063361C08UL, 0x0000000000000000UL, 0x0000000000000000UL, 0x0000FF0000000000UL, 
	0x0000000000180C0CUL, 0x0000000000000000UL, 0x3E301E0000000000UL, 0x000000006E333333UL, 0x66361E0606070000UL, 0x000000003E666666UL, 0x03633E0000000000UL, 0x000000003E630303UL, 
	0x33363C3030380000UL, 0x000000006E333333UL, 0x7F633E0000000000UL, 0x000000003E630303UL, 0x060F0626361C0000UL, 0x000000000F060606UL, 0x33336E0000000000UL, 0x001E33303E333333UL, 
	0x666E360606070000UL, 0x0000000067666666UL, 0x18181C0018180000UL, 0x000000003C181818UL, 0x6060700060600000UL, 0x003C666660606060UL, 0x1E36660606070000UL, 0x000000006766361EUL, 
	0x18181818181C0000UL, 0x000000003C181818UL, 0xDBFF670000000000UL, 0x00000000DBDBDBDBUL, 0x66663B0000000000UL, 0x0000000066666666UL, 0x63633E0000000000UL, 0x000000003E636363UL, 
	0x66663B0000000000UL, 0x000F06063E666666UL, 0x33336E0000000000UL, 0x007830303E333333UL, 0x666E3B0000000000UL, 0x000000000F060606UL, 0x06633E0000000000UL, 0x000000003E63301CUL, 
	0x0C0C3F0C0C080000UL, 0x00000000386C0C0CUL, 0x3333330000000000UL, 0x000000006E333333UL, 0xC3C3C30000000000UL, 0x00000000183C66C3UL, 0xC3C3C30000000000UL, 0x0000000066FFDBDBUL, 
	0x3C66C30000000000UL, 0x00000000C3663C18UL, 0x6363630000000000UL, 0x001F30607E636363UL, 0x18337F0000000000UL, 0x000000007F63060CUL, 0x180E181818700000UL, 0x0000000070181818UL, 
	0x1800181818180000UL, 0x0000000018181818UL, 0x18701818180E0000UL, 0x000000000E181818UL, 0x000000003B6E0000UL, 0x0000000000000000UL, 0x63361C0800000000UL, 0x00000000007F6363UL, 
};

void draw_block(Painter *painter, Rect rect, uint32_t color) {
	// Intersect the rectangle we want to fill with the clip, i.e. the rectangle we're allowed to draw into.
	rect = rect_intersection(painter->clip, rect);

	for (int y = rect.t; y < rect.b; ++y) {
		for (int x = rect.l; x < rect.r; ++x) {
			painter->bits[y * painter->width + x] = color;
		}
	}
}

void draw_rect(Painter *painter, Rect r, uint32_t fill_color, uint32_t border_color) {
	// border top
	draw_block(painter, (Rect){r.l, r.r, r.t, r.t+1}, border_color);
	// border right
	draw_block(painter, (Rect){r.r-1, r.r, r.t, r.b}, border_color);
	// border bottom
	draw_block(painter, (Rect){r.l, r.r, r.b-1, r.b}, border_color);
	// border left
	draw_block(painter, (Rect){r.l, r.l+1, r.t, r.b}, border_color);
	// fill
	draw_block(painter, (Rect){r.l+1, r.r-1, r.t+1, r.b-1}, fill_color);
}

void draw_string(Painter *painter, Rect bounds, char *string, int bytes, uint32_t color, bool align_center) {
	// setup the clipping region
	Rect old_clip = painter->clip;
	painter->clip = rect_intersection(old_clip, bounds);

	// Work out where to start drawing the text within the provided bounds.
	int x = bounds.l;
	int y = (bounds.t + bounds.b - GLYPH_HEIGHT) / 2;
	if (align_center) x += (int)(bounds.r - bounds.l - bytes * GLYPH_WIDTH) / 2;

	// For every character in the string...
	for (uintptr_t i = 0; i < bytes; ++i) {
		uint8_t c = string[i];
		if (c > 127) c = '?';

		// Work out where the corresponding glyph is to be drawn.
		Rect rect = rect_intersection(painter->clip, rect_make(x, x + 8, y, y + 16));
		uint8_t *data = (uint8_t*) _font + c * 16;

		// Blit the glyph bits.
		for (int i = rect.t; i < rect.b; ++i) {
			uint32_t *bits = painter->bits + i * painter->width + rect.l;
			uint8_t byte = data[i - y];

			for (int j = rect.l; j < rect.r; ++j) {
				if (byte & (1 << (j - x))) {
					*bits = color;
				}
				++bits;
			}
		}

		// Advance to the position of the next glyph.
		x += GLYPH_WIDTH;
	}

	// Restore the old clipping region.
	painter->clip = old_clip;
}

//////////////////////////////////////////////////////////////////////////////
// Core UI Code
//////////////////////////////////////////////////////////////////////////////
void ui_update(void);

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
	for (uintptr_t i = 0; i < element->child_count; ++i) {
		painter->clip = clip;
		ui_element_paint(element->children[i], painter);
	}
}

bool ui_element_destroy(Element *element) {
	// Is there some descendent of this element that needs to be destroyed?
	if (element->flags & ELEMENT_DESTROY_DESCENDENT) {
		// Clear the flag, ready for the next update cycle.
		element->flags &= ~ELEMENT_DESTROY_DESCENDENT;

		// For each child,
		for (uintptr_t i = 0; i < element->child_count; ++i) { // Exercise to the reader: how can this specific line of code be optimized?
			// Recurse. Was this specific element destroyed?
			if (ui_element_destroy(element->children[i])) {
				// Yes, so remove it from our list of children.
				memmove(&element->children[i], &element->children[i + 1], sizeof(Element *) * (element->child_count - i - 1));
				element->child_count--;
				i--;
			}
		}
	}

	// Does this element need to be destroyed?
	if (element->flags & ELEMENT_DESTROY) {
		// Sent it the MSG_DESTROY message.
		element_message(element, MSG_DESTROY, 0, 0);

		// If this element is being pressed, clear the pressed field in the Window.
		if (element->window->pressed == element) {
			ui_window_set_pressed(element->window, NULL, 0);
		}

		// If this element is being hovered, reset the hovered field in the Window to the default.
		if (element->window->hovered == element) {
			element->window->hovered = &element->window->element;
		}

		// Free the element's children list, and the element structure itself.
		free(element->children);
		free(element);
		return true;

	} else {
		// The element was not destroyed.
		return false;
	}
}

void ui_update(void) {
	for (uintptr_t i = 0; i < global_state.window_count; ++i) {
		Window *window = global_state.windows[i];

		// destroy all elements marked for destruction
		if (ui_element_destroy(&window->element)) {

			// The whole window has been destroyed, so removed it from our list.
			global_state.windows[i] = global_state.windows[global_state.window_count - 1];
			--global_state.window_count;
			--i;


		// Is there anything marked for repaint?
		} else if (rect_valid(window->update_region)) {
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

//////////////////////////////////////////////////////////////////////////////
// Platform specific code
//////////////////////////////////////////////////////////////////////////////
void ui_update(void);
void ui_window_input_event(Window *window, Message message, int data_int, void *data_ptr);
void ui_window_set_pressed(Window *window, Element *element, MouseButton button);


#ifdef PLATFORM_WIN32

int platform_window_message(Element *element, Message message, int data_int, void *data_ptr) {
	(void) data_int;
	(void) data_ptr;

	if (message == MSG_DESTROY) {
		Window *window = (Window *) element;
		free(window->bits);
		SetWindowLongPtr(window->hwnd, GWLP_USERDATA, 0);
		DestroyWindow(window->hwnd);
	} else if (message == MSG_LAYOUT && element->child_count > 0) {
		element_move(element->children[0], element->bounds, false);
		element_repaint(element, NULL);
	}
	return 0;
}

void platform_window_end_paint(Window *window, Painter *painter) {
	(void)painter;
	HDC dc = GetDC(window->hwnd);
	BITMAPINFOHEADER info = { 0 };
	info.biSize = sizeof(info);
	info.biWidth = window->width; 
	info.biHeight = window->height;
	info.biPlanes = 1;
	info.biBitCount = 32;
	StretchDIBits(dc, 
		window->update_region.l, window->update_region.t, 
		window->update_region.r - window->update_region.l, window->update_region.b - window->update_region.t,
		window->update_region.l, window->update_region.b + 1, 
		window->update_region.r - window->update_region.l, window->update_region.t - window->update_region.b,
		window->bits, (BITMAPINFO *) &info, DIB_RGB_COLORS, SRCCOPY);
	ReleaseDC(window->hwnd, dc);
}

LRESULT CALLBACK win32_window_proc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
	Window *window = (Window *) GetWindowLongPtr(hwnd, GWLP_USERDATA);

	if (!window) {
		return DefWindowProc(hwnd, message, wParam, lParam);
	}

	if (message == WM_CLOSE) {
		PostQuitMessage(0);
	} else if (message == WM_SIZE) {
		RECT client;
		GetClientRect(hwnd, &client);
		window->width = client.right;
		window->height = client.bottom;
		window->bits = (uint32_t*)realloc(window->bits, window->width * window->height * 4);
		window->element.bounds = rect_make(0, window->width, 0, window->height);
		window->element.clip = rect_make(0, window->width, 0, window->height);
		element_message(&window->element, MSG_LAYOUT, 0, 0);
		ui_update();
	} else if (message == WM_MOUSEMOVE) {
		if (!window->tracking_leave) {
			window->tracking_leave = true;
			TRACKMOUSEEVENT leave = { 0 };
			leave.cbSize = sizeof(TRACKMOUSEEVENT);
			leave.dwFlags = TME_LEAVE;
			leave.hwndTrack = hwnd;
			TrackMouseEvent(&leave);
		}
		POINT cursor;
		GetCursorPos(&cursor);
		ScreenToClient(hwnd, &cursor);
		window->mouse_x = cursor.x;
		window->mouse_y = cursor.y;
		ui_window_input_event(window, MSG_MOUSE_MOVE, 0, 0);
	} else if (message == WM_MOUSELEAVE) {
		window->tracking_leave = false;
		if (!window->pressed) {
			window->mouse_x = -1;
			window->mouse_y = -1;
		}
		ui_window_input_event(window, MSG_MOUSE_MOVE, 0, 0);
	} else if (message == WM_LBUTTONDOWN) {
		SetCapture(hwnd);
		ui_window_input_event(window, MSG_MOUSE_LEFT_DOWN, 0, 0);
	} else if (message == WM_LBUTTONUP) {
		if (window->pressed_mouse_button == MOUSE_BUTTON_LEFT) 
			ReleaseCapture();
		ui_window_input_event(window, MSG_MOUSE_LEFT_UP, 0, 0);
	} else if (message == WM_MBUTTONDOWN) {
		SetCapture(hwnd);
		ui_window_input_event(window, MSG_MOUSE_MIDDLE_DOWN, 0, 0);
	} else if (message == WM_MBUTTONUP) {
		if (window->pressed_mouse_button == MOUSE_BUTTON_MIDDLE) 
			ReleaseCapture();
		ui_window_input_event(window, MSG_MOUSE_MIDDLE_UP, 0, 0);
	} else if (message == WM_RBUTTONDOWN) {
		SetCapture(hwnd);
		ui_window_input_event(window, MSG_MOUSE_RIGHT_DOWN, 0, 0);
	} else if (message == WM_RBUTTONUP) {
		if (window->pressed_mouse_button == MOUSE_BUTTON_RIGHT) 
			ReleaseCapture();
		ui_window_input_event(window, MSG_MOUSE_RIGHT_UP, 0, 0);
	} else if (message == WM_PAINT) {
		PAINTSTRUCT paint;
		HDC dc = BeginPaint(hwnd, &paint);
		BITMAPINFOHEADER info = { 0 };
		info.biSize = sizeof(info);
		info.biWidth = window->width;
		info.biHeight = -window->height;
		info.biPlanes = 1;
		info.biBitCount = 32;
		StretchDIBits(dc, 0, 0, 
			window->element.bounds.r - window->element.bounds.l, 
			window->element.bounds.b - window->element.bounds.t, 
			0, 0, window->element.bounds.r - window->element.bounds.l, 
			window->element.bounds.b - window->element.bounds.t,
			window->bits, (BITMAPINFO*)&info, DIB_RGB_COLORS, SRCCOPY);
		EndPaint(hwnd, &paint);
	} else {
		return DefWindowProc(hwnd, message, wParam, lParam);
	}

	return 0;
}

Window *platform_create_window(const char *title, int width, int height) {
	Window *window = (Window *) element_create(sizeof(Window), NULL, 0, platform_window_message);
	window->element.window = window;
	window->hovered = &window->element;

	global_state.window_count++;
	global_state.windows = realloc(global_state.windows, sizeof(Window*) * global_state.window_count);
	global_state.windows[global_state.window_count - 1] = window;

	window->hwnd = CreateWindow("UILibraryTutorial", title, WS_OVERLAPPEDWINDOW, 
			CW_USEDEFAULT, CW_USEDEFAULT, width, height, NULL, NULL, NULL, NULL);
	SetWindowLongPtr(window->hwnd, GWLP_USERDATA, (LONG_PTR) window);
	ShowWindow(window->hwnd, SW_SHOW);
	PostMessage(window->hwnd, WM_SIZE, 0, 0);
	return window;
}

int platform_message_loop(void) {
	MSG message = {0};

	while (GetMessage(&message, NULL, 0, 0)) {
		TranslateMessage(&message);
		DispatchMessage(&message);
	}

	return (int)message.wParam;
}

void platform_init(void) {
	WNDCLASS window_class = { 0 };
	window_class.lpfnWndProc = win32_window_proc;
	window_class.hCursor = LoadCursor(NULL, IDC_ARROW);
	window_class.lpszClassName = "UILibraryTutorial";
	RegisterClass(&window_class);
}

#endif

#ifdef PLATFORM_LINUX

int platform_window_message(Element *element, Message message, int data_int, void *data_ptr) {
	(void) data_int;
	(void) data_ptr;
	if (message == MSG_DESTROY) {
		Window *window = (Window *) element;
		free(window->bits);
		window->image->data = NULL;
		XDestroyImage(window->image);
		XDestroyWindow(global_state.display, window->window);
	} else if (message == MSG_LAYOUT && element->child_count > 0) {
		element_move(element->children[0], element->bounds, false);
		element_repaint(element, NULL);
	}
	return 0;
}


void platform_window_end_paint(Window *window, Painter *painter) {
	(void) painter;
	XPutImage(global_state.display, window->window, DefaultGC(global_state.display, 0), window->image, 
		window->update_region.l, window->update_region.t, window->update_region.l, window->update_region.t,
		window->update_region.r - window->update_region.l, window->update_region.b - window->update_region.t);
}

Window *find_window(X11Window window) {
	for (uintptr_t i = 0; i < global_state.window_count; i++) {
		if (global_state.windows[i]->window == window) {
			return global_state.windows[i];
		}
	}
	return NULL;
}

Window *platform_create_window(const char *title, int width, int height) {
	Window *window = (Window *) element_create(sizeof(Window), NULL, 0, platform_window_message);
	window->element.window = window;
	window->hovered = &window->element;

	global_state.window_count++;
	global_state.windows = realloc(global_state.windows, sizeof(Window *) * global_state.window_count);
	global_state.windows[global_state.window_count - 1] = window;

	XSetWindowAttributes attributes = {};
	window->window = XCreateWindow(global_state.display, DefaultRootWindow(global_state.display), 
		0, 0, width, height, 
		0, 0, InputOutput, CopyFromParent, CWOverrideRedirect, &attributes);
	XStoreName(global_state.display, window->window, title);
	XSelectInput(global_state.display, window->window, 
		SubstructureNotifyMask | ExposureMask | PointerMotionMask | ButtonPressMask 
		| ButtonReleaseMask | KeyPressMask | KeyReleaseMask | StructureNotifyMask
		| EnterWindowMask | LeaveWindowMask | ButtonMotionMask | KeymapStateMask 
		| FocusChangeMask | PropertyChangeMask);
	XMapRaised(global_state.display, window->window);
	XSetWMProtocols(global_state.display, window->window, &global_state.windowClosedID, 1);
	window->image = XCreateImage(global_state.display, global_state.visual, 24, ZPixmap, 0, NULL, 10, 10, 32, 0);
	return window;
}

int platform_message_loop(void) {
	ui_update();
	while (true) {
		XEvent event;
		XNextEvent(global_state.display, &event);

		if (event.type == ClientMessage && (Atom) event.xclient.data.l[0] == global_state.windowClosedID) {
			return 0;
		} else if (event.type == Expose) {
			Window *window = find_window(event.xexpose.window);
			if (!window) continue;
			XPutImage(global_state.display, window->window, DefaultGC(global_state.display, 0), 
					window->image, 0, 0, 0, 0, window->width, window->height);
		} else if (event.type == ConfigureNotify) {
			Window *window = find_window(event.xconfigure.window);
			if (!window) continue;

			if (window->width != event.xconfigure.width || window->height != event.xconfigure.height) {
				window->width = event.xconfigure.width;
				window->height = event.xconfigure.height;
				window->bits = (uint32_t*)realloc(window->bits, window->width * window->height * 4);
				window->image->width = window->width;
				window->image->height = window->height;
				window->image->bytes_per_line = window->width * 4;
				window->image->data = (char *) window->bits;
				window->element.bounds = rect_make(0, window->width, 0, window->height);
				window->element.clip = rect_make(0, window->width, 0, window->height);
				element_message(&window->element, MSG_LAYOUT, 0, 0);
				ui_update();
			}
		} else if (event.type == MotionNotify) {
			Window *window = find_window(event.xmotion.window);
			if (!window) continue;
			window->mouse_x = event.xmotion.x;
			window->mouse_y = event.xmotion.y;
			ui_window_input_event(window, MSG_MOUSE_MOVE, 0, 0);
		} else if (event.type == LeaveNotify) {
			Window *window = find_window(event.xcrossing.window);
			if (!window) continue;

			if (!window->pressed) {
				window->mouse_x = -1;
				window->mouse_y = -1;
			}

			ui_window_input_event(window, MSG_MOUSE_MOVE, 0, 0);
		} else if (event.type == ButtonPress || event.type == ButtonRelease) {
			Window *window = find_window(event.xbutton.window);
			if (!window) continue;
			window->mouse_x = event.xbutton.x;
			window->mouse_y = event.xbutton.y;
			if (event.xbutton.button >= 1 && event.xbutton.button <= 3) {
				ui_window_input_event(window, 
					(Message)((event.type == ButtonPress ? MSG_MOUSE_LEFT_DOWN : MSG_MOUSE_LEFT_UP) 
					+ event.xbutton.button * 2 - 2), 0, 0);
			}
		}
	}
}

void platform_init(void) {
	global_state.display = XOpenDisplay(NULL);
	global_state.visual = XDefaultVisual(global_state.display, 0);
	global_state.windowClosedID = XInternAtom(global_state.display, "WM_DELETE_WINDOW", 0);
}

#endif

/////////////////////////////////////////
// Test usage code.
/////////////////////////////////////////

int my_button_message(Element *element, Message message, int di, void *dp) {
	if (message == MSG_CLICKED) {
		element_destroy(element);
		element_message(element->parent, MSG_LAYOUT, 0, 0);
	}
	return 0;
}

int main() {
	platform_init();
	Window *window = platform_create_window("DESTROY!", 800, 600);

	Panel *column1 = panel_create(&window->element, PANEL_GREY);
	column1->gap = 10;
	column1->padding = rect_make(15, 15, 15, 15);

	label_create(&column1->element, 0, "Pick a button!", -1);

	for (int i=0; i<12; ++i) {
		char buf[32];
		snprintf(buf, 32, "Push %d", i+1);
		Button *button = button_create(&column1->element, ELEMENT_HORIZONTAL_FILL, buf, -1);
		button->element.message_user = my_button_message;
	}

	return platform_message_loop();
}


