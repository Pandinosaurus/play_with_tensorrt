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

/* for OpenCV */
#include <opencv2/opencv.hpp>

/* for My modules */
#include "CommonHelper.h"
#include "InferenceHelper.h"
#include "ClassificationEngine.h"

/*** Macro ***/
#define TAG "ClassificationEngine"
#define PRINT(...)   COMMON_HELPER_PRINT(TAG, __VA_ARGS__)
#define PRINT_E(...) COMMON_HELPER_PRINT_E(TAG, __VA_ARGS__)

/* Model parameters */
#define MODEL_NAME   "mobilenetv2-7.onnx"
// #define MODEL_NAME   "mobilenetv2-7.trt"
#define LABEL_NAME   "synset.txt"


/*** Function ***/
int32_t ClassificationEngine::initialize(const std::string& workDir, const int32_t numThreads)
{
	/* Set model information */
	std::string modelFilename = workDir + "/model/" + MODEL_NAME;
	std::string labelFilename = workDir + "/model/" + LABEL_NAME;

	/* Set input tensor info */
	m_inputTensorList.clear();
	InputTensorInfo inputTensorInfo;
	inputTensorInfo.name = "data";
	inputTensorInfo.tensorType = TensorInfo::TENSOR_TYPE_FP32;
	inputTensorInfo.tensorDims.batch = 1;
	inputTensorInfo.tensorDims.width = 224;
	inputTensorInfo.tensorDims.height = 224;
	inputTensorInfo.tensorDims.channel = 3;
	inputTensorInfo.data = nullptr;
	inputTensorInfo.dataType = InputTensorInfo::DATA_TYPE_IMAGE_BGR;
	inputTensorInfo.swapColor = false;
	inputTensorInfo.imageInfo.width = -1;
	inputTensorInfo.imageInfo.height = -1;
	inputTensorInfo.imageInfo.channel = -1;
	inputTensorInfo.imageInfo.cropX = -1;
	inputTensorInfo.imageInfo.cropY = -1;
	inputTensorInfo.imageInfo.cropWidth = -1;
	inputTensorInfo.imageInfo.cropHeight = -1;
	inputTensorInfo.normalize.mean[0] = 0.485f;   	/* https://github.com/onnx/models/tree/master/vision/classification/mobilenet#preprocessing */
	inputTensorInfo.normalize.mean[1] = 0.456f;
	inputTensorInfo.normalize.mean[2] = 0.406f;
	inputTensorInfo.normalize.norm[0] = 0.229f;
	inputTensorInfo.normalize.norm[1] = 0.224f;
	inputTensorInfo.normalize.norm[2] = 0.225f;
#if 1
	/* Convert to speeden up normalization:  ((src / 255) - mean) / norm  = src * 1 / (255 * norm) - (mean / norm) */
	for (int32_t i = 0; i < 3; i++) {
		inputTensorInfo.normalize.mean[i] /= inputTensorInfo.normalize.norm[i];
		inputTensorInfo.normalize.norm[i] *= 255.0f;
		inputTensorInfo.normalize.norm[i] = 1.0f / inputTensorInfo.normalize.norm[i];
	}
#endif

	m_inputTensorList.push_back(inputTensorInfo);

	/* Set output tensor info */
	m_outputTensorList.clear();
	OutputTensorInfo outputTensorInfo;
	outputTensorInfo.name = "mobilenetv20_output_flatten0_reshape0";
	outputTensorInfo.tensorType = TensorInfo::TENSOR_TYPE_FP32;
	m_outputTensorList.push_back(outputTensorInfo);

	/* Create and Initialize Inference Helper */
	// m_inferenceHelper.reset(InferenceHelper::create(InferenceHelper::OPEN_CV));
	m_inferenceHelper.reset(InferenceHelper::create(InferenceHelper::TENSOR_RT));

	if (!m_inferenceHelper) {
		return RET_ERR;
	}
	if (m_inferenceHelper->setNumThread(4) != InferenceHelper::RET_OK) {
		m_inferenceHelper.reset();
		return RET_ERR;
	}
	if (m_inferenceHelper->initialize(modelFilename, m_inputTensorList, m_outputTensorList) != InferenceHelper::RET_OK) {
		m_inferenceHelper.reset();
		return RET_ERR;
	}

	/* read label */
	if (readLabel(labelFilename, m_labelList) != RET_OK) {
		return RET_ERR;
	}


	return RET_OK;
}

int32_t ClassificationEngine::finalize()
{
	if (!m_inferenceHelper) {
		PRINT_E("Inference helper is not created\n");
		return RET_ERR;
	}
	m_inferenceHelper->finalize();
	return RET_OK;
}


int32_t ClassificationEngine::invoke(const cv::Mat& originalMat, RESULT& result)
{
	if (!m_inferenceHelper) {
		PRINT_E("Inference helper is not created\n");
		return RET_ERR;
	}
	/*** PreProcess ***/
	const auto& tPreProcess0 = std::chrono::steady_clock::now();
	InputTensorInfo& inputTensorInfo = m_inputTensorList[0];
	/* do resize and color conversion here because some inference engine doesn't support these operations */
	cv::Mat imgSrc;
	cv::resize(originalMat, imgSrc, cv::Size(inputTensorInfo.tensorDims.width, inputTensorInfo.tensorDims.height));
	if (inputTensorInfo.imageInfo.channel == 3 && inputTensorInfo.swapColor) {
		cv::cvtColor(imgSrc, imgSrc, cv::COLOR_BGR2RGB);
	}
	inputTensorInfo.data = imgSrc.data;
	inputTensorInfo.dataType = InputTensorInfo::DATA_TYPE_IMAGE_BGR;
	inputTensorInfo.swapColor = false;
	inputTensorInfo.imageInfo.width = imgSrc.cols;
	inputTensorInfo.imageInfo.height = imgSrc.rows;
	inputTensorInfo.imageInfo.channel = imgSrc.channels();
	inputTensorInfo.imageInfo.cropX = 0;
	inputTensorInfo.imageInfo.cropY = 0;
	inputTensorInfo.imageInfo.cropWidth = imgSrc.cols;
	inputTensorInfo.imageInfo.cropHeight = imgSrc.rows;
	if (m_inferenceHelper->preProcess(m_inputTensorList) != InferenceHelper::RET_OK) {
		return RET_ERR;
	}
	const auto& tPreProcess1 = std::chrono::steady_clock::now();

	/*** Inference ***/
	const auto& tInference0 = std::chrono::steady_clock::now();
	if (m_inferenceHelper->invoke(m_outputTensorList) != InferenceHelper::RET_OK) {
		return RET_ERR;
	}
	const auto& tInference1 = std::chrono::steady_clock::now();

	/*** PostProcess ***/
	const auto& tPostProcess0 = std::chrono::steady_clock::now();
	/* Retrieve the result */
	std::vector<float_t> outputScoreList;
	outputScoreList.resize(m_outputTensorList[0].tensorDims.width * m_outputTensorList[0].tensorDims.height * m_outputTensorList[0].tensorDims.channel);
	const float_t* valFloat = m_outputTensorList[0].getDataAsFloat();
	for (int32_t i = 0; i < (int32_t)outputScoreList.size(); i++) {
		outputScoreList[i] = valFloat[i];
	}

	/* Find the max score */
	int32_t maxIndex = (int32_t)(std::max_element(outputScoreList.begin(), outputScoreList.end()) - outputScoreList.begin());
	auto maxScore = *std::max_element(outputScoreList.begin(), outputScoreList.end());
	PRINT("Result = %s (%d) (%.3f)\n", m_labelList[maxIndex].c_str(), maxIndex, maxScore);
	const auto& tPostProcess1 = std::chrono::steady_clock::now();

	/* Return the results */
	result.labelIndex = maxIndex;
	result.labelName = m_labelList[maxIndex];
	result.score = maxScore;
	result.timePreProcess = static_cast<std::chrono::duration<double_t>>(tPreProcess1 - tPreProcess0).count() * 1000.0;
	result.timeInference = static_cast<std::chrono::duration<double_t>>(tInference1 - tInference0).count() * 1000.0;
	result.timePostProcess = static_cast<std::chrono::duration<double_t>>(tPostProcess1 - tPostProcess0).count() * 1000.0;;

	return RET_OK;
}


int32_t ClassificationEngine::readLabel(const std::string& filename, std::vector<std::string>& labelList)
{
	std::ifstream ifs(filename);
	if (ifs.fail()) {
		PRINT_E("Failed to read %s\n", filename.c_str());
		return RET_ERR;
	}
	labelList.clear();
	std::string str;
	while (getline(ifs, str)) {
		labelList.push_back(str);
	}
	return RET_OK;
}
