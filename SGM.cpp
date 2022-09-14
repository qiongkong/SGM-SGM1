#include "SGM.h"
SGM::SGM()
{

}

SGM::~SGM()
{

}

bool SGM::Initialize(const uint32& width, const uint32& height, const SGMOption& option)
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
	void CensusTransform();

	// 代价计算
	void ComputeCost();

	// 代价聚合
	void CostAggregation();

	// 视差计算
	void ComputeDisparity();

	// 输出视差图
	memcpy(disp_left, disp_left_, width_ * height_ * sizeof(float32));

	return true;
}

bool SGM::Reset(const uint32& width, const uint32& height, const SGMOption& option) {
	// 释放内存
	if (census_left_ != nullptr) {
		delete[]
	}
}
