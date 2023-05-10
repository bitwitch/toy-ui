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
	}
	return 0;
}

Label *label_create(Element *parent, uint32_t flags, char *text, int text_bytes) {
	Label *label = (Label*)element_create(sizeof(Label), parent, flags, label_message);
	string_copy(&label->text, &label->text_bytes, text, text_bytes);
	return label;
}

void label_set_text(Label *label, char *text, int text_bytes) {
	string_copy(&label->text, &label->text_bytes, text, text_bytes);
}

//////////////////////////////////////////////////////////////////////////////
// Drawing helpers
//////////////////////////////////////////////////////////////////////////////
#define GLYPH_WIDTH  (9)
#define GLYPH_HEIGHT (16)

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

