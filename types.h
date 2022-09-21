#pragma once

#include<cstdint>
#include <limits>


/** \brief float无效值 */
constexpr auto Invalid_Float = std::numeric_limits<float>::infinity();


typedef int8_t		sint8;  //有符号8位整数
typedef uint8_t		uint8;
typedef int16_t		sint16;
typedef uint16_t		uint16;
typedef int32_t		sint32;
typedef uint32_t		uint32;
typedef int64_t		sint64;
typedef uint64_t		uint64;
typedef float		float32;
typedef double		float64;