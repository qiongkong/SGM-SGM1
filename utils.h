#pragma once
#include "types.h"

namespace utils
{


	void census_transform_5x5(const uint8* source, uint32* census, const sint32& width, const sint32& height);
	
	uint32 Hamming32(const uint32& x, const uint32& y);
}