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
