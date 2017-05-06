#pragma once
#include "Defs.h"
#include "Vector.h"


typedef struct {
   int left, top, right, bottom;
} Recti;

static Recti rectiCreate(int x, int y, int width, int height) {
   Recti out;
   out.left = x;
   out.top = y;
   out.right = x + width;
   out.bottom = y + height;
   return out;
}

static int rectiWidth(Recti *r) {
   return r->right - r->left;
}

static int rectiHeight(Recti *r) {
   return r->bottom - r->top;
}

static void rectiOffset(Recti *r, int x, int y) {
   r->left += x;
   r->right += x;
   r->top += y;
   r->bottom += y;
}

static boolean rectiContains(Recti r, Int2 p) {
   if (p.x < r.left || 
      p.y < r.top || 
      p.x >= r.right || 
      p.y >= r.bottom) return false;
   return true;
}

static boolean rectiIntersects(Recti a, Recti b){
   if (a.left >= b.right ||
      a.top >= b.bottom ||
      b.left >= a.right ||
      b.top >= a.bottom) return false;
   return true;
}