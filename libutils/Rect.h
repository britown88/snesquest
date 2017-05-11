#pragma once
#include "Defs.h"
#include "Vector.h"


typedef struct {
   int x, y, w, h;
} Recti;

static void rectiOffset(Recti *r, int x, int y) {
   r->x += x;
   r->y += y;
}

static boolean rectiContains(Recti r, Int2 p) {
   if (p.x < r.x || 
      p.y < r.y || 
      p.x >= r.x+r.w || 
      p.y >= r.y+r.h) return false;
   return true;
}

static boolean rectiIntersects(Recti a, Recti b){
   if (a.x >= b.x + b.w ||
      a.y >= b.y+b.h ||
      b.x >= a.x+a.w ||
      b.y >= a.y+a.h) return false;
   return true;
}