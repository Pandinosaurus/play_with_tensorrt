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
#include <fstream>
#include <memory>

/* for OpenCV */
#include <opencv2/opencv.hpp>

/* for My modules */
#include "CommonHelper.h"
#include "Classification.h"
#include "ImageProcessor.h"

/*** Macro ***/
#define TAG "ImageProcessor"
#define PRINT(...)   COMMON_HELPER_PRINT(TAG, __VA_ARGS__)
#define PRINT_E(...) COMMON_HELPER_PRINT_E(TAG, __VA_ARGS__)

/*** Global variable ***/
std::unique_ptr<Classification> s_classification;

/*** Function ***/
static cv::Scalar createCvColor(int32_t b, int32_t g, int32_t r) {
#ifdef CV_COLOR_IS_RGB
	return cv::Scalar(r, g, b);
#else
	return cv::Scalar(b, g, r);
#endif
}


int32_t ImageProcessor_initialize(const INPUT_PARAM* inputParam)
{
	if (s_classification) {
		PRINT_E("Already initialized\n");
		return -1;
	}

	s_classification.reset(new Classification());
	if (s_classification->initialize(inputParam->workDir, inputParam->numThreads) != Classification::RET_OK) {
		return -1;
	}
	return 0;
}

int32_t ImageProcessor_finalize(void)
{
	if (!s_classification) {
		PRINT_E("Not initialized\n");
		return -1;
	}

	if (s_classification->finalize() != Classification::RET_OK) {
		return -1;
	}

	return 0;
}


int32_t ImageProcessor_command(int32_t cmd)
{
	if (!s_classification) {
		PRINT_E("Not initialized\n");
		return -1;
	}

	switch (cmd) {
	case 0:
	default:
		PRINT_E("command(%d) is not supported\n", cmd);
		return -1;
	}
}


int32_t ImageProcessor_process(cv::Mat* mat, OUTPUT_PARAM* outputParam)
{
	if (!s_classification) {
		PRINT_E("Not initialized\n");
		return -1;
	}

	const cv::Mat originalMat = *mat;
	Classification::RESULT result = { 0 };
	if (s_classification->invoke(originalMat, result) != Classification::RET_OK) {
		return -1;
	}

	/* Draw the result */
	std::string resultStr;
	resultStr = "Result:" + result.labelName + " (score = " + std::to_string(result.score) + ")";
	cv::putText(originalMat, resultStr, cv::Point(10, 10), cv::FONT_HERSHEY_PLAIN, 1, createCvColor(0, 0, 0), 3);
	cv::putText(originalMat, resultStr, cv::Point(10, 10), cv::FONT_HERSHEY_PLAIN, 1, createCvColor(0, 255, 0), 1);

	/* Return the results */
	outputParam->classId = result.labelIndex;
	snprintf(outputParam->label, sizeof(outputParam->label), "%s", result.labelName.c_str());
	outputParam->score = result.score;
	outputParam->timePreProcess = result.timePreProcess;
	outputParam->timeInference = result.timeInference;
	outputParam->timePostProcess = result.timePostProcess;

	return 0;
}

