#ifdef PLATFORM_WIN32

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
	} else {
		return DefWindowProc(hwnd, message, wParam, lParam);
	}

	return 0;
}

Window *platform_create_window(const char *title, int width, int height) {
	Window *window = (Window *) element_create(sizeof(Window), NULL, 0);

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

Window *find_window(X11Window window) {
	for (uintptr_t i = 0; i < global_state.window_count; i++) {
		if (global_state.windows[i]->window == window) {
			return global_state.windows[i];
		}
	}
	return NULL;
}

Window *platform_create_window(const char *title, int width, int height) {
	Window *window = (Window *) element_create(sizeof(Window), NULL, 0);
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
	while (true) {
		XEvent event;
		XNextEvent(global_state.display, &event);

		if (event.type == ClientMessage && (Atom) event.xclient.data.l[0] == global_state.windowClosedID) {
			return 0;
		} else if (event.type == ConfigureNotify) {
			Window *window = find_window(event.xconfigure.window);
			if (!window) continue;

			if (window->width != event.xconfigure.width || window->height != event.xconfigure.height) {
				window->width = event.xconfigure.width;
				window->height = event.xconfigure.height;
				window->image->width = window->width;
				window->image->height = window->height;
				window->image->bytes_per_line = window->width * 4;
				window->image->data = (char *) window->bits;
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
