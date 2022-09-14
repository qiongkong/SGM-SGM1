#include "SGM.h"
SGM::SGM()
{

}

SGM::~SGM()
{

}

bool SGM::Initialize(const uint32& width, const uint32& height, const SGMOption& option)
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
	void CensusTransform();

	// ���ۼ���
	void ComputeCost();

	// ���۾ۺ�
	void CostAggregation();

	// �Ӳ����
	void ComputeDisparity();

	// ����Ӳ�ͼ
	memcpy(disp_left, disp_left_, width_ * height_ * sizeof(float32));

	return true;
}

bool SGM::Reset(const uint32& width, const uint32& height, const SGMOption& option) {
	// �ͷ��ڴ�
	if (census_left_ != nullptr) {
		delete[]
	}
}
