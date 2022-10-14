// SGM1.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include <opencv2/opencv.hpp>
#include <algorithm>
#include "SGM.h"
#include <ctime>

/*
*  \brief
*  \param argv 3
*  \param argc argc[1]:左影像路径 argc[2]:右影像路径 argc[3]: 视差图路径
*  \return 
*/
//int main(int argv, char** argc)


int main(int argv)
{
    argv = 3;
    const char* argc[] = { ".\\data\\cones\\im2.png", ".\\data\\cones\\im6.png", ".\\data\\cones\\聚合.png" };
    if (argv < 3) {
        std::cout << "Hello World!\n";
        return -1;
    }
    clock_t start, end;

    // ```读取影像
    std::string path_left = argc[0];
    std::string path_right = argc[1];


    cv::Mat img_left = cv::imread(path_left, cv::IMREAD_GRAYSCALE);
    cv::Mat img_right = cv::imread(path_right, cv::IMREAD_GRAYSCALE);


    if (img_left.data == nullptr || img_right.data == nullptr) {
        std::cout << "读取影像失败！" << std::endl;
        return -1;
    }
    if (img_left.rows != img_right.rows || img_left.cols != img_right.cols) {
        std::cout << "左右影像尺寸大小不一致！" << std::endl;
        return -1;
    }

    //```SGM匹配
    const sint32 width = static_cast<uint32>(img_left.cols);
    const sint32 height = static_cast<uint32>(img_right.rows);

    //// 左右影像 灰度数据
    //auto bytes_left = new uint8[width * height];
    //auto bytes_right = new uint8[width * height];
    //for (uint16 i = 0; i < height; i++) {
    //    for (uint16 j = 0; j < width; j++) {
    //        bytes_left[i * width + j] = img_left.at<uint8>(i, j);
    //        bytes_right[i * width + j] = img_right.at<uint8>(i, j);
    //    }
    //}

    std::cout << "Loading Pictures...Done" << std::endl;

    // SGM匹配 参数设计
    SGM::SGMOption sgm_option;
    // 聚合路径数
    sgm_option.num_paths = 8;
    // 候选视差范围
    sgm_option.min_disparity = 0;
    sgm_option.max_disparity = 64;
    // 惩罚项
    sgm_option.p1 = 10;
    sgm_option.p2_int = 150;
    // 左右一致性检查
    sgm_option.is_check_lr = true;
    sgm_option.lrcheck_thres = 1.0f;
    // 唯一性约束
    sgm_option.is_check_unique = true;
    sgm_option.uniqueness_ratio = 0.99;
    // 小连通区
    sgm_option.is_remove_speckles = true;
    sgm_option.min_speckle_area = 30;
    // 视差图填充
    // 视差填充的结果不很可靠，根据实际考虑，工程科研
    sgm_option.is_fill_holes = true;

    printf("宽度为 %d, 高度为 %d, 视差范围为 [%d, %d] \n", width, height, sgm_option.min_disparity, sgm_option.max_disparity);

    // 定义SGM匹配类实例
    SGM sgm;

    // 初始化
    std::cout << std::endl << "SGM初始化中..." << std::endl;

    start = clock();
    if (!sgm.Initialize(width, height, sgm_option)) {
        std::cout << "SGM初始化失败！" << std::endl;
        return -2;
    }
    end = clock();
    std::cout << "SGM初始化完成，用时" << double(end - start) / CLOCKS_PER_SEC << "s" << std::endl << std::endl;

    // 匹配
    std::cout << "SGM匹配中..." << std::endl;

    start = clock();
    // disparity用来保存子像素视差结果
    auto disparity = new float32[width * height]();
    if (!sgm.Match(img_left.data, img_right.data, disparity)) {
        std::cout << "SGM匹配失败！" << std::endl;
        return -2;
    }
    end = clock();
    std::cout << "SGM匹配完成，用时" << double(end - start) / CLOCKS_PER_SEC << "s" << std::endl;


    // 显示视差图
    // 计算点云不能用disp_mat数据，只用来显示和保存结果
    // 计算点云应用上面的disparity数组数据，是子像素浮点数
    cv::Mat disp_mat = cv::Mat(height, width, CV_8UC1);

    //for (uint32 i = 0; i < height; i++) {
    //    for (uint32 j = 0; j < width; j++) {
    //        const float32 disp = disparity[i * width + j];
    //        if (disp == Invalid_Float) {
    //            disp_mat.data[i * width + j] = 0;
    //        }
    //        else {
    //            disp_mat.data[i * width + j] = 2 * static_cast<uchar>(disp);
    //        }
    //    }
    //}

    //float32 min_disp = width, max_disp = -width;
    //for (sint32 i = 0; i < height; i++) {
    //    for (sint32 j = 0; j < width; j++) {
    //        const float32 disp = disparity[i * width + j];
    //        if (disp != Invalid_Float) {
    //            min_disp = std::min(min_disp, disp);
    //            max_disp = std::max(max_disp, disp);
    //        }
    //    }
    //}
    //for (sint32 i = 0; i < height; i++) {
    //    for (sint32 j = 0; j < width; j++) {
    //        const float32 disp = disparity[i * width + j];
    //        if (disp == Invalid_Float) {
    //            disp_mat.data[i * width + j] = 0;
    //        }
    //        else {
    //            disp_mat.data[i * width + j] = static_cast<uchar>((disp - min_disp) / (max_disp - min_disp) * 255);
    //        }
    //    }
    //}

    for (uint32 i = 0; i < height; i++) {
        for (uint32 j = 0; j < width; j++) {
            const float32 disp = disparity[i * width + j];
            if (disp == Invalid_Float) {
                disp_mat.data[i * width + j] = 0;
            }
            else {
                disp_mat.data[i * width + j] = static_cast<uchar>(disp * 255. / 64);
            }
        }
    }

    //cv::Mat disp8U = cv::Mat(disp_mat.rows, disp_mat.cols, CV_8UC1);
    //disp_mat.convertTo(disp8U, CV_8UC1, 1);
    
    cv::imshow("视差图", disp_mat);
    cv::imwrite(argc[2], disp_mat);

    cv::Mat disp_color;
    cv::applyColorMap(disp_mat, disp_color, cv::COLORMAP_JET);
    cv::imshow("视差图-伪彩色", disp_color);
    std::string disp_color_map_path = argc[2];
    disp_color_map_path += "伪彩色.png";
    cv::imwrite(disp_color_map_path, disp_color);

    cv::waitKey(0);

    delete[] disparity;
    disparity = nullptr;

    return 0;
}

// 运行程序: Ctrl + F5 或调试 >“开始执行(不调试)”菜单
// 调试程序: F5 或调试 >“开始调试”菜单

// 入门使用技巧: 
//   1. 使用解决方案资源管理器窗口添加/管理文件
//   2. 使用团队资源管理器窗口连接到源代码管理
//   3. 使用输出窗口查看生成输出和其他消息
//   4. 使用错误列表窗口查看错误
//   5. 转到“项目”>“添加新项”以创建新的代码文件，或转到“项目”>“添加现有项”以将现有代码文件添加到项目
//   6. 将来，若要再次打开此项目，请转到“文件”>“打开”>“项目”并选择 .sln 文件
