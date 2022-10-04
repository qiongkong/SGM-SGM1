#include "utils.h"
#include <assert.h>
#include <vector>

void utils::census_transform_5x5(const uint8* source, uint32* census,
	const sint32& width, const sint32& height)
{
	if (source == nullptr || census == nullptr || width <= 5u || height <= 5u) {
		return;
	}

	// �����ؼ���censusֵ
	for (sint32 i = 2; i < height - 2; i++) {
		for (sint32 j = 2; j < width - 2; j++) {

			// ��������ֵ
			const uint8 gray_center = source[i * width + j];

			//������СΪ5x5�Ĵ������������أ���һ�Ƚ�����ֵ����������ֵ��С������censusֵ
			uint32 census_val = 0u;
			for (sint32 r = -2; r <= 2; r++) {
				for (sint32 c = -2; c <= 2; c++) {
					census_val <<= 1;
					const uint8 gray = source[(i + r) * width + j + c];
					if (gray < gray_center) {
						census_val += 1;
					}
				}
			}
			census[i * width + j] = census_val;
		}
	}
}

uint32 utils::Hamming32(const uint32& x, const uint32& y)
{
	uint32 dist = 0, val = x ^ y;

	//����������1�ĸ���
	while (val) {
		++dist;
		val &= val - 1;
	}
	return dist;
}


// �K �� �L   5  3  7
// ��    ��	  1     2
// �J �� �I   8  4  6
//  �ۺ�ƥ�����-����1
uint8* cost_aggr_1_;
uint8* cost_aggr_2_;
uint8* cost_aggr_3_;
uint8* cost_aggr_4_;
uint8* cost_aggr_5_;
uint8* cost_aggr_6_;
uint8* cost_aggr_7_;
uint8* cost_aggr_8_;

void utils::CostAggregateLeftRight(const uint8* img_data, const sint32& width, const sint32& height, 
	const sint32& min_disparity, const sint32& max_disparity,
	const sint32& p1, const sint32& p2_init, const uint8* cost_init, uint8* cost_aggr, bool is_forward){
	
	assert(width > 0 && height > 0 && max_disparity > min_disparity);

	// �ӲΧ
	const sint32 disp_range = max_disparity - min_disparity;

	// P1, P2
	const auto& P1 = p1;
	const auto& P2_Init = p2_init;

	// ������->�ң�:is_forward = true, direction = 1;
	// ������->��:is_forward = false, direction = -1;
	const sint32 direction = is_forward ? 1 : -1;

	// �ۺ�
	for (sint32 i = 0u; i < height; i++) {
		// ·��ͷΪÿһ�е��ף�β��dir=-1��������
		auto cost_init_row = (is_forward) ? (cost_init + i * width * disp_range) : (cost_init + i * width * disp_range + (width - 1) * disp_range);
		auto cost_aggr_row = (is_forward) ? (cost_aggr + i * width * disp_range) : (cost_aggr + i * width * disp_range + (width - 1) * disp_range);
		auto img_row = (is_forward) ? (img_data + i * width) : (img_data + i * width + width - 1);

		// ·���ϵ�ǰ�Ҷ�ֵ����һ���Ҷ�ֵ
		uint8 gray = *img_row;
		uint8 gray_last = *img_row;

		// ·�����ϸ����صĴ������飬������Ԫ����Ϊ�˱���߽��������β����һ����
		std::vector<uint8> cost_last_path(disp_range + 2, UINT8_MAX);

		// ��ʼ������һ�����صľۺϴ��۵��ڳ�ʼ������
		memcpy(cost_aggr_row, cost_init_row, disp_range * sizeof(uint8));
		memcpy(&cost_last_path[1], cost_aggr_row, disp_range * sizeof(uint8));
		cost_init_row += direction * disp_range;
		cost_aggr_row += direction * disp_range;
		img_row += direction;

		// ·�����ϸ����ص���С����ֵ
		uint8 mincost_last_path = UINT8_MAX;
		for (auto cost : cost_last_path) {
			mincost_last_path = std::min(mincost_last_path, cost);
		}

		// �Է����ϵ�2�����ؿ�ʼ��˳��ۺ�
		for (sint32 j = 0; j < width - 1; j++) {
			gray = *img_row;
			uint8 min_cost = UINT8_MAX;
			for (sint32 d = 0; d < disp_range; d++) {
				// Lr(p,d) = C(p,d) + min( Lr(p-r,d), Lr(p-r,d-1) + P1, Lr(p-r,d+1) + P1, min(Lr(p-r))+P2 ) - min(Lr(p-r))
				const uint8 cost = cost_init_row[d];
				const uint16 l1 = cost_last_path[d + 1];
				const uint16 l2 = cost_last_path[d] + P1;
				const uint16 l3 = cost_last_path[d + 2] + P1;
				const uint16 l4 = mincost_last_path + std::max(P1, P2_Init / (abs(gray - gray_last) + 1));

				const uint8 cost_s = cost + static_cast<uint8>(std::min(std::min(l1, l2), std::min(l3, l4)) - mincost_last_path);

				cost_aggr_row[d] = cost_s;
				min_cost = std::min(min_cost, cost_s);
			}

			// �����ϸ����ص���С����ֵ�ʹ�������
			mincost_last_path = min_cost;
			memcpy(&cost_last_path[1], cost_aggr_row, disp_range * sizeof(uint8));

			// ��һ������
			cost_init_row += direction * disp_range;
			cost_aggr_row += direction * disp_range;
			img_row += direction;

			// ����ֵ���¸�ֵ
			gray_last = gray;
		}
	}
}


void utils::CostAggregateUpDown(const uint8* img_data, const sint32& width, const sint32& height,
	const sint32& min_disparity, const sint32& max_disparity,
	const sint32& p1, const sint32& p2_init, const uint8* cost_init, uint8* cost_aggr, bool is_forward) {

	assert(width > 0 && height > 0 && max_disparity > min_disparity);

	// �ӲΧ
	const sint32 disp_range = max_disparity - min_disparity;

	// P1, P2
	const auto& P1 = p1;
	const auto& P2_Init = p2_init;

	// ������->�£�:is_forward = true, direction = 1;
	// ������->�ϣ�:is_forward = false, direction = -1;
	const sint32 direction = is_forward ? 1 : -1;

	// �ۺ�
	for (sint32 j = 0; j < width; j++) {
		// ·��ͷΪÿһ�е��ף�β��dir=-1��������

		auto cost_init_col = (is_forward) ? (cost_init + j * disp_range) : (cost_init + (height - 1) * width * disp_range + j * disp_range);
		auto cost_aggr_col = (is_forward) ? (cost_aggr + j * disp_range) : (cost_aggr + (height - 1) * width * disp_range + j * disp_range);
		auto img_col = (is_forward) ? (img_data + j) : (img_data + (height - 1) * width + j);


		// ·���ϵ�ǰ�Ҷ�ֵ����һ���Ҷ�ֵ
		uint8 gray = *img_col;
		uint8 gray_last = *img_col;

		// ·�����ϸ����صĴ������飬������Ԫ����Ϊ�˱���߽��������β����һ����
		std::vector<uint8> cost_last_path(disp_range + 2, UINT8_MAX);

		// ��ʼ������һ�����صľۺϴ��۵��ڳ�ʼ������
		memcpy(cost_aggr_col, cost_init_col, disp_range * sizeof(uint8));
		memcpy(&cost_last_path[1], cost_aggr_col, disp_range * sizeof(uint8));
		cost_init_col += direction * width * disp_range;
		cost_aggr_col += direction * width * disp_range;
		img_col += direction * width;

		// ·�����ϸ����ص���С����ֵ
		uint8 mincost_last_path = UINT8_MAX;
		for (auto cost : cost_last_path) {
			mincost_last_path = std::min(mincost_last_path, cost);
		}

		// �Է����ϵ�2�����ؿ�ʼ��˳��ۺ�
		for (sint32 i = 0; i < height - 1; i++) {
			gray = *img_col;
			uint8 min_cost = UINT8_MAX;
			for (sint32 d = 0; d < disp_range; d++) {
				// Lr(p,d) = C(p,d) + min( Lr(p-r,d), Lr(p-r,d-1) + P1, Lr(p-r,d+1) + P1, min(Lr(p-r))+P2 ) - min(Lr(p-r))
				const uint8 cost = cost_init_col[d];
				const uint16 l1 = cost_last_path[d + 1];
				const uint16 l2 = cost_last_path[d] + P1;
				const uint16 l3 = cost_last_path[d + 2] + P1;
				const uint16 l4 = mincost_last_path + std::max(P1, P2_Init / (abs(gray - gray_last) + 1));

				const uint8 cost_s = cost + static_cast<uint8>(std::min(std::min(l1, l2), std::min(l3, l4)) - mincost_last_path);

				cost_aggr_col[d] = cost_s;
				min_cost = std::min(min_cost, cost_s);
			}

			// �����ϸ����ص���С����ֵ�ʹ�������
			mincost_last_path = min_cost;
			memcpy(&cost_last_path[1], cost_aggr_col, disp_range * sizeof(uint8));

			// ��һ������
			cost_init_col += direction * width * disp_range;
			cost_aggr_col += direction * width * disp_range;
			img_col += direction * width;

			// ����ֵ���¸�ֵ
			gray_last = gray;
		}
	}
}



