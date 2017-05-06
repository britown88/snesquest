#include "Time.h"

Microseconds t_m2u(Milliseconds t) {
   return t * 1000;
}
Milliseconds t_u2m(Microseconds t) {
   return t / 1000;
}