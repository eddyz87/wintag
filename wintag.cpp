/* TODO:
 *      per screen context
 *      root window change event handling (regrab keys)
 *      systray icon
 */

#include <map>
#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>

Atom active_window_atom(Display *display) {
    return XInternAtom(display, "_NET_ACTIVE_WINDOW", True);
}

struct XBuffer {
    unsigned char* mem;
    int format;
    unsigned long items;

    XBuffer(): mem(0), format(0), items(0) {}

    ~XBuffer() {
        if (mem) { XFree(mem); }
    }
};

struct XDisplay {
    Display *display;
    XDisplay(const char *name) {
        display = XOpenDisplay(name);
    }
    ~XDisplay() {
        XCloseDisplay(display);
    }
    operator Display* () {
        return display;
    }
};

int get_window_property(Display *display, Window window, Atom property, Atom type, int format, unsigned size, XBuffer& buffer/* result */) {
    Atom ret_type;
    unsigned long bytes_after;

    int r = XGetWindowProperty(display, window, property, 0, size, False, XA_WINDOW,
                               &ret_type, &buffer.format, &buffer.items, &bytes_after, &buffer.mem);

    if (r != Success)            { printf("XGetWindowProperty failed"); return 1; }
    if (ret_type != type)        { printf("XGetWindowProperty: ret type mismatch: %lu\n", ret_type); return 1; }
    if (format != buffer.format) { printf("XGetWindowProperty: format mismatch: %d\n", format); return 1; }
    if (buffer.items < size)     { printf("XGetWindowProperty: bad item size: %d\n", format); return 1; }

    return 0;
}

Window get_active_window(Display *display, Window root) {
    XBuffer result;
    if (get_window_property(display, root, active_window_atom(display), XA_WINDOW, 32, 1, result) != 0) { return 0; }
    
    return *(Window*) result.mem;
}

typedef std::map<unsigned int, Window> KeyToWindowMap;

void map_hotkeys(Display *display, Window root, KeyToWindowMap& map, unsigned int modifiers, const char* keys[]) {
    for (int i = 0; keys[i] != 0; ++i) {
        KeyCode key = XKeysymToKeycode(display, XStringToKeysym(keys[i]));
        XGrabKey(display, key, modifiers, root, True, GrabModeSync, GrabModeSync);
        map[key] = 0;
    }
}

void activate_window(Display *display, Window root, Window active_window) {
    XEvent ev;
    ev.type = ClientMessage;
    ev.xclient.display = display;
    ev.xclient.window = active_window;
    ev.xclient.message_type = active_window_atom(display);
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = 2; // pager message source
    ev.xclient.data.l[1] = CurrentTime;
    XSendEvent(display, root, False, SubstructureNotifyMask | SubstructureRedirectMask, &ev);
}

#define AltKeyMask Mod1Mask
#define WinKeyMask Mod4Mask

#define SelectKeyMask   WinKeyMask
#define SetKeyMask      (WinKeyMask | AltKeyMask)

int main() {
    XDisplay display = XDisplay("");
    Window root = XDefaultRootWindow(display);

    KeyToWindowMap hotkeys;
    const char* keys[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9", 0 };
    map_hotkeys(display, root, hotkeys, SelectKeyMask, keys);
    map_hotkeys(display, root, hotkeys, SetKeyMask, keys);
    
    XEvent ev;
    for(;;) {
        XNextEvent(display, &ev);
        if (ev.type == KeyPress || ev.type == KeyRelease) {
            //printf("Key event: code=%p mod=%p set=%p select=%p\n", ev.xkey.keycode, ev.xkey.state, SetKeyMask, SelectKeyMask);
            KeyToWindowMap::iterator it = hotkeys.find(ev.xkey.keycode);
            if (it != hotkeys.end() && ((ev.xkey.state == SetKeyMask) || (ev.xkey.state == SelectKeyMask))) {
                // do job
                if(ev.type == KeyPress) {
                    if (ev.xkey.state == SelectKeyMask) {
                        if (it->second != 0) {
                            printf("Bringing up active window: %lu\n", it->second);
                            activate_window(display, root, it->second);
                        } else {
                            printf("No active window\n");
                        }
                    } else if (ev.xkey.state == SetKeyMask) {
                        it->second = get_active_window(display, root);
                        printf("Remembering active window: %lu\n", it->second);
                    }
                }
                XAllowEvents(display, AsyncKeyboard, ev.xkey.time);
            } else {
                //let key go
                XAllowEvents(display, ReplayKeyboard, ev.xkey.time);
            }
            XFlush(display);
        }
    }

    return 0;
}
