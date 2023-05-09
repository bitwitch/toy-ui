typedef struct {
	int l, r, t, b;
} Rect;

Rect rect_make(int l, int r, int t, int b);

// Returns true if the rectangle has a positive width and height.
bool rect_valid(Rect a);

// Returns the intersection of the rectangles. If the rectangles don't overlap,
// an invalid rectangle is returned (as per rect_valid)
Rect rect_intersection(Rect a, Rect b);

// Returns the smallest rectangle containing both of the input rectangles.
Rect rect_bounding(Rect a, Rect b);

// Returns true if all sides are equal.
bool rect_equals(Rect a, Rect b);

// Returns true if the pixel with its top-left at the given coordinate is
// contained inside the rectangle.
bool rect_contains(Rect a, int x, int y);
