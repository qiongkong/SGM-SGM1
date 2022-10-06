#include "SGM.h"
#include <algorithm>
#include <assert.h>
#include <vector>

SGM::SGM()
{

}

bool SGM::Initialize(const sint32& width, const sint32& height, const SGMOption& option)
{
	// ···赋值

	// 影像尺寸
	width_ = width;
	height_ = height;
	// SGM参数
	option_ = option;

	if (width == 0 || height == 0) {
		return false;
	}

	//···开辟内存空间

	// census值（左右影像）
	census_left_ = new uint32[width * height]();
	census_right_ = new uint32[width * height]();

	// 匹配代价（初始/聚合）
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

	// 视差图
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

	// Census变换
	CensusTransform();

	// 代价计算
	ComputeCost();

	//// 代价聚合
	CostAggregation();

	// 视差计算
	ComputeDisparity();

	//// 输出视差图
	memcpy(disp_left, disp_left_, width_ * height_ * sizeof(sint32));

	return true;
}

bool SGM::Reset(const uint32& width, const uint32& height, const SGMOption& option){
	// 释放内存
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

	// 重置初始化标记
	is_initialized_ = false;

	// 初始化
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
	// 左右影像的census变换
	utils::census_transform_5x5(img_left_, census_left_, width_, height_);
	utils::census_transform_5x5(img_right_, census_right_, width_, height_);
}

void SGM::ComputeCost() const
{
	const sint32& min_disparity = option_.min_disparity;
	const sint32& max_disparity = option_.max_disparity;
	const sint32 disp_range = max_disparity - min_disparity;

	// 基于hamming距离计算代价
	for (sint32 i = 0; i < height_; i++) {
		for (sint32 j = 0; j < width_; j++) {



			// 逐视差计算代价值
			for (sint32 d = min_disparity; d < max_disparity; d++) {
				// 为啥最后+ (d - min_disparity),因为这里的cost是一个对应的值，而不是数列
				auto& cost = cost_init_[i * width_ * disp_range + j * disp_range + (d - min_disparity)];
				if (j - d < 0 || j - d >= width_) {
					cost = UINT8_MAX / 2;
					continue;
				}
				// 左影像census值
				const uint32 census_val_l = census_left_[i * width_ + j];

				// 右影像对应像点的census值
				const uint32 census_val_r = census_right_[i * width_ + j - d];

				//// 左影像census值
				//const auto& census_val_l = static_cast<uint32*>(census_left_)[i * width_ + j];
				//// 右影像对应像点的census值
				//const auto& census_val_r = static_cast<uint32*>(census_right_)[i * width_ + j - d];

				// 计算匹配代价
				cost = utils::Hamming32(census_val_l, census_val_r);
			}
		}
	}
}

void SGM::ComputeDisparity() const
{
	// 最小最大视差
	const sint32& min_disparity = option_.min_disparity;
	const sint32& max_disparity = option_.max_disparity;
	const sint32 disp_range = max_disparity - min_disparity;

	if (disp_range <= 0) {
		return;
	}

	// 左影像视差图
	const auto disparity = disp_left_;

	// 未使用代价聚合，暂用初始代价代替
	// auto cost_ptr = cost_init_;
	
	// 左影像聚合代价数组
	auto cost_ptr = cost_aggr_;

	const sint32 width = width_;
	const sint32 height = height_;

	// 为加快读取效率，把单个像素的所有代价值存储到局部数组里
	std::vector<uint16> cost_local(disp_range);


	// 逐像素计算最优视差
	for (sint32 i = 0; i < height; i++) {
		for (sint32 j = 0; j < width; j++) {

			uint16 min_cost = UINT16_MAX;
			uint16 sec_min_cost = UINT16_MAX;
			sint32 best_disparity = 0;

			// 遍历视差范围内所有代价值，输出最小代价值及对应的视差值
			for (sint32 d = min_disparity; d < max_disparity; d++) {
				const sint32 d_itx = d - min_disparity;
				const auto& cost = cost_local[d_itx] = cost_ptr[i * width * disp_range + j * disp_range + d_itx];
				if (min_cost > cost) {
					min_cost = cost;
					best_disparity = d;
				}
			}

			// 子像素拟合
			// 边界处理
			if (best_disparity == min_disparity || best_disparity == max_disparity - 1) {
				disparity[i * width + j] = Invalid_Float;
				continue;
			}
			// 最优视差前一个视差的代价值是cost_1, 后一个视差的代价值是cost_2
			const sint32 idx_1 = best_disparity - 1 - min_disparity;
			const sint32 idx_2 = best_disparity + 1 - min_disparity;
			const uint16 cost_1 = cost_local[idx_1];
			const uint16 cost_2 = cost_local[idx_2];

			// 解一元二次曲线极值即为最优值
			const uint16 denom = std::max(1, cost_1 + cost_2 - 2 * min_cost);
			disparity[i * width + j] = best_disparity + (cost_1 - cost_2) / (denom * 2.0f);

			// 将最小代价值对应的视差值即为像素的最优视差
			//if (max_cost != min_cost) {
			//	disp_left_[i * width_ + j] = static_cast<float>(best_disparity);
			//}
			//else {
			//	// 如果所有视差下代价值都一样，则该像素无效
			//	disp_left_[i * width_ + j] = Invalid_Float;
			//}
			// disp_left_[i * width_ + j] = static_cast<float>(best_disparity);
		}
	}

}

void SGM::CostAggregation() const{
	// 路径聚合
	// 1、左->右/右->左
	// 2、上->下/下->上
	// 3、左上->右下/右下->左上
	// 4、右上->左上/左下->右上
	//
	// ↘ ↓ ↙   5  3  7
	// →    ←	  1     2
	// ↗ ↑ ↖   8  4  6
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

	// 左右聚合
	utils::CostAggregateLeftRight(img_left_, width_, height_, min_disparity, max_disparity, P1, P2_Init, cost_init_, cost_aggr_1_, true);
	utils::CostAggregateLeftRight(img_left_, width_, height_, min_disparity, max_disparity, P1, P2_Init, cost_init_, cost_aggr_2_, false);

	// 上下聚合
	utils::CostAggregateUpDown(img_left_, width_, height_, min_disparity, max_disparity, P1, P2_Init, cost_init_, cost_aggr_3_, true);
	utils::CostAggregateUpDown(img_left_, width_, height_, min_disparity, max_disparity, P1, P2_Init, cost_init_, cost_aggr_4_, false);

	// 左上/右下聚合
	utils::CostAggregateDiagonal_1(img_left_, width_, height_, min_disparity, max_disparity, P1, P2_Init, cost_init_, cost_aggr_5_, true);
	utils::CostAggregateDiagonal_1(img_left_, width_, height_, min_disparity, max_disparity, P1, P2_Init, cost_init_, cost_aggr_6_, false);

	// 左上/右下聚合
	utils::CostAggregateDiagonal_2(img_left_, width_, height_, min_disparity, max_disparity, P1, P2_Init, cost_init_, cost_aggr_7_, true);
	utils::CostAggregateDiagonal_2(img_left_, width_, height_, min_disparity, max_disparity, P1, P2_Init, cost_init_, cost_aggr_8_, false);

	// 把4/8个方向加起来
	for (sint32 i = 0; i < size; i++) {
		cost_aggr_[i] = cost_aggr_1_[i] + cost_aggr_2_[i] + cost_aggr_3_[i] + cost_aggr_4_[i];
		if (option_.num_paths == 8) {
			cost_aggr_[i] += cost_aggr_5_[i] + cost_aggr_6_[i] + cost_aggr_7_[i] + cost_aggr_8_[i];
		}
	}
}