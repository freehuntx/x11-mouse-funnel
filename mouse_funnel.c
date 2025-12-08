/*
 * Compile with: gcc -O2 -o mouse_warp mouse_warp.c -lX11 -lXi -lXrandr
 *
 * Feature: 
 * - Zero-lag mouse warping using XInput2 RawMotion.
 * - Allows passing through "dead walls" between screens of different sizes.
 * - Proportional Mapping: Entering from the bottom of a small screen 
 *   puts you at the bottom of the big screen (relative positioning).
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/extensions/XInput2.h>
#include <X11/extensions/Xrandr.h>

#define EDGE_THRESHOLD 2 // Pixels from edge to trigger warp

typedef struct {
  int x, y, w, h;
  int index;
} Monitor;

Monitor *monitors = NULL;
int monitor_count = 0;
Display *dpy;
Window root;
int rr_event_base, rr_error_base;

// Update monitor layout on startup or config change
void update_monitors() {
  XRRScreenResources *res = XRRGetScreenResourcesCurrent(dpy, root);
  if (!res) return;

  if (monitors) free(monitors);
  monitor_count = 0;
  
  // First pass to count valid crtcs
  for (int i = 0; i < res->noutput; i++) {
    XRROutputInfo *info = XRRGetOutputInfo(dpy, res, res->outputs[i]);
    if (info->crtc && info->connection == RR_Connected) monitor_count++;
    XRRFreeOutputInfo(info);
  }

  monitors = malloc(sizeof(Monitor) * monitor_count);
  int valid_idx = 0;

  for (int i = 0; i < res->noutput; i++) {
    XRROutputInfo *info = XRRGetOutputInfo(dpy, res, res->outputs[i]);
    if (info->crtc && info->connection == RR_Connected) {
      XRRCrtcInfo *crtc = XRRGetCrtcInfo(dpy, res, info->crtc);
      monitors[valid_idx].x = crtc->x;
      monitors[valid_idx].y = crtc->y;
      monitors[valid_idx].w = crtc->width;
      monitors[valid_idx].h = crtc->height;
      monitors[valid_idx].index = valid_idx;
      valid_idx++;
      XRRFreeCrtcInfo(crtc);
    }
    XRRFreeOutputInfo(info);
  }
  XRRFreeScreenResources(res);
  printf("Layout updated: %d monitors active.\n", monitor_count);
}

Monitor* get_current_monitor(int x, int y) {
  for (int i = 0; i < monitor_count; i++) {
    if (x >= monitors[i].x && x < monitors[i].x + monitors[i].w &&
      y >= monitors[i].y && y < monitors[i].y + monitors[i].h) {
      return &monitors[i];
    }
  }
  return NULL;
}

Monitor* get_target_monitor(Monitor *current, int dx, int dy) {
  Monitor *best = NULL;
  int best_dist = 2147483647;
  int cx = current->x + current->w / 2;
  int cy = current->y + current->h / 2;

  for (int i = 0; i < monitor_count; i++) {
    if (&monitors[i] == current) continue;
    
    Monitor *m = &monitors[i];
    int is_candidate = 0;

    // Determine if monitor is in the direction of push
    if (dx < 0 && m->x + m->w <= current->x) is_candidate = 1; // Left
    else if (dx > 0 && m->x >= current->x + current->w) is_candidate = 1; // Right
    else if (dy < 0 && m->y + m->h <= current->y) is_candidate = 1; // Up
    else if (dy > 0 && m->y >= current->y + current->h) is_candidate = 1; // Down

    if (is_candidate) {
      int mx = m->x + m->w / 2;
      int my = m->y + m->h / 2;
      int dist = (cx - mx)*(cx - mx) + (cy - my)*(cy - my);
      if (dist < best_dist) {
        best_dist = dist;
        best = m;
      }
    }
  }
  return best;
}

int main(int argc, char **argv) {
  dpy = XOpenDisplay(NULL);
  if (!dpy) return 1;
  root = DefaultRootWindow(dpy);

  int xi_opcode, event, error;
  if (!XQueryExtension(dpy, "XInputExtension", &xi_opcode, &event, &error)) {
    fprintf(stderr, "XInput not available.\n");
    return 1;
  }

  if (!XRRQueryExtension(dpy, &rr_event_base, &rr_error_base)) {
    fprintf(stderr, "RandR not available.\n");
    return 1;
  }

  // Select XInput2 RawMotion
  XIEventMask mask;
  mask.deviceid = XIAllMasterDevices;
  mask.mask_len = XIMaskLen(XI_RawMotion);
  unsigned char mask_bits[XIMaskLen(XI_RawMotion)] = {0};
  XISetMask(mask_bits, XI_RawMotion);
  mask.mask = mask_bits;
  XISelectEvents(dpy, root, &mask, 1);

  // Select RandR Screen Change Notify
  XRRSelectInput(dpy, root, RRScreenChangeNotifyMask);
  update_monitors();

  XEvent ev;
  while (1) {
    XNextEvent(dpy, &ev);

    // Handle Screen Layout Changes
    if (ev.type == rr_event_base + RRScreenChangeNotify) {
      XRRUpdateConfiguration(&ev);
      update_monitors();
      continue;
    }

    // Handle Mouse Motion
    if (ev.xcookie.type == GenericEvent && ev.xcookie.extension == xi_opcode && XGetEventData(dpy, &ev.xcookie)) {
      if (ev.xcookie.evtype == XI_RawMotion) {
        XIDeviceEvent *d_ev = (XIDeviceEvent*)ev.xcookie.data;
        XIRawEvent *raw = (XIRawEvent*)d_ev;
        
        double dx = 0, dy = 0;
        if (XIMaskIsSet(raw->valuators.mask, 0)) dx = raw->raw_values[0];
        if (XIMaskIsSet(raw->valuators.mask, 1)) dy = raw->raw_values[1];

        if (dx != 0 || dy != 0) {
          Window root_ret, child_ret;
          int root_x, root_y, win_x, win_y;
          unsigned int mask_ret;
          XQueryPointer(dpy, root, &root_ret, &child_ret, &root_x, &root_y, &win_x, &win_y, &mask_ret);

          Monitor *curr = get_current_monitor(root_x, root_y);
          if (curr) {
            Monitor *target = NULL;
            int new_x = root_x, new_y = root_y;
            int warp = 0;

            // Calculate relative positions (0.0 to 1.0)
            double x_ratio = (double)(root_x - curr->x) / curr->w;
            double y_ratio = (double)(root_y - curr->y) / curr->h;

            // Check bounds and determine target
            if (dx < -0.1 && root_x <= curr->x + EDGE_THRESHOLD) { // Left
              target = get_target_monitor(curr, -1, 0);
              if (target) {
                new_x = target->x + target->w - 2;
                new_y = target->y + (int)(y_ratio * target->h);
                warp = 1;
              }
            } else if (dx > 0.1 && root_x >= curr->x + curr->w - 1 - EDGE_THRESHOLD) { // Right
              target = get_target_monitor(curr, 1, 0);
              if (target) {
                new_x = target->x + 1;
                new_y = target->y + (int)(y_ratio * target->h);
                warp = 1;
              }
            } else if (dy < -0.1 && root_y <= curr->y + EDGE_THRESHOLD) { // Up
              target = get_target_monitor(curr, 0, -1);
              if (target) {
                new_y = target->y + target->h - 2;
                new_x = target->x + (int)(x_ratio * target->w);
                warp = 1;
              }
            } else if (dy > 0.1 && root_y >= curr->y + curr->h - 1 - EDGE_THRESHOLD) { // Down
              target = get_target_monitor(curr, 0, 1);
              if (target) {
                new_y = target->y + 1;
                new_x = target->x + (int)(x_ratio * target->w);
                warp = 1;
              }
            }

            if (warp && target) {
              // Safety clamp
              if (new_y >= target->y + target->h) new_y = target->y + target->h - 1;
              if (new_x >= target->x + target->w) new_x = target->x + target->w - 1;
              
              XWarpPointer(dpy, None, root, 0, 0, 0, 0, new_x, new_y);
              XFlush(dpy);
            }
          }
        }
      }
      XFreeEventData(dpy, &ev.xcookie);
    }
  }
  return 0;
}
