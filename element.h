typedef struct Element Element;

typedef enum Message {
	MSG_NONE,
	MSG_LAYOUT,
	MSG_USER,
} Message;

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

Element *element_create(size_t bytes, Element *parent, uint32_t flags, MessageHandler message_class);
int element_message(Element *element, Message message, int data_int, void *data_ptr);
void element_move(Element *element, Rect bounds, bool always_layout);
