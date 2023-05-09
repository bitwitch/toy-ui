typedef struct Element Element;

struct Element {
	uint32_t flags; // First 16 bits are specific to the type of element (button, label, etc.). 
					// The higher order 16 bits are common to all elements.
	uint32_t child_count; 
	Element *parent;
	Element **children; 
	Window *window; // The window at the root of the hierarchy.
	void *cp; // Context pointer (for the user of the library).
};

Element *element_create(size_t bytes, Element *parent, uint32_t flags);
