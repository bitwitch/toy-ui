typedef struct Element Element;

typedef enum {
	MSG_NONE,
	MSG_LAYOUT,
	MSG_PAINT,
	MSG_MOUSE_MOVE,
	MSG_UPDATE,
	MSG_USER,
} Message;

typedef enum {
	UPDATE_NONE,
	UPDATE_HOVERED,
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
	Rect clip;         // The rectangle the element should draw into.
	uint32_t *bits;    // The bitmap itself. bits[y * painter->width + x] gives the RGB value of pixel (x, y).
	int width, height; // The width and height of the bitmap.
} Painter;

Element *element_create(size_t bytes, Element *parent, uint32_t flags, MessageHandler message_class);
int element_message(Element *element, Message message, int data_int, void *data_ptr);
void element_move(Element *element, Rect bounds, bool always_layout);
void element_repaint(Element *element, Rect *region);
Element *element_find_by_point(Element *element, int x, int y);
void draw_block(Painter *painter, Rect rect, uint32_t color);
void draw_rect(Painter *painter, Rect r, uint32_t fill_color, uint32_t border_color);
void draw_string(Painter *painter, Rect bounds, char *string, size_t bytes, uint32_t color, bool align_center);
