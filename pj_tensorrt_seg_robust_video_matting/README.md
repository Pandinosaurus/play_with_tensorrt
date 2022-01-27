# Robust Video Matting (RVM) with TensorRT in C++

![00_doc/demo.jpg](00_doc/demo.jpg)


https://user-images.githubusercontent.com/11009876/151349607-788f6b2f-df87-42ac-a953-9d27a21d7f06.mp4

https://user-images.githubusercontent.com/11009876/151362277-7a32fe9f-6d91-4f0d-bd81-19b7c006c8f4.mp4

## How to Run
1. Please follow the instruction: https://github.com/iwatake2222/play_with_tensorrt/blob/master/README.md
2. Additional steps:
    - Download the model using the following script
        - https://github.com/PINTO0309/PINTO_model_zoo/blob/main/242_RobustVideoMatting/download_resnet50.sh
        - copy `rvm_resnet50_720x1280/rvm_resnet50_720x1280.onnx` to `resource/model/rvm_resnet50_720x1280.onnx`
    - Build  `pj_tensorrt_seg_robust_video_matting` project (this directory)

* You can also try another model. Please modify model parameters in segmentation_engine.cpp

## Acknowledgements
- https://github.com/PeterL1n/RobustVideoMatting
- https://github.com/PINTO0309/PINTO_model_zoo
- Test image 1
    - Dj Allu Arjun Dance On Nagpuri Dj Song 2017 Latest by entertainment poker (Creative Commons)
    - https://www.youtube.com/watch?v=wlU2wDiP0ak
- Test image 2
    - [SPECIAL VIDEO] SEVENTEEN(세븐틴) - Anyone by  SEVENTEEN (Creative Commons)
    - https://www.youtube.com/watch?v=jjCBYlzwaY8
