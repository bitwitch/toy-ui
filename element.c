Element *element_create(size_t bytes, Element *parent, uint32_t flags) {
	assert(bytes >= sizeof(Element));
	Element *element = calloc(1, bytes);
	element->flags = flags;

	if (parent) {
		++parent->child_count;
		parent->children = realloc(parent->children, sizeof(Element*) * parent->child_count);
		parent->children[parent->child_count - 1] = element;
		element->parent = parent;
		element->window = parent->window;
	}
	return element;
}



