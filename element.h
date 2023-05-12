#define GLYPH_WIDTH  (9)
#define GLYPH_HEIGHT (16)
#define ELEMENT_VERTICAL_FILL   (1 << 16)
#define ELEMENT_HORIZONTAL_FILL (1 << 17)

typedef struct Element Element;

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

Element *element_create(int bytes, Element *parent, uint32_t flags, MessageHandler message_class);
int element_message(Element *element, Message message, int data_int, void *data_ptr);
void element_move(Element *element, Rect bounds, bool always_layout);
void element_repaint(Element *element, Rect *region);
Element *element_find_by_point(Element *element, int x, int y);

Button *button_create(Element *parent, uint32_t flags, char *text, int text_bytes);

void draw_block(Painter *painter, Rect rect, uint32_t color);
void draw_rect(Painter *painter, Rect r, uint32_t fill_color, uint32_t border_color);
void draw_string(Painter *painter, Rect bounds, char *string, int bytes, uint32_t color, bool align_center);
