#include "SGM.h"
#include <algorithm>
#include <assert.h>

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

	// Census�任
	CensusTransform();

	// ���ۼ���
	ComputeCost();

	//// ���۾ۺ�
	CostAggregation();

	// �Ӳ����
	ComputeDisparity();

	//// ����Ӳ�ͼ
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

void SGM::ComputeDisparity() const
{
	// ��С����Ӳ�
	const sint32& min_disparity = option_.min_disparity;
	const sint32& max_disparity = option_.max_disparity;
	const sint32 disp_range = max_disparity - min_disparity;

	// δʹ�ô��۾ۺϣ����ó�ʼ���۴���
	//auto cost_ptr = cost_init_;
	auto cost_ptr = cost_aggr_;

	// �����ؼ��������Ӳ�
	for (sint32 i = 0; i < height_; i++) {
		for (sint32 j = 0; j < width_; j++) {

			uint16 min_cost = UINT16_MAX;
			uint16 max_cost = 0;
			sint32 best_disparity = 0;

			// �����ӲΧ�����д���ֵ�������С����ֵ����Ӧ���Ӳ�ֵ
			for (sint32 d = min_disparity; d < max_disparity; d++) {
				const sint32 d_itx = d - min_disparity;
				const auto& cost = cost_ptr[i * width_ * disp_range + j * disp_range + d_itx];
				if (min_cost > cost) {
					min_cost = cost;
					best_disparity = d;
				}
				max_cost = std::max(max_cost, static_cast<uint16>(cost));
			}

			// ����С����ֵ��Ӧ���Ӳ�ֵ��Ϊ���ص������Ӳ�
			//if (max_cost != min_cost) {
			//	disp_left_[i * width_ + j] = static_cast<float>(best_disparity);
			//}
			//else {
			//	// ��������Ӳ��´���ֵ��һ�������������Ч
			//	disp_left_[i * width_ + j] = Invalid_Float;
			//}
			disp_left_[i * width_ + j] = static_cast<float>(best_disparity);
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

	// ��4/8�����������
	for (sint32 i = 0; i < size; i++) {
		cost_aggr_[i] = cost_aggr_1_[i] + cost_aggr_2_[i] + cost_aggr_3_[i] + cost_aggr_4_[i];
		if (option_.num_paths == 10) {
			cost_aggr_[i] += cost_aggr_5_[i] + cost_aggr_6_[i] + cost_aggr_7_[i] + cost_aggr_8_[i];
		}
	}
}