#include "SGM.h"
#include <algorithm>
#include <assert.h>
#include <vector>
#include "utils.h"
#include <ctime>
#include <iostream>

clock_t start, end;

SGM::SGM()
{

}

bool SGM::Initialize(const sint32& width, const sint32& height, const SGMOption& option)
{
	// ��������ֵ

	// Ӱ��ߴ�
	width_ = width;
	height_ = height;
	// SGM����
	option_ = option;

	if (width == 0 || height == 0) {
		return false;
	}

	//�����������ڴ�ռ�

	// censusֵ������Ӱ��
	census_left_ = new uint32[width * height]();
	census_right_ = new uint32[width * height]();

	// ƥ����ۣ���ʼ/�ۺϣ�
	const sint32 disp_range = option.max_disparity - option.min_disparity;
	if (disp_range <= 0){
		return false;
	}
	cost_init_ = new uint8[width * height * disp_range]();
	cost_aggr_ = new uint16[width * height * disp_range]();
	const sint32 size = width * height * disp_range;
	cost_aggr_1_ = new uint8[size]();
	cost_aggr_2_ = new uint8[size]();
	cost_aggr_3_ = new uint8[size]();
	cost_aggr_4_ = new uint8[size]();
	cost_aggr_5_ = new uint8[size]();
	cost_aggr_6_ = new uint8[size]();
	cost_aggr_7_ = new uint8[size]();
	cost_aggr_8_ = new uint8[size]();

	// �Ӳ�ͼ
	disp_left_ = new float32[width * height]();
	disp_right_ = new float32[width * height]();

	is_initialized_ = census_left_ && census_right_ && cost_init_ && cost_aggr_ && disp_left_;

	return is_initialized_;
}

bool SGM::Match(const uint8* img_left, const uint8* img_right, float32* disp_left)
{
	if(!is_initialized_){
		return false;
	}
	if(img_left == nullptr || img_right == nullptr){
		return false;
	}

	img_left_ = img_left;
	img_right_ = img_right;

	start = clock();

	// Census�任
	CensusTransform();

	// ���ۼ���
	ComputeCost();

	end = clock();
	std::cout << "���ۼ��㣬��ʱ" << double(end - start) / CLOCKS_PER_SEC << "s" << std::endl;

	start = clock();
	//// ���۾ۺ�
	CostAggregation();
	end = clock();
	std::cout << "���۾ۺϣ���ʱ" << double(end - start) / CLOCKS_PER_SEC << "s" << std::endl;

	start = clock();
	// �Ӳ����
	ComputeDisparity();
	end = clock();
	std::cout << "�Ӳ���㣬��ʱ" << double(end - start) / CLOCKS_PER_SEC << "s" << std::endl;

	start = clock();
	// ����һ���Լ��
	if (option_.is_check_lr) {
		// �Ӳ���㣬��Ӱ��
		ComputeDisparityRight();
		// һ���Լ��
		LRCheck();
	}
	// �Ƴ�С��ͨ��
	if (option_.is_remove_speckles) {
		utils::RemoveSpeckles(disp_left_, width_, height_, 1, option_.min_speckle_area, Invalid_Float);
	}

	// �Ӳ����
	if (option_.is_fill_holes) {
		FillHolesInDispMap();
	}

	// ��ֵ�˲�
	utils::MedianFilter(disp_left_, disp_left_, width_, height_, 3);
	end = clock();
	std::cout << "����  ��ʱ" << double(end - start) / CLOCKS_PER_SEC << "s" << std::endl << std::endl;


	// ����Ӳ�ͼ
	memcpy(disp_left, disp_left_, width_ * height_ * sizeof(sint32));

	return true;
}

bool SGM::Reset(const uint32& width, const uint32& height, const SGMOption& option){
	// �ͷ��ڴ�
	if (census_left_ != nullptr) {
		delete[] census_left_;
		census_left_ = nullptr;
	}
	if (census_right_ != nullptr) {
		delete[] census_right_;
		census_right_ = nullptr;
	}
	if (cost_init_ != nullptr) {
		delete[] cost_init_;
		cost_init_ = nullptr;
	}
	if (cost_aggr_ != nullptr) {
		delete[] cost_aggr_;
		cost_aggr_ = nullptr;
	}
	if (disp_left_ != nullptr) {
		delete[] disp_left_;
		disp_left_ = nullptr;
	}

	// ���ó�ʼ�����
	is_initialized_ = false;

	// ��ʼ��
	return Initialize(width, height, option);
}

SGM::~SGM()
{
	if (census_left_ != nullptr) {
		delete[] census_left_;
		census_left_ = nullptr;
	}
	if (census_right_ != nullptr) {
		delete[] census_right_;
		census_right_ = nullptr;
	}
	if (cost_init_ != nullptr) {
		delete[] cost_init_;
		cost_init_ = nullptr;
	}
	if (cost_aggr_ != nullptr) {
		delete[] cost_aggr_;
		cost_aggr_ = nullptr;
	}
	if (disp_left_ != nullptr) {
		delete[] disp_left_;
		disp_left_ = nullptr;
	}

	is_initialized_ = false;
}

void SGM::CensusTransform()
{
	// ����Ӱ���census�任
	utils::census_transform_5x5(img_left_, census_left_, width_, height_);
	utils::census_transform_5x5(img_right_, census_right_, width_, height_);
}

void SGM::ComputeCost() const
{
	const sint32& min_disparity = option_.min_disparity;
	const sint32& max_disparity = option_.max_disparity;
	const sint32 disp_range = max_disparity - min_disparity;

	// ����hamming����������
	for (sint32 i = 0; i < height_; i++) {
		for (sint32 j = 0; j < width_; j++) {



			// ���Ӳ�������ֵ
			for (sint32 d = min_disparity; d < max_disparity; d++) {
				// Ϊɶ���+ (d - min_disparity),��Ϊ�����cost��һ����Ӧ��ֵ������������
				auto& cost = cost_init_[i * width_ * disp_range + j * disp_range + (d - min_disparity)];
				if (j - d < 0 || j - d >= width_) {
					cost = UINT8_MAX / 2;
					continue;
				}
				// ��Ӱ��censusֵ
				const uint32 census_val_l = census_left_[i * width_ + j];

				// ��Ӱ���Ӧ����censusֵ
				const uint32 census_val_r = census_right_[i * width_ + j - d];

				//// ��Ӱ��censusֵ
				//const auto& census_val_l = static_cast<uint32*>(census_left_)[i * width_ + j];
				//// ��Ӱ���Ӧ����censusֵ
				//const auto& census_val_r = static_cast<uint32*>(census_right_)[i * width_ + j - d];

				// ����ƥ�����
				cost = utils::Hamming32(census_val_l, census_val_r);
			}
		}
	}
}


void SGM::CostAggregation() const{
	// ·���ۺ�
	// 1����->��/��->��
	// 2����->��/��->��
	// 3������->����/����->����
	// 4������->����/����->����
	//
	// �K �� �L   5  3  7
	// ��    ��	  1     2
	// �J �� �I   8  4  6
	//
	const auto& min_disparity = option_.min_disparity;
	const auto& max_disparity = option_.max_disparity;
	assert(max_disparity > min_disparity);

	const sint32 size = width_ * height_ * (max_disparity - min_disparity);
	if (size <= 0) {
		return;
	}

	const auto& P1 = option_.p1;
	const auto& P2_Init = option_.p2_int;

	// ���Ҿۺ�
	utils::CostAggregateLeftRight(img_left_, width_, height_, min_disparity, max_disparity, P1, P2_Init, cost_init_, cost_aggr_1_, true);
	utils::CostAggregateLeftRight(img_left_, width_, height_, min_disparity, max_disparity, P1, P2_Init, cost_init_, cost_aggr_2_, false);

	// ���¾ۺ�
	utils::CostAggregateUpDown(img_left_, width_, height_, min_disparity, max_disparity, P1, P2_Init, cost_init_, cost_aggr_3_, true);
	utils::CostAggregateUpDown(img_left_, width_, height_, min_disparity, max_disparity, P1, P2_Init, cost_init_, cost_aggr_4_, false);

	// ����/���¾ۺ�
	utils::CostAggregateDiagonal_1(img_left_, width_, height_, min_disparity, max_disparity, P1, P2_Init, cost_init_, cost_aggr_5_, true);
	utils::CostAggregateDiagonal_1(img_left_, width_, height_, min_disparity, max_disparity, P1, P2_Init, cost_init_, cost_aggr_6_, false);

	// ����/���¾ۺ�
	utils::CostAggregateDiagonal_2(img_left_, width_, height_, min_disparity, max_disparity, P1, P2_Init, cost_init_, cost_aggr_7_, true);
	utils::CostAggregateDiagonal_2(img_left_, width_, height_, min_disparity, max_disparity, P1, P2_Init, cost_init_, cost_aggr_8_, false);

	// ��4/8�����������
	for (sint32 i = 0; i < size; i++) {
		cost_aggr_[i] = cost_aggr_1_[i] + cost_aggr_2_[i] + cost_aggr_3_[i] + cost_aggr_4_[i];
		if (option_.num_paths == 8) {
			cost_aggr_[i] += cost_aggr_5_[i] + cost_aggr_6_[i] + cost_aggr_7_[i] + cost_aggr_8_[i];
		}
	}
}

void SGM::ComputeDisparity() const
{
	// ��С����Ӳ�
	const sint32& min_disparity = option_.min_disparity;
	const sint32& max_disparity = option_.max_disparity;
	const sint32 disp_range = max_disparity - min_disparity;

	if (disp_range <= 0) {
		return;
	}

	// ��Ӱ���Ӳ�ͼ
	const auto disparity = disp_left_;

	// δʹ�ô��۾ۺϣ����ó�ʼ���۴���
	// auto cost_ptr = cost_init_;

	// ��Ӱ��ۺϴ�������
	auto cost_ptr = cost_aggr_;

	const sint32 width = width_;
	const sint32 height = height_;

	const bool is_check_unique = option_.is_check_unique;
	const float32 uniqueness_ratio = option_.uniqueness_ratio;

	// Ϊ�ӿ��ȡЧ�ʣ��ѵ������ص����д���ֵ�洢���ֲ�������
	std::vector<uint16> cost_local(disp_range);


	// �����ؼ��������Ӳ�
	for (sint32 i = 0; i < height; i++) {
		for (sint32 j = 0; j < width; j++) {

			uint16 min_cost = UINT16_MAX;
			uint16 sec_min_cost = UINT16_MAX;
			sint32 best_disparity = 0;

			// �����ӲΧ�����д���ֵ�������С����ֵ����Ӧ���Ӳ�ֵ
			for (sint32 d = min_disparity; d < max_disparity; d++) {
				const sint32 d_itx = d - min_disparity;
				const auto& cost = cost_local[d_itx] = cost_ptr[i * width * disp_range + j * disp_range + d_itx];
				if (min_cost > cost) {
					min_cost = cost;
					best_disparity = d;
				}
			}

			// Ψһ�Լ��
			if (is_check_unique){
				// �ٴα�����Ѱ�Ҵ���С����ֵ
				for (sint32 d = min_disparity; d < max_disparity; d++) {
					if (d == best_disparity) {
						// ������С����ֵ
						continue;
					}
					const auto& cost = cost_local[d - min_disparity];
					sec_min_cost = std::min(sec_min_cost, cost);
				}

				// Ψһ��Լ��
				// ��(sec - min) / min < 1 - uniqueness, ��Ϊ��Ч����
				if (sec_min_cost - min_cost <= static_cast<uint16>(min_cost * (1 - uniqueness_ratio))) {
					disparity[i * width + j] = Invalid_Float;
						continue;
				}
			}

			// ���������
			// �߽紦��
			if (best_disparity == min_disparity || best_disparity == max_disparity - 1) {
				disparity[i * width + j] = Invalid_Float;
				continue;
			}
			// �����Ӳ�ǰһ���Ӳ�Ĵ���ֵ��cost_1, ��һ���Ӳ�Ĵ���ֵ��cost_2
			const sint32 idx_1 = best_disparity - 1 - min_disparity;
			const sint32 idx_2 = best_disparity + 1 - min_disparity;
			const uint16 cost_1 = cost_local[idx_1];
			const uint16 cost_2 = cost_local[idx_2];

			// ��һԪ�������߼�ֵ��Ϊ����ֵ
			const uint16 denom = std::max(1, cost_1 + cost_2 - 2 * min_cost);
			disparity[i * width + j] = best_disparity + (cost_1 - cost_2) / (denom * 2.0f);

			// ����С����ֵ��Ӧ���Ӳ�ֵ��Ϊ���ص������Ӳ�
			//if (max_cost != min_cost) {
			//	disp_left_[i * width_ + j] = static_cast<float>(best_disparity);
			//}
			//else {
			//	// ��������Ӳ��´���ֵ��һ�������������Ч
			//	disp_left_[i * width_ + j] = Invalid_Float;
			//}
			// disp_left_[i * width_ + j] = static_cast<float>(best_disparity);
		}
	}
}

void SGM::ComputeDisparityRight() const
{
	// ��С����Ӳ�
	const sint32& min_disparity = option_.min_disparity;
	const sint32& max_disparity = option_.max_disparity;
	const sint32 disp_range = max_disparity - min_disparity;

	if (disp_range <= 0) {
		return;
	}

	// ��Ӱ���Ӳ�ͼ
	const auto disparity = disp_right_;

	// ��Ӱ��ۺϴ�������
	auto cost_ptr = cost_aggr_;

	const sint32 width = width_;
	const sint32 height = height_;

	// Ψһ�Լ��
	const bool is_check_unique = option_.is_check_unique;
	const float32 uniqueness_ratio = option_.uniqueness_ratio;

	// Ϊ�ӿ��ȡЧ�ʣ��ѵ������ص����д���ֵ�洢���ֲ�������
	std::vector<uint16> cost_local(disp_range);


	// �����ؼ��������Ӳ�
	// ͨ����Ӱ��Ĵ��ۣ�������Ӱ��Ĵ���
	// ��cost(xr, yr, d) = ��cost(xr+d, yl, d)
	// �Ӳ����ͬ��������Ӱ���ϵ��кż�ȥ����Ӱ���ϵ��к�

	for (sint32 i = 0; i < height; i++) {
		for (sint32 j = 0; j < width; j++) {

			uint16 min_cost = UINT16_MAX;
			uint16 sec_min_cost = UINT16_MAX;
			sint32 best_disparity = 0;

			// �����ӲΧ�����д���ֵ�������С����ֵ����Ӧ���Ӳ�ֵ
			for (sint32 d = min_disparity; d < max_disparity; d++) {
				const sint32 d_itx = d - min_disparity;
				const sint32 col_left = j + d;
				if (col_left >= 0 && col_left < width) {
					const auto& cost = cost_local[d_itx] = cost_ptr[i * width * disp_range + col_left * disp_range + d_itx];
					if (min_cost > cost) {
						min_cost = cost;
						best_disparity = d;
					}
				}
				else {
					cost_local[d_itx] = UINT16_MAX;
				}
			}

			// Ψһ�Լ��
			if (is_check_unique) {
				// �ٴα�����Ѱ�Ҵ���С����ֵ
				for (sint32 d = min_disparity; d < max_disparity; d++) {
					if (d == best_disparity) {
						// ������С����ֵ
						continue;
					}
					const auto& cost = cost_local[d - min_disparity];
					sec_min_cost = std::min(sec_min_cost, cost);
				}

				// Ψһ��Լ��
				// ��(sec - min) / min < 1 - uniqueness, ��Ϊ��Ч����
				if (sec_min_cost - min_cost <= static_cast<uint16>(min_cost * (1 - uniqueness_ratio))) {
					disparity[i * width + j] = Invalid_Float;
					continue;
				}
			}

			// ���������
			// �߽紦��
			if (best_disparity == min_disparity || best_disparity == max_disparity - 1) {
				disparity[i * width + j] = Invalid_Float;
				continue;
			}
			// �����Ӳ�ǰһ���Ӳ�Ĵ���ֵ��cost_1, ��һ���Ӳ�Ĵ���ֵ��cost_2
			const sint32 idx_1 = best_disparity - 1 - min_disparity;
			const sint32 idx_2 = best_disparity + 1 - min_disparity;
			const uint16 cost_1 = cost_local[idx_1];
			const uint16 cost_2 = cost_local[idx_2];

			// ��һԪ�������߼�ֵ��Ϊ����ֵ
			const uint16 denom = std::max(1, cost_1 + cost_2 - 2 * min_cost);
			disparity[i * width + j] = best_disparity + (cost_1 - cost_2) / (denom * 2.0f);

			// ����С����ֵ��Ӧ���Ӳ�ֵ��Ϊ���ص������Ӳ�
			//if (max_cost != min_cost) {
			//	disp_left_[i * width_ + j] = static_cast<float>(best_disparity);
			//}
			//else {
			//	// ��������Ӳ��´���ֵ��һ�������������Ч
			//	disp_left_[i * width_ + j] = Invalid_Float;
			//}
			// disp_left_[i * width_ + j] = static_cast<float>(best_disparity);
		}
	}
}


// ����һ���Լ��
// ���һ�������ͬ�����Ӳ��Ƿ����������1
void SGM::LRCheck()
{
	const sint32 width = width_;
	const sint32 height = height_;

	const float& threshold = option_.lrcheck_thres;

	// �ڵ���������ǰ���ڵ���������ͼ�Ͽɼ���������ͼ�ϲ��ɼ�����������
	// ��ƥ������λ�ڷ��ڵ�����Ĵ���ƥ����������
	// ����q��pͨ���Ӳ�d�ҵ���ͬ���㣬�������Ӱ���������һ������p'Ҳ��q��ͬ������������Ӳ��dҪ����ôp�����ڵ�����
	// p��Ϊ�ڵ�������Ӱ���ϲ��ɼ�����������ƥ�䵽��Ӱ���ϵ�ǰ�����أ���ǰ�����ص��Ӳ�ֵ�ض��ȱ������ش󣬼���p���Ӳ��
	// �ڵ������غ���ƥ��������

	auto& occlusions = occlusions_;
	auto& mismatches = mismatches_;

	occlusions.clear();
	mismatches.clear();

	// ����һ���Լ��
	for (sint32 i = 0; i < height; i++) {
		for (sint32 j = 0; j < width; j++) {
			// ��Ӱ���Ӳ�ֵ
			auto& disp = disp_left_[i * width + j];

			if (disp == Invalid_Float) {
				mismatches.emplace_back(i, j);
				continue;
			}

			// �����Ӳ�ֵ�ҵ���Ӱ���϶�Ӧ��ͬ������
			const auto col_right = static_cast<sint32>(j - disp + 0.5);  // ��������

			if (col_right >= 0 && col_right < width) {
				// ��Ӱ��ͬ�������Ӳ�ֵ
				const auto& disp_r = disp_right_[i * width + col_right];

				// �ж������Ӳ�ֵ�Ƿ�һ�£���ֵ��������ֵ��
				if (abs(disp - disp_r) > threshold) {
					// �����ڵ�������ƥ����
					// ͨ����Ӱ���Ӳ��������Ӱ���ƥ�����أ�����ȡ�Ӳ�disp_rl
					// if (disp_rl > disp)
					//		pixel in occlusions
					// else
					//		pixel in mismatches
					const sint32 col_rl = static_cast<sint32>(col_right + disp_r + 0.5);
					if (col_rl > 0 && col_rl < width) {
						const auto& disp_l = disp_left_[i * width + col_rl];
						if (disp_l > disp) {
							occlusions.emplace_back(i, j);
						}
						else {
							mismatches.emplace_back(i, j);
						}
					}
					else {
						mismatches.emplace_back(i, j);
					}

					// �Ӳ�ֵ��Ч
					disp = Invalid_Float;
				}
			}
			else {
				// ͨ���Ӳ�ֵ����Ӱ���Ҳ���ͬ�����أ�����Ӱ��Χ
				disp = Invalid_Float;
				mismatches.emplace_back(i, j);
			}
		}
	}
}


// �Ӳ����
void SGM::FillHolesInDispMap()
{
	const sint32 width = width_;
	const sint32 height = height_;

	std::vector<float32> disp_collects;

	// ����8������
	const float32 pi = 3.1415926f;
	float32 angle[8] = { pi, 3 * pi / 4, pi / 2, pi / 4, 0, 7 * pi / 4, 3 * pi / 2, 5 * pi / 4 };
	
	// ������Ϊ���ģ��ȽǶ����ⷢ��8�����ߣ��ռ�ÿ�����������ĵ�һ����Ч����
	// �����ڵ������أ���Ϊ��������Ǳ������أ��������ǲ���ѡ����Χ��ǰ�������Ӳ�ֵ�ģ�Ӧ��ѡ����Χ�������ص��Ӳ�ֵ�����ڱ��������Ӳ�ֵ��ǰ������С���������ռ���Χ����Ч�Ӳ�ֵ��Ӧѡ���С�ļ�����������һ���أ�SGM����ѡ����Ǵ���С�Ӳ
	// ������ƥ�����أ�������λ���ڵ�����������Χ�����ض��ǿɼ��ģ�����û���ڵ����µ��Ӳ�������������������һ�������ı���͹���һС����������ʱ��Χ���Ӳ�ֵ���ǵȼ۵ģ�û���ĸ�Ӧѡ�ĸ���Ӧѡ����ʱȡ��ֵ�ͺ��ʺϡ�
	
	// ��������г̣���Զ�����ز�������
	const sint32 max_search_length = 1.0 * std::max(abs(option_.max_disparity), abs(option_.min_disparity));

	float32* disp_ptr = disp_left_;
	for (sint32 k = 0; k < 3; k++) {
		// ��һ��ѭ�������ڵ������ڶ���ѭ��������ƥ����
		// ����������
		auto& prs_pixels = (k == 0) ? occlusions_ : mismatches_;

		if (prs_pixels.empty()) {
			continue;
		}

		std::vector<std::pair<sint32, sint32>> third_pixels;
		if (k == 2) {
			// ������ѭ������ǰ����û�д��������
			for (sint32 i = 0; i < height; i++) {
				for (sint32 j = 0; j < width; j++) {
					if (disp_ptr[i * width + j] == Invalid_Float) {
						third_pixels.emplace_back(i, j);
					}
				}
			}
			prs_pixels = third_pixels;
		}

		std::vector<float32> fill_disps(prs_pixels.size());

		// ��������������
		for (auto n = 0u; n < prs_pixels.size(); n++) {
			auto pix = prs_pixels[n];
			const sint32 y = pix.first;
			const sint32 x = pix.second;

			// 8���������������׸���Ч�Ӳ�ֵ
			disp_collects.clear();
			for (sint8 s = 0; s < 8; s++) {
				const float32 ang = angle[s];
				const float32 sina = float32(sin(ang));
				const float32 cosa = float32(cos(ang));

				for (sint32 m = 1; m < max_search_length; m++) {
					const sint32 yy = lround(y + m * sina);
					const sint32 xx = lround(x + m * cosa);
					if (yy < 0 || yy >= height || xx < 0 || xx >= width) {
						break;
					}
					const auto& disp = *(disp_ptr + yy * width + xx);
					if (disp != Invalid_Float) {
						disp_collects.push_back(disp);
						break;
					}
				}
			}

			// ��Ϊ�գ�������һ������
			if (disp_collects.empty()) {
				continue;
			}

			std::sort(disp_collects.begin(), disp_collects.end());

			// �ڵ�����ѡ��ڶ�С���Ӳ�ֵ
			// ��ƥ������ѡ����ֵ
			if (k == 0) {
				if (disp_collects.size() > 1) {
					fill_disps[n] = disp_collects[1];
				}
				else {
					fill_disps[n] = disp_collects[0];
				}
			}
			else {
				fill_disps[n] = disp_collects[disp_collects.size() / 2];
			}
		}

		for (auto n = 0u; n < prs_pixels.size(); n++) {
			auto& pix = prs_pixels[n];
			const sint32 y = pix.first;
			const sint32 x = pix.second;
			disp_ptr[y * width + x] = fill_disps[n];
		}

	}
}