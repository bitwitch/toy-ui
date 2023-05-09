typedef struct Element Element;

typedef enum {
	MSG_USER,
} Message;

typedef int (*MessageHandler)(Element *element, Message message, int data_int, void *data_ptr);

struct Element {
	uint32_t flags; // First 16 bits are specific to the type of element (button, label, etc.). 
					// The higher order 16 bits are common to all elements.
	uint32_t child_count; 
	Element *parent;
	Element **children; 
	Window *window; // The window at the root of the hierarchy.
	MessageHandler message_class, message_user;
	void *cp; // Context pointer (for the user of the library).
};

Element *element_create(size_t bytes, Element *parent, uint32_t flags, MessageHandler message_class);
int element_message(Element *element, Message message, int data_int, void *data_ptr);
