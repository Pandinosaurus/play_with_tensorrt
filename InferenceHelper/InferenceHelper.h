#ifndef INFERENCE_HELPER_
#define INFERENCE_HELPER_

/* for general */
#include <cstdint>
#include <cmath>
#include <string>
#include <vector>
#include <array>

class TensorInfo {
public:
	enum {
		TENSOR_TYPE_NONE,
		TENSOR_TYPE_UINT8,
		TENSOR_TYPE_FP32,
		TENSOR_TYPE_INT32,
		TENSOR_TYPE_INT64,
	};

public:
	TensorInfo() {
		name = "";
		id = -1;
		tensorType = TENSOR_TYPE_NONE;
		tensorDims.batch = 1;
		tensorDims.width = 1;
		tensorDims.height = 1;
		tensorDims.channel = 1;
	}
	~TensorInfo() {}

public:
	std::string name;			// [In] Set the name of tensor
	int32_t     id;				// [Out] Do not modify
	int32_t     tensorType;		// [In] Set the type of tensor (e.g. TENSOR_TYPE_FP32)
	struct {
		int32_t batch;   // 0
		int32_t width;   // 1
		int32_t height;  // 2
		int32_t channel; // 3
	} tensorDims;				// InputTensorInfo: [In] Set the dimentions of tensor 
								// OutputTensorInfo: [Out] The dimentions of tensor is set from model information
};

class InputTensorInfo : public TensorInfo {
public:
	enum {
		DATA_TYPE_IMAGE,
		DATA_TYPE_BLOB_NHWC,	// data which already finished preprocess(color conversion, resize, normalize, etc.)
		DATA_TYPE_BLOB_NCHW,
	};

public:
	InputTensorInfo() {
		data = nullptr;
		dataType = DATA_TYPE_IMAGE;
		imageInfo.width = 1;
		imageInfo.height = 1;
		imageInfo.channel = 1;
		imageInfo.cropX = 1;
		imageInfo.cropY = 1;
		imageInfo.cropWidth = 1;
		imageInfo.cropHeight = 1;
		imageInfo.isBGR = true;
		imageInfo.swapColor = false;
		for (int32_t i = 0; i < 3; i++) {
			normalize.mean[i] = 0.0f;
			normalize.norm[i] = 1.0f;
		}
	}
	~InputTensorInfo() {}

public:
	void* data;			// [In] Set the pointer to image/blob
	int32_t dataType;	// [In] Set the type of data (e.g. DATA_TYPE_IMAGE)

	struct {
		int32_t width;
		int32_t height;
		int32_t channel;
		int32_t cropX;
		int32_t cropY;
		int32_t cropWidth;
		int32_t cropHeight;
		bool    isBGR;        // used when channel == 3 (true: BGR, false: RGB)
		bool    swapColor;
	} imageInfo;              // [In] used when dataType == DATA_TYPE_IMAGE

	struct {
		float_t mean[3];
		float_t norm[3];
	} normalize;              // [In] used when dataType == DATA_TYPE_IMAGE
};


class OutputTensorInfo : public TensorInfo {
public:
	OutputTensorInfo() {
		data = nullptr;
		quant.scale = 0;
		quant.zeroPoint = 0;
		m_dataFp32 = nullptr;
	}
	~OutputTensorInfo() {
		if (m_dataFp32 != nullptr) {
			delete[] m_dataFp32;
		}
	}
	const float_t* getDataAsFloat() {
		if (tensorType == TENSOR_TYPE_UINT8) {
			int32_t dataNum = 1;
			dataNum = tensorDims.batch * tensorDims.channel * tensorDims.height * tensorDims.width;
			if (m_dataFp32 == nullptr) {
				m_dataFp32 = new float_t[dataNum];
			}
			for (int32_t i = 0; i < dataNum; i++) {
				const uint8_t* valUint8 = (uint8_t*)data;
				float_t valFloat = (valUint8[i] - quant.zeroPoint) * quant.scale;
				m_dataFp32[i] = valFloat;
			}
			return m_dataFp32;
		} else if (tensorType == TENSOR_TYPE_FP32) {
			return (float_t*)data;
		} else {
			return nullptr;
		}
	}

public:
	void* data;				// [Out] Pointer to the output data
	struct {
		float_t scale;
		uint8_t zeroPoint;
	} quant;				// [Out] Parameters for dequantization (convert uint8 to float)

private:
	float_t* m_dataFp32;
};


namespace cv {
	class Mat;
};

class InferenceHelper {
public:
	enum {
		RET_OK = 0,
		RET_ERR = -1,
	};

	typedef enum {
		TENSOR_RT,
		TENSORFLOW_LITE,
		TENSORFLOW_LITE_EDGETPU,
		TENSORFLOW_LITE_GPU,
		TENSORFLOW_LITE_XNNPACK,
		NCNN,
		MNN,
		OPEN_CV,
		OPEN_CV_GPU,
	} HELPER_TYPE;

public:
	static InferenceHelper* create(const HELPER_TYPE typeFw);
	static void preProcessByOpenCV(const InputTensorInfo& inputTensorInfo, bool isNCHW, cv::Mat& imgBlob);	// use this if the selected inference engine doesn't support pre-process

public:
	virtual ~InferenceHelper() {}
	virtual int32_t setNumThread(const int32_t numThread) = 0;
	virtual int32_t setCustomOps(const std::vector<std::pair<const char*, const void*>>& customOps) = 0;
	virtual int32_t initialize(const std::string& modelFilename, std::vector<InputTensorInfo>& inputTensorInfoList, std::vector<OutputTensorInfo>& outputTensorInfoList) = 0;
	virtual int32_t finalize(void) = 0;
	virtual int32_t preProcess(const std::vector<InputTensorInfo>& inputTensorInfoList) = 0;
	virtual int32_t invoke(std::vector<OutputTensorInfo>& outputTensorInfoList) = 0;

protected:
	HELPER_TYPE m_helperType;
};

#endif
