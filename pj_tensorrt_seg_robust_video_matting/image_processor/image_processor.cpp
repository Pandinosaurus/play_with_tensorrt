/* Copyright 2021 iwatake2222

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/
/*** Include ***/
/* for general */
#include <cstdint>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>
#include <array>
#include <algorithm>
#include <chrono>
#include <memory>

/* for OpenCV */
#include <opencv2/opencv.hpp>

/* for My modules */
#include "common_helper.h"
#include "common_helper_cv.h"
#include "segmentation_engine.h"
#include "image_processor.h"

/*** Macro ***/
#define TAG "ImageProcessor"
#define PRINT(...)   COMMON_HELPER_PRINT(TAG, __VA_ARGS__)
#define PRINT_E(...) COMMON_HELPER_PRINT_E(TAG, __VA_ARGS__)

/*** Global variable ***/
static std::unique_ptr<SegmentationEngine> s_engine;

static cv::Scalar s_bg_color;
static float  s_mask_area_border_x_ratio;

/*** Function ***/
static void DrawFps(cv::Mat& mat, double time_inference, cv::Point pos, double font_scale, int32_t thickness, cv::Scalar color_front, cv::Scalar color_back, bool is_text_on_rect = true)
{
    char text[64];
    static auto time_previous = std::chrono::steady_clock::now();
    auto time_now = std::chrono::steady_clock::now();
    double fps = 1e9 / (time_now - time_previous).count();
    time_previous = time_now;
    snprintf(text, sizeof(text), "FPS: %.1f, Inference: %.1f [ms]", fps, time_inference);
    CommonHelper::DrawText(mat, text, cv::Point(0, 0), 0.5, 2, CommonHelper::CreateCvColor(0, 0, 0), CommonHelper::CreateCvColor(180, 180, 180), true);
}


int32_t ImageProcessor::Initialize(const InputParam& input_param)
{
    if (s_engine) {
        PRINT_E("Already initialized\n");
        return -1;
    }

    s_engine.reset(new SegmentationEngine());
    if (s_engine->Initialize(input_param.work_dir, input_param.num_threads) != SegmentationEngine::kRetOk) {
        s_engine->Finalize();
        s_engine.reset();
        return -1;
    }

    s_bg_color = cv::Vec<float, 3>(0.0f, 255.0f, 0.0f);
    s_mask_area_border_x_ratio = 1.0f;

    return 0;
}

int32_t ImageProcessor::Finalize(void)
{
    if (!s_engine) {
        PRINT_E("Not initialized\n");
        return -1;
    }

    if (s_engine->Finalize() != SegmentationEngine::kRetOk) {
        return -1;
    }

    return 0;
}


int32_t ImageProcessor::Command(int32_t cmd)
{
    if (!s_engine) {
        PRINT_E("Not initialized\n");
        return -1;
    }

    //s_mask_area_border_x_ratio = cmd / 100.0f;
    return 0;
}

static void UpdateMaskArea()
{
    static float delta = 0.01f;
    s_mask_area_border_x_ratio += delta;
    if (s_mask_area_border_x_ratio > 1.0) {
        s_mask_area_border_x_ratio = 1.0;
        delta *= -1;
    }
    if (s_mask_area_border_x_ratio < 0.0) {
        s_mask_area_border_x_ratio = 0.0;
        delta *= -1;
    }
}

int32_t ImageProcessor::Process(cv::Mat& mat, Result& result)
{
    if (!s_engine) {
        PRINT_E("Not initialized\n");
        return -1;
    }

    //cv::resize(mat, mat, cv::Size(640, 640 * mat.rows / mat.cols));

    SegmentationEngine::Result segmentation_result;
    if (s_engine->Process(mat, segmentation_result) != SegmentationEngine::kRetOk) {
        return -1;
    }

    
    cv::Mat mat_fgr = segmentation_result.mat_fgr;
    cv::Mat mat_pha = segmentation_result.mat_pha;
#if 0
    mat_fgr.convertTo(mat_fgr, CV_8UC3, 255);
    mat_pha.convertTo(mat_pha, CV_8UC1, 255);
    cv::imshow("mat_fgr", mat_fgr);
    cv::imshow("mat_pha", mat_pha);
    cv::waitKey(1);
#else
    /*** Create result image ***/
    /* binalization */
    //cv::threshold(mat_pha, mat_pha, 0.5, 1.0, cv::THRESH_BINARY);

    /* Select masking area (just to show a nice demo) */
    UpdateMaskArea();
    cv::rectangle(mat_pha, cv::Rect(static_cast<int32_t>(s_mask_area_border_x_ratio * mat_pha.cols), 0, static_cast<int32_t>((1.0f - s_mask_area_border_x_ratio) * mat_pha.cols), mat_pha.rows), cv::Vec<float, 1>(1.0f), -1);

    /* Extact masked area */
    cv::Mat mat_composit;
    cv::resize(mat_pha, mat_pha, mat.size());
    mat_pha = CommonHelper::CombineMat1to3(mat_pha, mat_pha, mat_pha);  /* 1 channel to 3 channel for masking */
    mat.convertTo(mat_composit, CV_32FC3);
    cv::multiply(mat_composit, mat_pha, mat_composit);
    mat_composit.convertTo(mat_composit, CV_8UC3);

    /* draw background */
    static const cv::Mat kMatOnes = cv::Mat(mat_pha.size(), CV_32FC3, { 1.0f, 1.0f, 1.0f });
    cv::multiply(kMatOnes - mat_pha, s_bg_color, mat_pha);
    mat_pha.convertTo(mat_pha, CV_8UC3);
    mat_composit = mat_composit + mat_pha;

    cv::hconcat(mat, mat_composit, mat);
#endif
    DrawFps(mat, segmentation_result.time_inference, cv::Point(0, 0), 0.5, 2, CommonHelper::CreateCvColor(0, 0, 0), CommonHelper::CreateCvColor(180, 180, 180), true);

    /* Return the results */
    result.time_pre_process = segmentation_result.time_pre_process;
    result.time_inference = segmentation_result.time_inference;
    result.time_post_process = segmentation_result.time_post_process;

    return 0;
}

