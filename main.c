#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <stdlib.h>
#include <stdbool.h>

static Atom NET_ACTIVE_WINDOW;
static Atom NET_WM_STATE;
static Atom NET_WM_STATE_FULLSCREEN;

static Window get_active_window(Display *dpy, Window root) {
    Window aw = None;
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop = NULL;

    if (XGetWindowProperty(dpy, root, NET_ACTIVE_WINDOW, 0, (~0L),
                           False, AnyPropertyType, &actual_type, &actual_format,
                           &nitems, &bytes_after, &prop) == Success && prop) {
        if (nitems >= 1) {
            aw = *(Window *)prop;
        }
        XFree(prop);
    }
    return aw;
}

static bool is_window_fullscreen(Display *dpy, Window w) {
    if (w == None) return false;
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop = NULL;
    bool fs = false;

    if (XGetWindowProperty(dpy, w, NET_WM_STATE, 0, (~0L),
                           False, XA_ATOM, &actual_type, &actual_format,
                           &nitems, &bytes_after, &prop) != Success) {
        return false;
    }
    if (prop && actual_type == XA_ATOM && actual_format == 32) {
        Atom *atoms = (Atom *)prop;
        for (unsigned long i = 0; i < nitems; i++) {
            if (atoms[i] == NET_WM_STATE_FULLSCREEN) {
                fs = true;
                break;
            }
        }
    }
    if (prop) XFree(prop);
    return fs;
}

static void select_on_window(Display *dpy, Window w) {
    if (w == None) return;
    long mask = PropertyChangeMask | StructureNotifyMask;
    XSelectInput(dpy, w, mask);
}

static void polybar_toggle(bool fs) {
    static bool hidden = false;
    if(fs){
        hidden = true;
        system("polybar-msg cmd hide 1>/dev/null");
    } else if (!fs && hidden){
        hidden = false;
        system("polybar-msg cmd show 1>/dev/null");
    }
}

int main(void) {
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) {
        return 1;
    }

    Window root = DefaultRootWindow(dpy);

    NET_ACTIVE_WINDOW       = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
    NET_WM_STATE            = XInternAtom(dpy, "_NET_WM_STATE", False);
    NET_WM_STATE_FULLSCREEN = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);

    XSelectInput(dpy, root, PropertyChangeMask);

    Window active = get_active_window(dpy, root);
    if (active != None) select_on_window(dpy, active);
    bool fs = is_window_fullscreen(dpy, active);
    polybar_toggle(fs);

    for (;;) {
        XEvent ev;
        XNextEvent(dpy, &ev);

        if (ev.xany.type == PropertyNotify) {
            XPropertyEvent *pe = &ev.xproperty;

            if (pe->window == root && pe->atom == NET_ACTIVE_WINDOW) {
                Window new_active = get_active_window(dpy, root);
                if (new_active != active) {
                    active = new_active;
                    if (active != None) select_on_window(dpy, active);
                    fs = is_window_fullscreen(dpy, active);
                    polybar_toggle(fs);
                }
            }

            if (pe->window == active && pe->atom == NET_WM_STATE) {
                bool new_fs = is_window_fullscreen(dpy, active);
                if (new_fs != fs) {
                    fs = new_fs;
                    polybar_toggle(fs);
                }
            }
        } else if (ev.xany.type == DestroyNotify) {
            XDestroyWindowEvent *de = (XDestroyWindowEvent *)&ev;
            if (de->window == active) {
                active = get_active_window(dpy, root);
                if (active != None) select_on_window(dpy, active);
                fs = is_window_fullscreen(dpy, active);
                polybar_toggle(fs);
            }
        } else if (ev.xany.type == UnmapNotify) {
            XUnmapEvent *ue = (XUnmapEvent *)&ev;
            if (ue->window == active) {
                Window maybe_new = get_active_window(dpy, root);
                if (maybe_new != active) {
                    active = maybe_new;
                    if (active != None) select_on_window(dpy, active);
                    fs = is_window_fullscreen(dpy, active);
                    polybar_toggle(fs);
                }
            }
        }
    }

    XCloseDisplay(dpy);
    return 0;
}

