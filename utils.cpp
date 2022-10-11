#include "utils.h"
#include <assert.h>
#include <vector>
#include <algorithm>  // std::sort

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


// ���ң�λ��ƫ�ƾ����кż�1���ϵ��£�λ��ƫ�ƾ����кż�1���Խ���·���е����ϵ����£�λ��ƫ�ƾ����к��кŸ���1��
// ÿ��·��ǰ�����صĴ��ۺͻҶȵ�λ��ƫ�������ǹ̶��ģ���������ƫ������1������(disp_range������)���ϵ���ƫ������ w ������(w * disp_range ������)�����ϵ�����ƫ������ (w + 1) ������((w+1) * disp_range������)��
// �߽�ʱ���кż�����������һ�����к�����Ϊ��ʼ�к�
// �Խ���1������/����
void utils::CostAggregateDiagonal_1(const uint8* img_data, const sint32& width, const sint32& height,
	const sint32& min_disparity, const sint32& max_disparity,
	const sint32& p1, const sint32& p2_init, const uint8* cost_init, uint8* cost_aggr, bool is_forward) {

	assert(width > 1 && height > 1 && max_disparity > min_disparity);

	// �ӲΧ
	const sint32 disp_range = max_disparity - min_disparity;

	// P1, P2
	const auto& P1 = p1;
	const auto& P2_Init = p2_init;

	// ��������->���£�:is_forward = true, direction = 1;
	// ��������->���ϣ�:is_forward = false, direction = -1;
	const sint32 direction = is_forward ? 1 : -1;

	// �ۺ�

	// �洢��ǰ�����кţ��ж��Ƿ񵽴�Ӱ��߽�
	sint32 current_row = 0;
	sint32 current_col = 0;

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
		
		// �Խ���·���ϵ���һ�����أ����width+1������
		// ���ӱ߽紦���кŰ�ԭ�������ǰ�����к�������һ�߽�
		current_row = is_forward ? 0 : height - 1;  // ��
		current_col = j;  // ��
		if (is_forward && current_col == width - 1 && current_row < height - 1) {
			// ���ϵ����£����ұ߽�
			cost_init_col = cost_init + (current_row + direction) * width * disp_range;
			cost_aggr_col = cost_aggr + (current_row + direction) * width * disp_range;
			img_col = img_data + (current_row + direction) * width;
		}
		else if (!is_forward && current_col == 0 && current_row > 0) {
			// ���µ����ϣ�����߽�
			cost_init_col = cost_init + (current_row + direction) * width * disp_range + (width - 1) * disp_range;
			cost_aggr_col = cost_aggr + (current_row + direction) * width * disp_range + (width - 1) * disp_range;
			img_col = img_data + (current_row + direction) * width + width - 1;
		}
		else {
			cost_init_col += direction * (width + 1) * disp_range;
			cost_aggr_col += direction * (width + 1) * disp_range;
			img_col += direction * (width + 1);
		}

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


			// ��ǰ�������к�
			current_row += direction;
			current_col += direction;

			// ��һ������
			// ���ӱ߽紦��
			if (is_forward && current_col == width - 1 && current_row < height - 1) {
				// ���ϵ����£����ұ߽�
				cost_init_col = cost_init + (current_row + direction) * width * disp_range;
				cost_aggr_col = cost_aggr + (current_row + direction) * width * disp_range;
				img_col = img_data + (current_row + direction) * width;
			}
			else if (!is_forward && current_col == 0 && current_row > 0) {
				// ���µ����ϣ�����߽�
				cost_init_col = cost_init + (current_row + direction) * width * disp_range + (width - 1) * disp_range;
				cost_aggr_col = cost_aggr + (current_row + direction) * width * disp_range + (width - 1) * disp_range;
				img_col = img_data + (current_row + direction) * width + width - 1;
			}
			else {
				cost_init_col += direction * (width + 1) * disp_range;
				cost_aggr_col += direction * (width + 1) * disp_range;
				img_col += direction * (width + 1);
			}

			// ����ֵ���¸�ֵ
			gray_last = gray;
		}
	}
}

// �߽�ʱ���кż�����������һ�����к�����Ϊ��ʼ�к�
// �Խ���2������/����
void utils::CostAggregateDiagonal_2(const uint8* img_data, const sint32& width, const sint32& height,
	const sint32& min_disparity, const sint32& max_disparity,
	const sint32& p1, const sint32& p2_init, const uint8* cost_init, uint8* cost_aggr, bool is_forward) {

	assert(width > 1 && height > 1 && max_disparity > min_disparity);

	// �ӲΧ
	const sint32 disp_range = max_disparity - min_disparity;

	// P1, P2
	const auto& P1 = p1;
	const auto& P2_Init = p2_init;

	// ��������->���£�:is_forward = true, direction = 1;
	// ��������->���ϣ�:is_forward = false, direction = -1;
	const sint32 direction = is_forward ? 1 : -1;

	// �ۺ�

	// �洢��ǰ�����кţ��ж��Ƿ񵽴�Ӱ��߽�
	sint32 current_row = 0;
	sint32 current_col = 0;

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

		// �Խ���·���ϵ���һ�����أ����width+1������
		// ���ӱ߽紦���кŰ�ԭ�������ǰ�����к�������һ�߽�
		current_row = is_forward ? 0 : height - 1;  // ��
		current_col = j;  // ��
		if (is_forward && current_col == 0 && current_row < height - 1) {
			// ���ϵ����£�����߽�
			cost_init_col = cost_init + (current_row + direction) * width * disp_range + (width - 1) * disp_range;
			cost_aggr_col = cost_aggr + (current_row + direction) * width * disp_range + (width - 1) * disp_range;
			img_col = img_data + (current_row + direction) * width;
		}
		else if (!is_forward && current_col == width - 1 && current_row > 0) {
			// ���µ����ϣ�����߽�
			cost_init_col = cost_init + (current_row + direction) * width * disp_range;
			cost_aggr_col = cost_aggr + (current_row + direction) * width * disp_range;
			img_col = img_data + (current_row + direction) * width;
		}
		else {
			cost_init_col += direction * (width - 1) * disp_range;
			cost_aggr_col += direction * (width - 1) * disp_range;
			img_col += direction * (width - 1);
		}

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


			// ��ǰ�������к�
			current_row += direction;
			current_col += direction;

			// ��һ������
			// ���ӱ߽紦��
			if (is_forward && current_col == 0 && current_row < height - 1) {
				// ���ϵ����£�����߽�
				cost_init_col = cost_init + (current_row + direction) * width * disp_range + (width - 1) * disp_range;
				cost_aggr_col = cost_aggr + (current_row + direction) * width * disp_range + (width - 1) * disp_range;
				img_col = img_data + (current_row + direction) * width;
			}
			else if (!is_forward && current_col == width - 1 && current_row > 0) {
				// ���µ����ϣ�����߽�
				cost_init_col = cost_init + (current_row + direction) * width * disp_range;
				cost_aggr_col = cost_aggr + (current_row + direction) * width * disp_range;
				img_col = img_data + (current_row + direction) * width;
			}
			else {
				cost_init_col += direction * (width - 1) * disp_range;
				cost_aggr_col += direction * (width - 1) * disp_range;
				img_col += direction * (width - 1);
			}

			// ����ֵ���¸�ֵ
			gray_last = gray;
		}
	}
}


// ȥ��С��ͨ��
void utils::RemoveSpeckles(float32* disparity_map, const sint32& width, const sint32& height, const sint32& diff_insame, const uint32& min_speckle_area, const float32 invalid_val)
{
	assert(width > 0 && height > 0);
	if (width <= 0 || height <= 0) {
		return;
	}

	// ��������Ƿ񱻷��ʵ�����
	std::vector<bool> visited(uint32(width * height), false);
	
	// ������
	for (sint32 i = 0; i < height; i++) {
		for (sint32 j = 0; j < width; j++) {

			// �������ʹ������غ���Ч����
			if (visited[i * width + j] || disparity_map[i * width + j] == invalid_val) {
				continue;
			}

			// ������ȱ������������
			// ����ͨ�����С����ֵ�������Ӳ�ȫ����Ϊ��Чֵ
			std::vector<std::pair<sint32, sint32>> vec;
			vec.emplace_back(i, j);
			visited[i * width + j] = true;
			uint32 cur = 0;
			uint32 next = 0;
			do {
				// ������ȱ���������׷��
				next = vec.size();
				for (uint32 k = cur; k < next; k++) {
					const auto& pixel = vec[k];
					const sint32 row = pixel.first;
					const sint32 col = pixel.second;
					const auto& disp_base = disparity_map[row * width + col];

					// 8�������
					for (sint8 r = -1; r <= 1; r++) {
						for (sint8 c = -1; c <= 1; c++) {
							if (r == 0 && c == 0) {
								continue;
							}
							sint32 rowr = row + r;
							sint32 colc = col + c;
							if (rowr >= 0 && rowr < height && colc >= 0 && colc < width) {
								if (!visited[rowr * width + colc] &&
									disparity_map[rowr * width + colc] != invalid_val &&
									abs(disparity_map[rowr * width + colc] - disp_base) < diff_insame) {
									vec.emplace_back(rowr, colc);
									visited[rowr * width + colc] = true;
								}
							}
						}
					}
				}
				cur = next;
			} while (next < vec.size());

			// ��ͨ�����С����ֵ�������Ӳ��Ϊ��Чֵ
			if (vec.size() < min_speckle_area) {
				for (auto& pix : vec) {
					disparity_map[pix.first * width + pix.second] = invalid_val;
				}
			}
		}
	}
}


// ��ֵ�˲�����Ϊһ��ƽ���㷨������Ҫ�������޳��Ӳ�ͼ�е�һЩ��������Ⱥ��㣬ͬʱ�������С��������
void utils::MedianFilter(const float32* in, float32* out, const sint32& width, const sint32& height, const sint32 wnd_size) 
{
	// ���ڰ뾶
	const sint32 radius = wnd_size / 2;
	// ���ڴ�С
	const sint32 size = wnd_size * wnd_size;

	// �洢�ֲ�����������
	std::vector<float32> wnd_data;
	// Ԥ����n��Ԫ�صĴ洢�ռ�
	wnd_data.reserve(size);

	// ������
	for (sint32 i = 0; i < height; i++) {
		for (sint32 j = 0; j < width; j++) {
			wnd_data.clear();

			// ��ȡ�ֲ���������
			for (sint32 r = -radius; r <= radius; r++) {
				for (sint32 c = -radius; c <= radius; c++) {
					const sint32 rowr = i + r;
					const sint32 colc = j + c;
					if (rowr >= 0 && rowr < height && colc >= 0 && colc < width) {
						wnd_data.push_back(in[rowr * width + colc]);
					}
				}
			}

			// ����
			std::sort(wnd_data.begin(), wnd_data.end());

			// ȡ��ֵ
			out[i * width + j] = wnd_data[wnd_data.size() / 2];
		}
	}
}