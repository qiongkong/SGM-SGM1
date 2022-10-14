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

	// Census变换
	CensusTransform();

	// 代价计算
	ComputeCost();

	end = clock();
	std::cout << "代价计算，用时" << double(end - start) / CLOCKS_PER_SEC << "s" << std::endl;

	start = clock();
	//// 代价聚合
	CostAggregation();
	end = clock();
	std::cout << "代价聚合，用时" << double(end - start) / CLOCKS_PER_SEC << "s" << std::endl;

	start = clock();
	// 视差计算
	ComputeDisparity();
	end = clock();
	std::cout << "视差计算，用时" << double(end - start) / CLOCKS_PER_SEC << "s" << std::endl;

	start = clock();
	// 左右一致性检查
	if (option_.is_check_lr) {
		// 视差计算，右影像
		ComputeDisparityRight();
		// 一致性检查
		LRCheck();
	}
	// 移除小连通区
	if (option_.is_remove_speckles) {
		utils::RemoveSpeckles(disp_left_, width_, height_, 1, option_.min_speckle_area, Invalid_Float);
	}

	// 视差填充
	if (option_.is_fill_holes) {
		FillHolesInDispMap();
	}

	// 中值滤波
	utils::MedianFilter(disp_left_, disp_left_, width_, height_, 3);
	end = clock();
	std::cout << "后处理，  用时" << double(end - start) / CLOCKS_PER_SEC << "s" << std::endl << std::endl;


	// 输出视差图
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

	const bool is_check_unique = option_.is_check_unique;
	const float32 uniqueness_ratio = option_.uniqueness_ratio;

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

			// 唯一性检查
			if (is_check_unique){
				// 再次遍历，寻找次最小代价值
				for (sint32 d = min_disparity; d < max_disparity; d++) {
					if (d == best_disparity) {
						// 跳过最小代价值
						continue;
					}
					const auto& cost = cost_local[d - min_disparity];
					sec_min_cost = std::min(sec_min_cost, cost);
				}

				// 唯一性约束
				// 若(sec - min) / min < 1 - uniqueness, 则为无效估计
				if (sec_min_cost - min_cost <= static_cast<uint16>(min_cost * (1 - uniqueness_ratio))) {
					disparity[i * width + j] = Invalid_Float;
						continue;
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

void SGM::ComputeDisparityRight() const
{
	// 最小最大视差
	const sint32& min_disparity = option_.min_disparity;
	const sint32& max_disparity = option_.max_disparity;
	const sint32 disp_range = max_disparity - min_disparity;

	if (disp_range <= 0) {
		return;
	}

	// 右影像视差图
	const auto disparity = disp_right_;

	// 左影像聚合代价数组
	auto cost_ptr = cost_aggr_;

	const sint32 width = width_;
	const sint32 height = height_;

	// 唯一性检查
	const bool is_check_unique = option_.is_check_unique;
	const float32 uniqueness_ratio = option_.uniqueness_ratio;

	// 为加快读取效率，把单个像素的所有代价值存储到局部数组里
	std::vector<uint16> cost_local(disp_range);


	// 逐像素计算最优视差
	// 通过左影像的代价，计算右影像的代价
	// 右cost(xr, yr, d) = 左cost(xr+d, yl, d)
	// 视差等于同名点在左影像上的列号减去在右影像上的列号

	for (sint32 i = 0; i < height; i++) {
		for (sint32 j = 0; j < width; j++) {

			uint16 min_cost = UINT16_MAX;
			uint16 sec_min_cost = UINT16_MAX;
			sint32 best_disparity = 0;

			// 遍历视差范围内所有代价值，输出最小代价值及对应的视差值
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

			// 唯一性检查
			if (is_check_unique) {
				// 再次遍历，寻找次最小代价值
				for (sint32 d = min_disparity; d < max_disparity; d++) {
					if (d == best_disparity) {
						// 跳过最小代价值
						continue;
					}
					const auto& cost = cost_local[d - min_disparity];
					sec_min_cost = std::min(sec_min_cost, cost);
				}

				// 唯一性约束
				// 若(sec - min) / min < 1 - uniqueness, 则为无效估计
				if (sec_min_cost - min_cost <= static_cast<uint16>(min_cost * (1 - uniqueness_ratio))) {
					disparity[i * width + j] = Invalid_Float;
					continue;
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


// 左右一致性检查
// 左右互换，看同名点视差是否相隔不大于1
void SGM::LRCheck()
{
	const sint32 width = width_;
	const sint32 height = height_;

	const float& threshold = option_.lrcheck_thres;

	// 遮挡区：由于前景遮挡而在左视图上可见但在右视图上不可见的像素区域。
	// 误匹配区：位于非遮挡区域的错误匹配像素区域。
	// 假设q是p通过视差d找到的同名点，如果在左影像存在另外一个像素p'也和q是同名点而且它的视差比d要大，那么p就是遮挡区。
	// p因为遮挡而在右影像上不可见，所以它会匹配到右影像上的前景像素，而前景像素的视差值必定比背景像素大，即比p的视差大
	// 遮挡区像素和误匹配区像素

	auto& occlusions = occlusions_;
	auto& mismatches = mismatches_;

	occlusions.clear();
	mismatches.clear();

	// 左右一致性检查
	for (sint32 i = 0; i < height; i++) {
		for (sint32 j = 0; j < width; j++) {
			// 左影像视差值
			auto& disp = disp_left_[i * width + j];

			if (disp == Invalid_Float) {
				mismatches.emplace_back(i, j);
				continue;
			}

			// 根据视差值找到右影像上对应的同名像素
			const auto col_right = static_cast<sint32>(j - disp + 0.5);  // 四舍五入

			if (col_right >= 0 && col_right < width) {
				// 右影像同名像素视差值
				const auto& disp_r = disp_right_[i * width + col_right];

				// 判断两个视差值是否一致（差值不大于阈值）
				if (abs(disp - disp_r) > threshold) {
					// 区分遮挡区和误匹配区
					// 通过右影像视差算出在左影像的匹配像素，并获取视差disp_rl
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

					// 视差值无效
					disp = Invalid_Float;
				}
			}
			else {
				// 通过视差值在右影像找不到同名像素，超出影像范围
				disp = Invalid_Float;
				mismatches.emplace_back(i, j);
			}
		}
	}
}


// 视差填充
void SGM::FillHolesInDispMap()
{
	const sint32 width = width_;
	const sint32 height = height_;

	std::vector<float32> disp_collects;

	// 定义8个方向
	const float32 pi = 3.1415926f;
	float32 angle[8] = { pi, 3 * pi / 4, pi / 2, pi / 4, 0, 7 * pi / 4, 3 * pi / 2, 5 * pi / 4 };
	
	// 以像素为中心，等角度往外发射8条射线，收集每条射线碰到的第一个有效像素
	// 对于遮挡区像素，因为它的身份是背景像素，所以它是不能选择周围的前景像素视差值的，应该选择周围背景像素的视差值。由于背景像素视差值比前景像素小，所以在收集周围的有效视差值后，应选择较小的几个，具体哪一个呢？SGM作者选择的是次最小视差。
	// 对于误匹配像素，它并不位于遮挡区，所以周围的像素都是可见的，而且没有遮挡导致的视差非连续的情况，它就像一个连续的表面凸起的一小块噪声，这时周围的视差值都是等价的，没有哪个应选哪个不应选，这时取中值就很适合。
	
	// 最大搜索行程，过远的像素不必搜索
	const sint32 max_search_length = 1.0 * std::max(abs(option_.max_disparity), abs(option_.min_disparity));

	float32* disp_ptr = disp_left_;
	for (sint32 k = 0; k < 3; k++) {
		// 第一次循环处理遮挡区，第二次循环处理误匹配区
		// 待处理像素
		auto& prs_pixels = (k == 0) ? occlusions_ : mismatches_;

		if (prs_pixels.empty()) {
			continue;
		}

		std::vector<std::pair<sint32, sint32>> third_pixels;
		if (k == 2) {
			// 第三次循环处理前两次没有处理的像素
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

		// 遍历待处理像素
		for (auto n = 0u; n < prs_pixels.size(); n++) {
			auto pix = prs_pixels[n];
			const sint32 y = pix.first;
			const sint32 x = pix.second;

			// 8个方向上遇到的首个有效视差值
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

			// 若为空，进行下一个像素
			if (disp_collects.empty()) {
				continue;
			}

			std::sort(disp_collects.begin(), disp_collects.end());

			// 遮挡区：选择第二小的视差值
			// 误匹配区：选择中值
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