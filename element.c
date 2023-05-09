Element *element_create(size_t bytes, Element *parent, uint32_t flags, MessageHandler message_class) {
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
}
