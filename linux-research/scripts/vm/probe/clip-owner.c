// Print the CLIPBOARD owner window and the name/class of the nearest ancestor that has one.
// Build: gcc -O1 -o clip-owner clip-owner.c -lX11
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
int main(void)
{
  Display* d = XOpenDisplay(NULL);
  if (!d) return 1;
  Window w = XGetSelectionOwner(d, XInternAtom(d, "CLIPBOARD", False));
  printf("owner 0x%lx", w);
  for (Window cur = w; cur; )
  {
    XClassHint ch = {0}; char* name = NULL;
    if (XGetClassHint(d, cur, &ch) || XFetchName(d, cur, &name))
    {
      printf(" class='%s' name='%s' (on 0x%lx)", ch.res_class ? ch.res_class : "", name ? name : "", cur);
      break;
    }
    Window root, parent, *kids; unsigned n;
    if (!XQueryTree(d, cur, &root, &parent, &kids, &n)) break;
    if (kids) XFree(kids);
    cur = parent == root ? 0 : parent;
  }
  printf("\n");
  return 0;
}
