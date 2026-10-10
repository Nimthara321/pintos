#ifndef THREADS_FIXED_POINT_H
#define THREADS_FIXED_POINT_H
#include <stdint.h>
#define FP_F (1<<14)
#define INT_TO_FP(n)((int32_t)((n)*FP_F))
#define FP_TO_INT_ZERO(x)((int32_t)((x)/FP_F))
#define FP_TO_INT_NEAR(x)((int32_t)((x)>=0? ((x)+FP_F/2)/FP_F : ((x)-FP_F/2)/FP_F))
#define FP_ADD(x,y)((x)+(y))
#define FP_SUB(x,y)((x)-(y))
#define FP_ADD_INT(x,n)((x)+(n)*FP_F)
#define FP_SUB_INT(x,n)((x)-(n)*FP_F)
#define FP_MUL(x,y)((int32_t)(((int64_t)(x))*(y)/FP_F))
#define FP_MUL_INT(x,n)((x)*(n))
#define FP_DIV(x,y)((int32_t)(((int64_t)(x))*FP_F/(y)))
#define FP_DIV_INT(x,n)((x)/(n))
#endif