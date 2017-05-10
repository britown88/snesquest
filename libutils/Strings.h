#pragma once

#include "libutils\Defs.h"
#include "libutils\DLLBullshit.h"

typedef const char* StringView;
typedef char* MutableStringView;
StringView stringIntern(StringView view);