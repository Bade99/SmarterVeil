#pragma once

typedef unsigned char      u8; //prepping for jai
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef unsigned long long u64;

#define U8MAX  ((u8)0xff)
#define U16MAX ((u16)0xffff)
#define U32MAX ((u32)0xffff'ffff)
#define U64MAX ((u64)0xffff'ffff'ffff'ffff)

typedef signed char      i8;
typedef signed short     i16;
typedef signed int       i32;
typedef signed long long i64;

#define I8MIN  (-127i8 - 1)
#define I8MAX  127i8
#define I16MIN (-32767i16 - 1)
#define I16MAX 32767i16
#define I32MIN (-2147483647i32 - 1)
#define I32MAX 2147483647i32
#define I64MIN (-9223372036854775807i64 - 1)
#define I64MAX 9223372036854775807i64

typedef i32 b32; //boolean that doesnt unexpectedly break things

typedef float  f32;
typedef double f64;

#define F32MAX (3.402823466e+38F)

typedef char8_t  utf8;
typedef char16_t utf16;
typedef char32_t utf32;

#define Bytes(n) (n)
#define Kilobytes(n) ((n)*1024)
#define Megabytes(n) (Kilobytes(n)*1024)
#define Gigabytes(n) (Megabytes(n)*1024LL)
#define Terabytes(n) (Gigabytes(n)*1024LL)



static_assert(sizeof(u8) == 1);
static_assert(sizeof(u16) == 2);
static_assert(sizeof(u32) == 4);
static_assert(sizeof(u64) == 8);

static_assert(sizeof(i8) == 1);
static_assert(sizeof(i16) == 2);
static_assert(sizeof(i32) == 4);
static_assert(sizeof(i64) == 8);

#include <limits>