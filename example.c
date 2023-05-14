#include "toui.c"
#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

#define BUF(x) x // annotates that x is a stretchy buffer
#define MAX_COUNT 10
#define PROPERTY_KEY_MAX_SIZE 14
#define MSG_PROPERTY_CHANGED (MSG_USER + 1)

// helper macro for more simply defining button click handlers
#define BUTTON_HANDLE_CLICK_PROLOGUE(name) \
	int name(Element *element, Message message, int di, void *dp) { \
		if (message == MSG_CLICKED) {
#define BUTTON_HANDLE_CLICK_EPILOGUE() \
		} \
		return 0; \
	}

typedef enum {
	PROPERTY_NONE,
	PROPERTY_U32,
} PropertyKind;

typedef enum {
	OBJECT_NONE,
	OBJECT_COUNTER,
} ObjectKind;

typedef struct Property {
	PropertyKind kind;
	char key[PROPERTY_KEY_MAX_SIZE + 1];
	union {
		uint32_t u32;
	};
} Property;

typedef struct {
	ObjectKind kind;
	BUF(Property *properties);
} Object;

// The data associated with each element in the UI that mirrors 
// some property of the selected object in the document.
typedef struct {
	char *key; // The property key
	union {
		// For react_u32_label_create
		struct {
			char *prefix; // Not a copy
		} u32_label;

		// For react_u32_button_create
		struct {
			uint32_t min, max;
			int32_t click_delta;
		} u32_button;
	};
} ReactData;

typedef enum {
	STATE_CHANGE_NONE,
	STATE_CHANGE_SET_PROPERTY,
} StateChangeKind;

typedef enum {
	STATE_CHANGE_MODE_NONE,
	STATE_CHANGE_MODE_NORMAL,
	STATE_CHANGE_MODE_UNDO,
	STATE_CHANGE_MODE_REDO,
} StateChangeMode;

typedef struct {
	StateChangeKind kind;
	uint64_t object_id;
	union {
		Property property;
	};
} StateChange;


// Global Variables
struct { uint64_t key; Object value; } *objects;
uint64_t object_id_allocator;
uint64_t selected_object_id;

Element *container;
BUF(Element **react_elements);
BUF(StateChange *undo_stack);
BUF(StateChange *redo_stack);


void property_free(Property property) {
	// Nothing to do yet! None of our property types need freeing.
}

void state_change_free(StateChange step) {
	if (step.kind == STATE_CHANGE_SET_PROPERTY) {
		property_free(step.property);
	} else {
		// ... add code here when new step types are introduced.
	}
}

void object_free(Object object) {
	// Free each property.
	for (int i = 0; i < arrlen(object.properties); ++i) {
		property_free(object.properties[i]);
	}
	// And free the dynamic array used to store the properties.
	arrfree(object.properties);
}

void document_free(void) {
	for (int i = 0; i < hmlen(objects); ++i) {
		object_free(objects[i].value);
	}

	hmfree(objects);
	arrfree(react_elements);

	for (int i = 0; i < arrlen(undo_stack); ++i) {
		state_change_free(undo_stack[i]);
	}

	for (int i = 0; i < arrlen(redo_stack); ++i) {
		state_change_free(redo_stack[i]);
	}

	arrfree(undo_stack);
	arrfree(redo_stack);
}

Property object_read_any(Object *object, char *key) {
	// Look through each property in the object's array.
	for (int i = 0; i < arrlen(object->properties); ++i) {
		// Check if the key matches.
		if (0 == strcmp(object->properties[i].key, key)) {
			// If so, return the property.
			return object->properties[i];
		}
	}

	// The property was not found.
	// Return an empty property.
	// Since this has type = 0, the caller will know it is an invalid property.
	return (Property){0};
}

uint32_t object_read_u32(Object *object, char *key, uint32_t default_val) {
	Property property = object_read_any(object, key);
	return property.kind == PROPERTY_U32 ? property.u32 : default_val;
}

Object *selected_object(void) {
	return &hmgetp(objects, selected_object_id)->value;
}

// exclude can be NULL
// if key is NULL, all elements are matched
void react_update(Element *exclude, char *key) {
	for (int i = 0; i < arrlen(react_elements); i++) {
		ReactData *data = (ReactData*) react_elements[i]->context;

		if (react_elements[i] == exclude) continue;

		// If we are matching a specific key and this element's doesn't match it, skip it.
		if (key && strcmp(data->key, key)) continue;

		element_message(react_elements[i], MSG_PROPERTY_CHANGED, 0, 0);
	}
}

void populate(void);

void state_change_apply(StateChange step, Element *update_exclude, StateChangeMode mode) {
	bool changed_selected_obj = false;

	// Is the object specified in the step the same as the currently selected object?
	if (selected_object_id != step.object_id) {
		// No, so update the selected object.
		selected_object_id = step.object_id;
		changed_selected_obj = true;
	}

	Object *object = selected_object();
	StateChange reverse = {0};
	char *update_key = NULL;

	if (step.kind == STATE_CHANGE_SET_PROPERTY) {
		// store the reverse state change for undo
		reverse = step;
		reverse.property = object_read_any(object, step.property.key);
		
		// Store the property key that will need to be updated in the UI.
		update_key = step.property.key;

		bool found = false;

		// Look for the property with this key in the object.
		for (int i = 0; i < arrlen(object->properties); ++i) {
			if (0 == strcmp(object->properties[i].key, step.property.key)) {
				// Found it! Replace the property in the array.
				object->properties[i] = step.property;
				found = true;
				break;
			}
		}

		if (!found) {
			// The object did not already have the property in its array,
			// so insert it at the end.
			arrput(object->properties, step.property);
		}
	} else {
		// ... add more code when new step types are introduced
	}

	if (mode == STATE_CHANGE_MODE_NORMAL) {
		// Clear the redo stack and put the reverse step on the undo stack.
		for (int i = 0; i < arrlen(redo_stack); i++) 
			state_change_free(redo_stack[i]);
		arrfree(redo_stack);
		arrput(undo_stack, reverse);
	} else if (mode == STATE_CHANGE_MODE_UNDO) {
		// We just undid an action, so put the reverse step on the redo stack.
		arrput(redo_stack, reverse);
	} else if (mode == STATE_CHANGE_MODE_REDO) {
		// We just redid an action, so put the reverse step on the undo stack.
		arrput(undo_stack, reverse);
	}

	if (changed_selected_obj) {
		populate();
	} else {
		// The selected object stayed the same, so we only need to do a minimal update.
		// Only update the elements matching the property key that was modified.
		react_update(update_exclude, update_key);
	}

}


int react_u32_button_message(Element *element, Message message, int data_int, void *data_ptr) {
	ReactData *data = (ReactData*) element->context;

	if (message == MSG_PROPERTY_CHANGED) {
		element_repaint(element, NULL);

	} else if (message == MSG_CLICKED) {
		StateChange step = {0};
		step.kind = STATE_CHANGE_SET_PROPERTY;
		step.object_id = selected_object_id;
		step.property.kind = PROPERTY_U32;
		strcpy(step.property.key, data->key);
		step.property.u32 = data->u32_button.click_delta + object_read_u32(selected_object(), data->key, 0);

		// Double check that this new value of the property is actually in the valid range.
		if (step.property.u32 >= data->u32_button.min && step.property.u32 <= data->u32_button.max) {
			state_change_apply(step, NULL, STATE_CHANGE_MODE_NORMAL);
		}

	} else if (message == MSG_BUTTON_GET_COLOR) {
		uint32_t *color = (uint32_t*)data_ptr;
		int64_t count = data->u32_button.click_delta + object_read_u32(selected_object(), data->key, 0);
		if (count < data->u32_button.min || count > data->u32_button.max) 
			*color = 0xCCCCCC;
	} else if (message == MSG_DESTROY) {
		free(data);
	}


	return 0;
}

Button *react_u32_button_create(Element *parent, uint32_t flags, char *key, char *label, 
		                    uint32_t min, uint32_t max, int32_t click_delta) 
{
	ReactData *data = calloc(1, sizeof(ReactData));
	data->key = key;
	data->u32_button.min = min;
	data->u32_button.max = max;
	data->u32_button.click_delta = click_delta;

	Button *button = button_create(parent, flags, label, -1);
	int64_t result = click_delta + object_read_u32(selected_object(), key, 0);
	bool disabled = result < min || result > max;
	button->element.message_user = react_u32_button_message;
	button->element.context = data;
	arrput(react_elements, &button->element);
	return button;
}

int react_u32_label_message(Element *element, Message message, int data_int, void *data_ptr) {
	ReactData *data = (ReactData*) element->context;

	if (message == MSG_PROPERTY_CHANGED) {
		char buf[64];
		snprintf(buf, sizeof(buf), "%s%d", 
			data->u32_label.prefix, 
			object_read_u32(selected_object(), data->key, 0));
		label_set_text((Label*)element, buf, -1);
		element_repaint(element, NULL);
	} else if (message == MSG_DESTROY) {
		free(data);
	}

	return 0;
}

Label *react_u32_label_create(Element *parent, uint32_t flags, char *key, char *prefix) {
	ReactData *data = calloc(1, sizeof(ReactData));
	data->key = key;
	data->u32_label.prefix = prefix;

	Label *label = label_create(parent, flags, NULL, 0);
	label->element.message_user = react_u32_label_message;
	label->element.context = data;
	arrput(react_elements, &label->element);
	return label;
}

BUTTON_HANDLE_CLICK_PROLOGUE(button_undo_message) {
	if (arrlen(undo_stack)) {
		// Remove the last step from the undo stack and apply it.
		state_change_apply(arrpop(undo_stack), NULL, STATE_CHANGE_MODE_UNDO);
	}
} BUTTON_HANDLE_CLICK_EPILOGUE()

BUTTON_HANDLE_CLICK_PROLOGUE(button_redo_message) {
	if (arrlen(redo_stack)) {
		// Remove the last step from the redo stack and apply it.
		state_change_apply(arrpop(redo_stack), NULL, STATE_CHANGE_MODE_REDO);
	}
} BUTTON_HANDLE_CLICK_EPILOGUE()

BUTTON_HANDLE_CLICK_PROLOGUE(button_save_message) {
	FILE *f = fopen("example.dat", "wb");

	// Save the object ID allocator.
	fwrite(&object_id_allocator, 1, sizeof(uint64_t), f);

	// Save the number of objects in the document.
	uint32_t object_count = (uint32_t)hmlenu(objects);
	fwrite(&object_count, 1, sizeof(uint32_t), f);

	for (int i = 0; i < hmlen(objects); ++i) {
		// Save the object's key, kind and number of properties.
		fwrite(&objects[i].key, 1, sizeof(uint64_t), f);
		ObjectKind kind = objects[i].value.kind;
		fwrite(&kind, 1, sizeof(ObjectKind), f);
		uint32_t property_count = (uint32_t)arrlen(objects[i].value.properties);
		fwrite(&property_count, 1, sizeof(uint32_t), f);

		// For each property in the object...
		for (uint32_t j = 0; j < property_count; ++j) {
			// Save the property's kind annd key.
			fwrite(&objects[i].value.properties[j].kind, 1, sizeof(PropertyKind), f);
			fwrite(&objects[i].value.properties[j].key, 1, PROPERTY_KEY_MAX_SIZE + 1, f);
			// Save the property's value. Dependent on the kind.
			if (objects[i].value.properties[j].kind == PROPERTY_U32) {
				fwrite(&objects[i].value.properties[j].u32, 1, sizeof(uint32_t), f);
			} else {
				// ...
			}
		}
	}
	fclose(f);
} BUTTON_HANDLE_CLICK_EPILOGUE()

BUTTON_HANDLE_CLICK_PROLOGUE(button_load_message) {
	FILE *f = fopen("example.dat", "rb");
	if (!f) return 0;

	document_free();

	// Load the object ID allocator.
	fread(&object_id_allocator, 1, sizeof(uint64_t), f);

	// Load the number of objects in the file.
	uint32_t object_count = 0;
	fread(&object_count, 1, sizeof(uint32_t), f);

	for (uint32_t i = 0; i < object_count; i++) {
		Object object = {0};

		// Read the ID, type and number of properties.
		uint64_t id = 0;
		fread(&id, 1, sizeof(uint64_t), f);
		PropertyKind kind = 0;
		fread(&kind, 1, sizeof(PropertyKind), f);
		uint32_t property_count = 0;
		fread(&property_count, 1, sizeof(property_count), f);
		object.kind = kind;

		// Make sure that object ID allocator is actually set to a valid value.
		if (object_id_allocator < id) {
			object_id_allocator = id;
		}

		for (uint32_t j = 0; j < property_count; j++) {
			// Read the kind and key of the property.
			Property property = {0};
			fread(&property.kind, 1, sizeof(property.kind), f);
			fread(&property.key, 1, PROPERTY_KEY_MAX_SIZE + 1, f);
			property.key[PROPERTY_KEY_MAX_SIZE] = 0;

			// Read the property's value. Dependent on the kind.
			if (property.kind == PROPERTY_U32) {
				fread(&property.u32, 1, sizeof(uint32_t), f);
			} else {
				// ...
			}

			// Add the property to the array.
			arrput(object.properties, property);
		}

		// Add the object to the map.
		hmput(objects, id, object);

		// If this is a counter object, set it to be the selected object.
		if (kind == OBJECT_COUNTER) {
			selected_object_id = id;
		}
	}

	fclose(f);

	populate();

} BUTTON_HANDLE_CLICK_EPILOGUE()


void populate(void) {
	for (uintptr_t i = 0; i < container->child_count; ++i) {
		element_destroy(container->children[i]);
	}
	arrfree(react_elements);

	Object *object = selected_object();

	if (object->kind == OBJECT_COUNTER) {
		Panel *row = panel_create(container, PANEL_HORIZONTAL | ELEMENT_HORIZONTAL_FILL);

		Button *button_dec = react_u32_button_create(&row->element, 0, "count", "-", 0, 10, -1);
		Label *label_count = react_u32_label_create(&row->element, ELEMENT_HORIZONTAL_FILL | LABEL_CENTER, "count", "Count: ");
		Button *button_inc = react_u32_button_create(&row->element, 0, "count", "+", 0, 10, 1);
	}

	react_update(NULL, NULL);
	element_message(container, MSG_LAYOUT, 0, 0);
	element_repaint(container, NULL);
}

#ifdef PLATFORM_WIN32
int WinMain(HINSTANCE instance, HINSTANCE previousInstance, LPSTR commandLine, int showCommand) {
#else
int main() {
#endif
	// Create the counter object.
	Object object_counter = {0};
	object_counter.kind = OBJECT_COUNTER;

	// Add a U32 property "count" with the value 10.
	Property prop_count = {0};
	prop_count.kind = PROPERTY_U32;
	strcpy(prop_count.key, "count");
	prop_count.u32 = 0; 
	arrput(object_counter.properties, prop_count);

	// Put the counter object into the objects map, and select it.
	object_id_allocator++;
	hmput(objects, object_id_allocator, object_counter);
	selected_object_id = object_id_allocator;


	platform_init();
	Window *window = platform_create_window("Example", 400, 200);
	Panel *panel = panel_create(&window->element, PANEL_GREY);
	panel->padding = rect_make(10, 10, 10, 10);
	panel->gap = 15;

	// Controls
	Panel *row = panel_create(&panel->element, PANEL_HORIZONTAL | ELEMENT_HORIZONTAL_FILL);
	Label *label_controls = label_create(&row->element, 0, "Controls: ", -1);
	Button *button_undo = button_create(&row->element, 0, "Undo", -1);
	Button *button_redo = button_create(&row->element, 0, "Redo", -1);
	button_undo->element.message_user = button_undo_message;
	button_redo->element.message_user = button_redo_message;
	Button *button_save = button_create(&row->element, 0, "Save", -1);
	Button *button_load = button_create(&row->element, 0, "Load", -1);
	button_save->element.message_user = button_save_message;
	button_load->element.message_user = button_load_message;

	container = &panel_create(&panel->element, ELEMENT_HORIZONTAL_FILL | ELEMENT_VERTICAL_FILL)->element;

	populate();

	return platform_message_loop();
}
