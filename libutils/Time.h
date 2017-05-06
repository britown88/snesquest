#pragma once

#include <stdint.h>

typedef uint64_t Microseconds;
typedef uint64_t Milliseconds;

Microseconds t_m2u(Milliseconds t);
Milliseconds t_u2m(Microseconds t);
