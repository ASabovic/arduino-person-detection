# arduino-person-detection
This repository contains code that enables the person detection on the Arduino Nano 33 BLE board. Also, there are scripts that enable the BLE connection for data transfer (captured image, local/remote inference results) between the Arduino board and the IoT gateway (using Bleak library), where the heavy-weight ML model is running.

There are three main examples: 

1. Local inference - in this example the decision can only be made locally on the Arduino Nano 33 BLE board. After the image is captured using the Arducam ov2640 Mini 2MP Plus camera module, it will be read, decoded and processed, it will be forwarded as a input to tiny ML model running on the board. Once the inference task is done, the final result is confirmed by briefly turning on the appropriate LED (i.e., green if the person is detected on the image and red if it is not).

2. Local inference + sending local inference result to the gateway (PC in this case) - this is example is very similar to the first one, except after the local inference result is confirmed on the board, it will be sent to the gateway or Cloud depending on the implementation. Once the result is received on the IoT gateway (or Cloud), it will be displayed.

3. Remote inference - in this example the decision is made remotely using the heavy-weight ML model. Once the image is captured, read, decoded and processed, it will be sent via BLE to the IoT gateway (or Cloud) for remote inferencing. The BLE connection and communication is established using two libraries: 

a. Arduino BLE library that enables BLE features on the Arduino board.
b. Bleak library that enables our IoT gateway to find the Arduino device, connect with it, and exchange data.

Once the image is received, it will be edited and forwarded as a input to heavy-weight ML model for remote inferencing. When this part is finished, the inference result will be sent back to the Arduino board and confirmed in the same way explained above (in first two examples). The implementation on the gateway side can be found in separate example (Gateway_examples).

There is the last available example (natural_light) that shows the implementation of both inference strategy at same time on the Arduino board. More information about this example and the way how the more optimal solution between these two is selected, can be found in comments.

# Required Arduino libraries 
This is the list of required Arduino libraries that should be included in order to run this project successfully: 

1. ArduCAM -> https://github.com/ArduCAM/Arduino/tree/master/ArduCAM

Edit the memorysaver header file (located in https://github.com/ArduCAM/Arduino/blob/master/ArduCAM/memorysaver.h), ensuring the right camera is selected.

2. Arduino JPEGDecoder -> https://github.com/Bodmer/JPEGDecoder (or install it from Arduino IDE Library Manager)

It is very important for this library to edit the JPEGDecoder User_Config header file (located in https://github.com/Bodmer/JPEGDecoder/blob/master/src/User_Config.h). The default SD Card library should be removed (or commented).

3. Arduino BLE -> https://github.com/arduino-libraries/ArduinoBLE (or https://www.arduino.cc/reference/en/libraries/arduinoble/)
4. Arduino TensorFlow Lite -> https://github.com/tensorflow/tflite-micro-arduino-examples

# Required Python libraries

This is the list of required Python libraries that should be included in this project (in the script running on our PC/IoT gateway):

1. Bleak - https://github.com/hbldh/bleak
2. Tensorflow - https://pypi.org/project/tensorflow/