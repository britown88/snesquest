//#define ClosureT \
//    CLOSURE_RET(int) \
//    CLOSURE_NAME(SampleClosure) \
//    CLOSURE_ARGS(int)
//#include "Closure_Create.h"
//
//int foo(ClosureData a, int b){
//   return 0;
//}
//
//SampleClosure c;
//closureInit(SampleClosure)(&c, data, &foo, &destructor);
//closureCall(&c, 1);
//closureDestroy(SampleClosure)(&c);

#define ClosureTPart ClosureT
#include "Closure_Decl.h"
#define ClosureTPart ClosureT
#include "Closure_Impl.h"
#undef ClosureT