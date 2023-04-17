#ifndef TENSORFLOW_LITE_MICRO_EXAMPLES_PERSON_DETECTION_APPLICATION_H_
#define TENSORFLOW_LITE_MICRO_EXAMPLES_PERSON_DETECTION_APPLICATION_H_

//Required libraries 
#include <TensorFlowLite.h>
#include "main_functions.h"
#include "image_provider.h"
#include "detection_responder.h"
#include "model_settings.h"
#include "person_detect_model_data.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "tensorflow/lite/version.h"
#include <ArduinoBLE.h>
#include "tasks.h"

extern int RX_BUFFER_SIZE;
extern bool RX_BUFFER_FIXED_LENGTH;
extern unsigned long prevNow;
extern bool wasConnected;
extern char* uuidOftxChar;
extern char* uuidOfrxChar;
extern char* uuidOfService;
extern char* nameofPeripheral;
extern byte tmp[256];

#define TASK_AMOUNT 3

typedef enum TaskName
{
    F_camera,
    F_image,
    F_led2
};

extern struct Task application [TASK_AMOUNT];
extern void app_init();
extern struct Task *get_task_ti(struct TaskInstance *ti);
extern struct Task *get_task_e(struct Edge *e);
extern void execute(struct TaskInstance *selected_task);

//BLE functions
extern void blePeripheralConnectHandler(BLEDevice central);
extern void blePeripheralDisconnectHandler(BLEDevice central);
extern void onRxCharValueUpdate(BLEDevice central, BLECharacteristic characteristic);
extern void initBLE(void);

//All defined functions
extern void send_image();
extern int read_voltage();
extern void led2_task();

#endif
