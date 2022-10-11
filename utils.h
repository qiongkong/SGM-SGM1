#pragma once
#include "types.h"

namespace utils
{
	void census_transform_5x5(const uint8* source, uint32* census, const sint32& width, const sint32& height);
	
	uint32 Hamming32(const uint32& x, const uint32& y);

	void CostAggregateLeftRight(const uint8* img_data, const sint32& width, const sint32& height, const sint32& min_disparity, const sint32& max_disparity,
		const sint32& p1, const sint32& p2_init, const uint8* cost_init, uint8* cost_aggr, bool is_forward);

	void CostAggregateUpDown(const uint8* img_data, const sint32& width, const sint32& height,
		const sint32& min_disparity, const sint32& max_disparity,
		const sint32& p1, const sint32& p2_init, const uint8* cost_init, uint8* cost_aggr, bool is_forward);

	void CostAggregateDiagonal_1(const uint8* img_data, const sint32& width, const sint32& height, const sint32& min_disparity, const sint32& max_disparity, const sint32& p1, const sint32& p2_init, const uint8* cost_init, uint8* cost_aggr, bool is_forward);
	
	void CostAggregateDiagonal_2(const uint8* img_data, const sint32& width, const sint32& height, const sint32& min_disparity, const sint32& max_disparity, const sint32& p1, const sint32& p2_init, const uint8* cost_init, uint8* cost_aggr, bool is_forward);
	
	void RemoveSpeckles(float32* disparity_map, const sint32& width, const sint32& height, const sint32& diff_insame, const uint32& min_speckle_area, const float32 invalid_val);
}