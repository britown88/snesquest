 #include "Math.h"

int int2Dot(Int2 v1, Int2 v2) {
   return v1.x * v2.x + v1.y * v2.y;
}

Int2 int2Perp(Int2 v) {
   return (Int2) { -v.y, v.x };
}

Int2 int2Subtract(Int2 v1, Int2 v2) {
   return (Int2) { v1.x - v2.x, v1.y - v2.y };
}

int pointOnLine(Int2 l1, Int2 l2, Int2 point) {
   return int2Dot(int2Perp(int2Subtract(l2, l1)), int2Subtract(point, l1));
}

boolean lineSegmentIntersectsAABBi(Int2 l1, Int2 l2, Recti *rect) {
   int topleft, topright, bottomright, bottomleft;

   if (l1.x > rect->x+rect->w && l2.x > rect->x + rect->w) { return false; }
   if (l1.x < rect->x && l2.x < rect->x) { return false; }
   if (l1.y > rect->y + rect->h && l2.y > rect->y + rect->h) { return false; }
   if (l1.y < rect->y && l2.y < rect->y) { return false; }

   topleft = pointOnLine(l1, l2, (Int2) { rect->x, rect->y });
   topright = pointOnLine(l1, l2, (Int2) { rect->x + rect->w, rect->y });
   bottomright = pointOnLine(l1, l2, (Int2) { rect->x + rect->w, rect->y + rect->h });
   bottomleft = pointOnLine(l1, l2, (Int2) { rect->x, rect->y + rect->h });

   if (topleft > 0 && topright > 0 && bottomright > 0 && bottomleft > 0) {
      return false;
   }

   if (topleft < 0 && topright < 0 && bottomright < 0 && bottomleft < 0) {
      return false;
   }

   return true;
   
}