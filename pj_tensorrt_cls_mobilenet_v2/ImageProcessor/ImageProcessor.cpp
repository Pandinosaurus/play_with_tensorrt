/*** Include ***/
/* for general */
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <fstream>


/* for OpenCV */
#include <opencv2/opencv.hpp>

#include "InferenceHelper.h"
#include "ImageProcessor.h"

/*** Macro ***/
#if defined(ANDROID) || defined(__ANDROID__)
#define CV_COLOR_IS_RGB
#include <android/log.h>
#define TAG "MyApp_NDK"
#define _PRINT(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#else
#define _PRINT(...) printf(__VA_ARGS__)
#endif
#define PRINT(...) _PRINT("[ImageProcessor] " __VA_ARGS__)

#define CHECK(x)                              \
  if (!(x)) {                                                \
	PRINT("Error at %s:%d\n", __FILE__, __LINE__); \
	exit(1);                                                 \
  }

/* Model parameters */
#define MODEL_NAME   "mobilenetv2-7.onnx"
#define LABEL_NAME   "synset.txt"
static const float PIXEL_MEAN[3] = { 0.0f, 0.0f, 0.0f };
static const float PIXEL_STD[3] = { 1.0f,  1.0f, 1.0f };

/*** Global variable ***/
static std::vector<std::string> s_labels;
static InferenceHelper *s_inferenceHelper;
static TensorInfo *s_inputTensor;
static TensorInfo *s_outputTensor;

/*** Function ***/
static cv::Scalar createCvColor(int b, int g, int r) {
#ifdef CV_COLOR_IS_RGB
	return cv::Scalar(r, g, b);
#else
	return cv::Scalar(b, g, r);
#endif
}

static void readLabel(const char* filename, std::vector<std::string> & labels)
{
	std::ifstream ifs(filename);
	if (ifs.fail()) {
		PRINT("failed to read %s\n", filename);
		return;
	}
	std::string str;
	while (getline(ifs, str)) {
		labels.push_back(str);
	}
}


int ImageProcessor_initialize(const INPUT_PARAM *inputParam)
{
	s_inferenceHelper = InferenceHelper::create(InferenceHelper::TENSOR_RT);

	std::string modelFilename = std::string(inputParam->workDir) + "/model/" + MODEL_NAME;
	std::string labelFilename = std::string(inputParam->workDir) + "/model/" + LABEL_NAME;

	s_inferenceHelper->initialize(modelFilename.c_str(), inputParam->numThreads);
	s_inputTensor = new TensorInfo();
	s_outputTensor = new TensorInfo();

	s_inferenceHelper->getTensorByName("data", s_inputTensor);
	s_inferenceHelper->getTensorByName("mobilenetv20_output_flatten0_reshape0", s_outputTensor);

	/* read label */
	readLabel(labelFilename.c_str(), s_labels);

	return 0;
}

int ImageProcessor_command(int cmd)
{
	switch (cmd) {
	default:
		PRINT("command(%d) is not supported\n", cmd);
		return -1;
	}
}


int ImageProcessor_process(cv::Mat *mat, OUTPUT_PARAM *outputParam)
{
	/*** PreProcess ***/
	cv::Mat inputImage;
	int modelInputWidth = s_inputTensor->dims[3];
	int modelInputHeight = s_inputTensor->dims[2];
	int modelInputChannel = s_inputTensor->dims[1];

	cv::resize(*mat, inputImage, cv::Size(modelInputWidth, modelInputHeight));
	cv::cvtColor(inputImage, inputImage, cv::COLOR_BGR2RGB);
	if (s_inputTensor->type == TensorInfo::TENSOR_TYPE_UINT8) {
		inputImage.convertTo(inputImage, CV_8UC3);
	} else {
		inputImage.convertTo(inputImage, CV_32FC3, 1.0 / 255);
		cv::subtract(inputImage, cv::Scalar(cv::Vec<float, 3>(PIXEL_MEAN)), inputImage);
		cv::divide(inputImage, cv::Scalar(cv::Vec<float, 3>(PIXEL_STD)), inputImage);
	}

	/* Set data to input tensor */
#if 0
	s_inferenceHelper->setBufferToTensorByIndex(s_inputTensor->index, (char*)inputImage.data, (int)(inputImage.total() * inputImage.elemSize()));
#else
	if (s_inputTensor->type == TensorInfo::TENSOR_TYPE_UINT8) {
		memcpy(s_inputTensor->data, inputImage.reshape(0, 1).data, sizeof(uint8_t) * 1 * modelInputWidth * modelInputHeight * modelInputChannel);
	} else {
		/* convert NHWC to NCHW */
		float *dst = (float*)(s_inputTensor->data);
		float *src = (float*)(inputImage.data);
		#pragma omp parallel for
		for (int c = 0; c < modelInputChannel; c++) {
			for (int i = 0; i < modelInputWidth * modelInputHeight; i++) {
				dst[c * modelInputWidth * modelInputHeight + i] = src[i * modelInputChannel + c];
				// dst[c * modelInputWidth * modelInputHeight + i] = (src[i * modelInputChannel + c] - PIXEL_MEAN[c]) / PIXEL_STD[c];
			}
		}
	}
#endif
	
	/*** Inference ***/
	s_inferenceHelper->invoke();

	/*** PostProcess ***/
	/* Retrieve the result */
	std::vector<float> outputScoreList;
	outputScoreList.resize(s_outputTensor->dims[1]);
	const float* valFloat = s_outputTensor->getDataAsFloat();
	printf("valFloat = %p\n", valFloat);
	for (int i = 0; i < (int)outputScoreList.size(); i++) {
		outputScoreList[i] = valFloat[i];
	}

	/* Find the max score */
	int maxIndex = (int)(std::max_element(outputScoreList.begin(), outputScoreList.end()) - outputScoreList.begin());
	auto maxScore = *std::max_element(outputScoreList.begin(), outputScoreList.end());
	PRINT("Result = %s (%d) (%.3f)\n", s_labels[maxIndex].c_str(), maxIndex, maxScore);

	/* Draw the result */
	std::string resultStr;
	resultStr = "Result:" + s_labels[maxIndex] + " (score = " + std::to_string(maxScore) + ")";
	cv::putText(*mat, resultStr, cv::Point(10, 10), cv::FONT_HERSHEY_PLAIN, 1, createCvColor(0, 0, 0), 3);
	cv::putText(*mat, resultStr, cv::Point(10, 10), cv::FONT_HERSHEY_PLAIN, 1, createCvColor(0, 255, 0), 1);

	/* Return the results */
	outputParam->classId = maxIndex;
	snprintf(outputParam->label, sizeof(outputParam->label), "%s", s_labels[maxIndex].c_str());
	outputParam->score = maxScore;
	
	return 0;
}


int ImageProcessor_finalize(void)
{
	s_inferenceHelper->finalize();
	delete s_inputTensor;
	delete s_outputTensor;
	delete s_inferenceHelper;
	return 0;
}
