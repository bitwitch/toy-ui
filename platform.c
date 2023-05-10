void ui_update(void);
void ui_window_input_event(Window *window, Message message, int data_int, void *data_ptr);
void ui_window_set_pressed(Window *window, Element *element, MouseButton button);

int platform_window_message(Element *element, Message message, int data_int, void *data_ptr) {
	(void) data_int;
	(void) data_ptr;
	if (message == MSG_LAYOUT && element->child_count > 0) {
		element_move(element->children[0], element->bounds, false);
		element_repaint(element, NULL);
	}
	return 0;
}
void platform_window_end_paint(Window *window, Painter *painter);

#ifdef PLATFORM_WIN32

void platform_window_end_paint(Window *window, Painter *painter) {
	(void)painter;
	HDC dc = GetDC(window->hwnd);
	BITMAPINFOHEADER info = { 0 };
	info.biSize = sizeof(info);
	info.biWidth = window->width; 
	info.biHeight = window->height;
	info.biPlanes = 1;
	info.biBitCount = 32;
	StretchDIBits(dc, 
		window->update_region.l, window->update_region.t, 
		window->update_region.r - window->update_region.l, window->update_region.b - window->update_region.t,
		window->update_region.l, window->update_region.b + 1, 
		window->update_region.r - window->update_region.l, window->update_region.t - window->update_region.b,
		window->bits, (BITMAPINFO *) &info, DIB_RGB_COLORS, SRCCOPY);
	ReleaseDC(window->hwnd, dc);
}

LRESULT CALLBACK win32_window_proc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
	Window *window = (Window *) GetWindowLongPtr(hwnd, GWLP_USERDATA);

	if (!window) {
		return DefWindowProc(hwnd, message, wParam, lParam);
	}

	if (message == WM_CLOSE) {
		PostQuitMessage(0);
	} else if (message == WM_SIZE) {
		RECT client;
		GetClientRect(hwnd, &client);
		window->width = client.right;
		window->height = client.bottom;
		window->bits = (uint32_t*)realloc(window->bits, window->width * window->height * 4);
		window->element.bounds = rect_make(0, window->width, 0, window->height);
		window->element.clip = rect_make(0, window->width, 0, window->height);
		element_message(&window->element, MSG_LAYOUT, 0, 0);
		ui_update();
	} else if (message == WM_MOUSEMOVE) {
		if (!window->tracking_leave) {
			window->tracking_leave = true;
			TRACKMOUSEEVENT leave = { 0 };
			leave.cbSize = sizeof(TRACKMOUSEEVENT);
			leave.dwFlags = TME_LEAVE;
			leave.hwndTrack = hwnd;
			TrackMouseEvent(&leave);
		}
		POINT cursor;
		GetCursorPos(&cursor);
		ScreenToClient(hwnd, &cursor);
		window->mouse_x = cursor.x;
		window->mouse_y = cursor.y;
		ui_window_input_event(window, MSG_MOUSE_MOVE, 0, 0);
	} else if (message == WM_MOUSELEAVE) {
		window->tracking_leave = false;
		if (!window->pressed) {
			window->mouse_x = -1;
			window->mouse_y = -1;
		}
		ui_window_input_event(window, MSG_MOUSE_MOVE, 0, 0);
	} else if (message == WM_LBUTTONDOWN) {
		SetCapture(hwnd);
		ui_window_input_event(window, MSG_MOUSE_LEFT_DOWN, 0, 0);
	} else if (message == WM_LBUTTONUP) {
		if (window->pressed_mouse_button == MOUSE_BUTTON_LEFT) 
			ReleaseCapture();
		ui_window_input_event(window, MSG_MOUSE_LEFT_UP, 0, 0);
	} else if (message == WM_MBUTTONDOWN) {
		SetCapture(hwnd);
		ui_window_input_event(window, MSG_MOUSE_MIDDLE_DOWN, 0, 0);
	} else if (message == WM_MBUTTONUP) {
		if (window->pressed_mouse_button == MOUSE_BUTTON_MIDDLE) 
			ReleaseCapture();
		ui_window_input_event(window, MSG_MOUSE_MIDDLE_UP, 0, 0);
	} else if (message == WM_RBUTTONDOWN) {
		SetCapture(hwnd);
		ui_window_input_event(window, MSG_MOUSE_RIGHT_DOWN, 0, 0);
	} else if (message == WM_RBUTTONUP) {
		if (window->pressed_mouse_button == MOUSE_BUTTON_RIGHT) 
			ReleaseCapture();
		ui_window_input_event(window, MSG_MOUSE_RIGHT_UP, 0, 0);
	} else if (message == WM_PAINT) {
		PAINTSTRUCT paint;
		HDC dc = BeginPaint(hwnd, &paint);
		BITMAPINFOHEADER info = { 0 };
		info.biSize = sizeof(info);
		info.biWidth = window->width;
		info.biHeight = -window->height;
		info.biPlanes = 1;
		info.biBitCount = 32;
		StretchDIBits(dc, 0, 0, 
			window->element.bounds.r - window->element.bounds.l, 
			window->element.bounds.b - window->element.bounds.t, 
			0, 0, window->element.bounds.r - window->element.bounds.l, 
			window->element.bounds.b - window->element.bounds.t,
			window->bits, (BITMAPINFO*)&info, DIB_RGB_COLORS, SRCCOPY);
		EndPaint(hwnd, &paint);
	} else {
		return DefWindowProc(hwnd, message, wParam, lParam);
	}

	return 0;
}

Window *platform_create_window(const char *title, int width, int height) {
	Window *window = (Window *) element_create(sizeof(Window), NULL, 0, platform_window_message);
	window->element.window = window;
	window->hovered = &window->element;

	global_state.window_count++;
	global_state.windows = realloc(global_state.windows, sizeof(Window*) * global_state.window_count);
	global_state.windows[global_state.window_count - 1] = window;

	window->hwnd = CreateWindow("UILibraryTutorial", title, WS_OVERLAPPEDWINDOW, 
			CW_USEDEFAULT, CW_USEDEFAULT, width, height, NULL, NULL, NULL, NULL);
	SetWindowLongPtr(window->hwnd, GWLP_USERDATA, (LONG_PTR) window);
	ShowWindow(window->hwnd, SW_SHOW);
	PostMessage(window->hwnd, WM_SIZE, 0, 0);
	return window;
}

int platform_message_loop() {
	MSG message = {0};

	while (GetMessage(&message, NULL, 0, 0)) {
		TranslateMessage(&message);
		DispatchMessage(&message);
	}

	return (int)message.wParam;
}

void platform_init() {
	WNDCLASS window_class = { 0 };
	window_class.lpfnWndProc = win32_window_proc;
	window_class.hCursor = LoadCursor(NULL, IDC_ARROW);
	window_class.lpszClassName = "UILibraryTutorial";
	RegisterClass(&window_class);
}

#endif

#ifdef PLATFORM_LINUX

void platform_window_end_paint(Window *window, Painter *painter) {
	(void) painter;
	XPutImage(global_state.display, window->window, DefaultGC(global_state.display, 0), window->image, 
		window->update_region.l, window->update_region.t, window->update_region.l, window->update_region.t,
		window->update_region.r - window->update_region.l, window->update_region.b - window->update_region.t);
}

Window *find_window(X11Window window) {
	for (uintptr_t i = 0; i < global_state.window_count; i++) {
		if (global_state.windows[i]->window == window) {
			return global_state.windows[i];
		}
	}
	return NULL;
}

Window *platform_create_window(const char *title, int width, int height) {
	Window *window = (Window *) element_create(sizeof(Window), NULL, 0, platform_window_message);
	window->element.window = window;
	window->hovered = &window->element;

	global_state.window_count++;
	global_state.windows = realloc(global_state.windows, sizeof(Window *) * global_state.window_count);
	global_state.windows[global_state.window_count - 1] = window;

	XSetWindowAttributes attributes = {};
	window->window = XCreateWindow(global_state.display, DefaultRootWindow(global_state.display), 
		0, 0, width, height, 
		0, 0, InputOutput, CopyFromParent, CWOverrideRedirect, &attributes);
	XStoreName(global_state.display, window->window, title);
	XSelectInput(global_state.display, window->window, 
		SubstructureNotifyMask | ExposureMask | PointerMotionMask | ButtonPressMask 
		| ButtonReleaseMask | KeyPressMask | KeyReleaseMask | StructureNotifyMask
		| EnterWindowMask | LeaveWindowMask | ButtonMotionMask | KeymapStateMask 
		| FocusChangeMask | PropertyChangeMask);
	XMapRaised(global_state.display, window->window);
	XSetWMProtocols(global_state.display, window->window, &global_state.windowClosedID, 1);
	window->image = XCreateImage(global_state.display, global_state.visual, 24, ZPixmap, 0, NULL, 10, 10, 32, 0);
	return window;
}

int platform_message_loop() {
	ui_update();
	while (true) {
		XEvent event;
		XNextEvent(global_state.display, &event);

		if (event.type == ClientMessage && (Atom) event.xclient.data.l[0] == global_state.windowClosedID) {
			return 0;
		} else if (event.type == Expose) {
			Window *window = find_window(event.xexpose.window);
			if (!window) continue;
			XPutImage(global_state.display, window->window, DefaultGC(global_state.display, 0), 
					window->image, 0, 0, 0, 0, window->width, window->height);
		} else if (event.type == ConfigureNotify) {
			Window *window = find_window(event.xconfigure.window);
			if (!window) continue;

			if (window->width != event.xconfigure.width || window->height != event.xconfigure.height) {
				window->width = event.xconfigure.width;
				window->height = event.xconfigure.height;
				window->bits = (uint32_t*)realloc(window->bits, window->width * window->height * 4);
				window->image->width = window->width;
				window->image->height = window->height;
				window->image->bytes_per_line = window->width * 4;
				window->image->data = (char *) window->bits;
				window->element.bounds = rect_make(0, window->width, 0, window->height);
				window->element.clip = rect_make(0, window->width, 0, window->height);
				element_message(&window->element, MSG_LAYOUT, 0, 0);
				ui_update();
			}
		} else if (event.type == MotionNotify) {
			Window *window = find_window(event.xmotion.window);
			if (!window) continue;
			window->mouse_x = event.xmotion.x;
			window->mouse_y = event.xmotion.y;
			ui_window_input_event(window, MSG_MOUSE_MOVE, 0, 0);
		} else if (event.type == LeaveNotify) {
			Window *window = find_window(event.xcrossing.window);
			if (!window) continue;

			if (!window->pressed) {
				window->mouse_x = -1;
				window->mouse_y = -1;
			}

			ui_window_input_event(window, MSG_MOUSE_MOVE, 0, 0);
		} else if (event.type == ButtonPress || event.type == ButtonRelease) {
			Window *window = find_window(event.xbutton.window);
			if (!window) continue;
			window->mouse_x = event.xbutton.x;
			window->mouse_y = event.xbutton.y;
			if (event.xbutton.button >= 1 && event.xbutton.button <= 3) {
				ui_window_input_event(window, 
					(Message)((event.type == ButtonPress ? MSG_MOUSE_LEFT_DOWN : MSG_MOUSE_LEFT_UP) 
					+ event.xbutton.button * 2 - 2), 0, 0);
			}
		}
	}
}

void platform_init() {
	global_state.display = XOpenDisplay(NULL);
	global_state.visual = XDefaultVisual(global_state.display, 0);
	global_state.windowClosedID = XInternAtom(global_state.display, "WM_DELETE_WINDOW", 0);
}

#endif
