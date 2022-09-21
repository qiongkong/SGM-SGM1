// SGM1.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include <opencv2/opencv.hpp>
#include "SGM.h"

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
    const char* argc[] = { ".\\data\\cones\\im2.png", ".\\data\\cones\\im6.png", ".\\data\\cones\\im888.png" };
    if (argv < 3) {
        std::cout << "Hello World!\n";
        return -1;
    }

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
    const uint32 width = static_cast<uint32>(img_left.cols);
    const uint32 height = static_cast<uint32>(img_right.rows);

    SGM::SGMOption sgm_option;
    sgm_option.num_paths = 8;
    sgm_option.min_disparity = 0;
    sgm_option.max_disparity = 64;
    sgm_option.p1 = 10;
    sgm_option.p2_int = 150;

    SGM sgm;

    // 初始化
    if (!sgm.Initialize(width, height, sgm_option)) {
        std::cout << "SGM初始化失败！" << std::endl;
        return -2;
    }

    // 匹配
    auto disparity = new float32[width * height]();
    if (!sgm.Match(img_left.data, img_right.data, disparity)) {
        std::cout << "SGM匹配失败！" << std::endl;
        return -2;
    }

    // 显示视差图
    cv::Mat disp_mat = cv::Mat(height, width, CV_8UC1);
    std::cout << disparity;
    for (uint32 i = 0; i < height; i++) {
        for (uint32 j = 0; j < width; j++) {
            const float32 disp = disparity[i * width + j];
            if (disp == Invalid_Float) {
                disp_mat.data[i * width + j] = 0;
            }
            else {
                disp_mat.data[i * width + j] = 2 * static_cast<uchar>(disp);
            }
        }
    }
    cv::imshow("视差图", disp_mat);
    cv::imwrite(argc[2], disp_mat);

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
