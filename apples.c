#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_icccm.h>
#include <xcb/screensaver.h>

#define SHELL "/bin/sh"

xcb_connection_t* dis;
xcb_window_t root;
xcb_ewmh_connection_t* ewmh;
char screensaverFirstEvent;
char screensaverDisabled = 0;

int catchErrorSilent(xcb_void_cookie_t cookie) {
    xcb_generic_error_t* e = xcb_request_check(dis, cookie);
    int errorCode = 0;
    if(e) {
        errorCode = e->error_code;
        free(e);
    }
    return errorCode;
}
int registerForWindowEvents(xcb_window_t window, int mask) {
    xcb_void_cookie_t cookie;
    cookie = xcb_change_window_attributes_checked(dis, window, XCB_CW_EVENT_MASK, &mask);
    return catchErrorSilent(cookie);
}
void initConnection() {
    dis = xcb_connect(NULL, NULL);
    ewmh = (xcb_ewmh_connection_t*)malloc(sizeof(xcb_ewmh_connection_t));
    xcb_intern_atom_cookie_t* cookie = xcb_ewmh_init_atoms(dis, ewmh);
    xcb_ewmh_init_atoms_replies(ewmh, cookie, NULL);
    root = ewmh->screens[0]->root;
    registerForWindowEvents(root, XCB_EVENT_MASK_PROPERTY_CHANGE);
    catchErrorSilent(xcb_screensaver_select_input_checked(dis, root, XCB_SCREENSAVER_EVENT_NOTIFY_MASK | XCB_SCREENSAVER_EVENT_CYCLE_MASK));
    screensaverFirstEvent = xcb_get_extension_data(dis, &xcb_screensaver_id)->first_event;
}

int checkActiveWindow() {
    xcb_ewmh_get_atoms_reply_t reply;
    xcb_window_t activeWindow;
    int hasActiveWindow = xcb_ewmh_get_active_window_reply(ewmh, xcb_ewmh_get_active_window(ewmh, 0), &activeWindow, NULL);
    if(!hasActiveWindow )
        return 0;
    int hasState = xcb_ewmh_get_wm_state_reply(ewmh, xcb_ewmh_get_wm_state(ewmh, activeWindow), &reply, NULL);
    if(hasState) {
        for(uint32_t i=0;i<reply.atoms_len;i++) {
            if(reply.atoms[i] == ewmh->_NET_WM_STATE_FULLSCREEN) {
                if(!screensaverDisabled) {
                    xcb_screensaver_suspend_checked (dis, 1);
                    screensaverDisabled = 1;
                }
                return 1;
            }
        }
        xcb_ewmh_get_atoms_reply_wipe(&reply);
    }

    if(screensaverDisabled) {
        xcb_screensaver_suspend_checked (dis, 0);
        screensaverDisabled = 0;
    }
    return 0;
}

void spawnCmd(const char* cmd) {
    pid_t pid = fork();
    if(!pid) {
        execl(SHELL, SHELL, "-c", cmd, NULL);
    }
    waitpid(pid, NULL, -1);
}
int main(int argc, const char* const argv[]) {
    initConnection();
    if(argc == 1)
        exit(2);
    int cycled = 0;
    const char* const screensaverCmd = argv[1];
    const char* const cycleCmd = argv[2];

    checkActiveWindow();
    xcb_generic_event_t* event;
    while((event = xcb_wait_for_event(dis))) {
        xcb_screensaver_notify_event_t* screensaverEvent;
        if(event->response_type == XCB_PROPERTY_NOTIFY) {
            if(((xcb_property_notify_event_t*)event)->atom == ewmh->_NET_ACTIVE_WINDOW) {
                checkActiveWindow();
            }
        } else if(event->response_type == screensaverFirstEvent + XCB_SCREENSAVER_NOTIFY){
            screensaverEvent = (xcb_screensaver_notify_event_t*)event;
            if(screensaverEvent->state == XCB_SCREENSAVER_STATE_ON) {
                spawnCmd(screensaverCmd);
            }
            else if(screensaverEvent->state == XCB_SCREENSAVER_STATE_CYCLE) {
                if(!cycled && cycleCmd )
                    spawnCmd(cycleCmd );
                cycled = 1;
            }
            else {
                cycled = 0;
            }
        }
        xcb_flush(dis);
        free(event);
    }
}