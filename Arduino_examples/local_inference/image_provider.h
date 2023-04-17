#ifndef TENSORFLOW_LITE_MICRO_EXAMPLES_PERSON_DETECTION_IMAGE_PROVIDER_H_
#define TENSORFLOW_LITE_MICRO_EXAMPLES_PERSON_DETECTION_IMAGE_PROVIDER_H_

#include "tensorflow/lite/c/common.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"

TfLiteStatus GetImage(tflite::ErrorReporter* error_reporter, int image_width, int image_height, int channels, int8_t* image_data);
extern void initialize_camera(tflite::ErrorReporter* error_reporter, int image_width, int image_height, int channels, int8_t* image_data);
extern unsigned char jpeg_buffer[4096];
extern int jpeg_length;                      

#endif  // TENSORFLOW_LITE_MICRO_EXAMPLES_PERSON_DETECTION_IMAGE_PROVIDER_H_
