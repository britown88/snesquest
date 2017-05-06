#pragma once

#include "Vector.h"

typedef struct {
   float data[16];
}Matrix;

Matrix matrixMultiply(Matrix *lhs, Matrix *rhs);
Float2 matrixMultiplyV(Matrix *lhs, Float2 rhs);
void matrixIdentity(Matrix *m);
void matrixOrtho(Matrix *m, float left, float right, float bottom, float top, float near, float far);
void matrixScale(Matrix *m, Float2 v);
void matrixTranslate(Matrix *m, Float2 v);

