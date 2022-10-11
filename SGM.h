#pragma once
#include "types.h"
#include "utils.h"

class SGM
{
public:
	SGM();
	~SGM();

	// \brief SGM参数结构体
	struct SGMOption {
		uint8 num_paths;			// 聚合路径数
		sint32 min_disparity;	// 最小视差
		sint32 max_disparity;	// 最大视差

		// P1, P2
		// P2 = P2_int / (Ip-Tq)
		sint32 p1;				// 惩罚项参数P1
		sint32 p2_int;			// 惩罚项参数P2

		bool is_check_lr;  // 是否检查左右一致性
		float32 lrcheck_thres;  // 左右一致性约束阈值

		bool is_check_unique;  // 是否检查唯一性
		float32 uniqueness_ratio;  // 唯一性约束阈值，（最小代价-次最小代价）/最小代价 > 阈值，则有效像素

		bool is_remove_speckles;  // 是否移出小的连通区
		uint32 min_speckle_area;  // 最小的连通区面积（像素数），小于的移出

		SGMOption() : num_paths(8), min_disparity(0), max_disparity(640),
					  p1(10), p2_int(150), 
					  is_check_lr(true), lrcheck_thres(1.0f),
			          is_check_unique(true), uniqueness_ratio(0.95f),
					  is_remove_speckles(true), min_speckle_area(20) {
		}
	};

	/*
	* \briff 类的初始化，完成一些内存的预分配、参数的预设置等
	* \param width		输入，核线像对影像宽
	* \param height		输入，核线像对影像高
	* \param option		输入，SemiGlobalMatching参数
	*/
	bool Initialize(const sint32& width, const sint32& height, const SGMOption& option);

	/*
	* \brief 执行匹配
	* \param img_left		输入，左影像数据指针
	* \param img_right	    输入，右影像数据指针
	* \param disp_left		输出，左影像视差图指针，预先分配和影像等尺寸的内存空间
	*/
	bool Match(const uint8* img_left, const uint8* img_right, float32* disp_left);

	/*
	* \brief	 重设
	* \param width		输入，核线像对影像宽
	* \param height		输入，核线像对影像高
	* \param option		输入，SemiGlobalMatching参数
	*/
	bool Reset(const uint32& width, const uint32& height, const SGMOption& option);

private:
	// \brief Census变换
	void CensusTransform();

	// \brief 代价计算
	void ComputeCost() const;

	// \bried 代价聚合
	void CostAggregation() const;

	// \bried 视差计算
	void ComputeDisparity() const;

	void ComputeDisparityRight() const;

	// \bried 一致性检查
	void LRCheck() const;

	/* \brief SGM参数 */
	SGMOption option_;

	/* \brief 影像宽 */
	sint32 width_;

	/* \brief 影像高*/
	sint32 height_;

	/* \brief 左影像数据 */
	const uint8* img_left_;

	/* \brief 右影像数据 */
	const uint8* img_right_;

	/* \brief 左影像census值 */
	uint32* census_left_;

	/*	\brief 右影像census值	*/
	uint32* census_right_;

	/*	\brief 初始匹配代价		*/
	uint8* cost_init_;

	/* \brief 聚合匹配代价 */
	uint16* cost_aggr_;

	/* \brief 左影像视差图*/
	float32* disp_left_;

	/* \brief 右影像视差图*/
	float32* disp_right_;

	/* \brief 是否初始化标志 */
	bool is_initialized_;

	// ↘ ↓ ↙  5  3  7
	// →    ←	 1     2
	// ↗ ↑ ↖  8  4  6

	// 聚合匹配代价-方向1
	uint8* cost_aggr_1_;
	uint8* cost_aggr_2_;
	uint8* cost_aggr_3_;
	uint8* cost_aggr_4_;
	uint8* cost_aggr_5_;
	uint8* cost_aggr_6_;
	uint8* cost_aggr_7_;
	uint8* cost_aggr_8_;
};

