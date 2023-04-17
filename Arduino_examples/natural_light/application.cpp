/*
This is the script that defines all application tasks and their parameters. These parameter values are very important for the task scheduler implementation. 
The execution time can be measure using the Power Profiler Kit 2 -> https://www.nordicsemi.com/Products/Development-hardware/Power-Profiler-Kit-2
The task priority and first task values depend on the application and requirements we set before running the code. The task priority value dictates which application task will be 
executed first. The first task parameter is important as it defines the starting task(s) (parent tasks) of the application. 
Finally, the required voltage thresholds can be calculated based on equations presented in -> 
https://www.researchgate.net/publication/361729293_An_Energy-Aware_Task_Scheduler_for_Energy_Harvesting_Battery-Less_IoT_Devices (Equation 1, page 10)
Also, this script contains definition of all parameters for the BLE communication, BLE initialization functions as well as function that enables image transfer to IoT gateway 
via BLE.
*/
#include "application.h"
#include <ArduinoBLE.h>

struct Task application[TASK_AMOUNT];

//Parameters used for the BLE communication
char* uuidOftxChar = "UUID_TX_CHAR"; // example -> char* uuidOftxChar = "2d2F88c4-f244-5a80-21f1-ee0224e80658"
char* uuidOfrxChar = "UUID_RX_CHAR"; // example -> char* uuidOfrxChar = "00002A3D-0000-1000-8000-00805f9b34fb"
char* uuidOfService = "UUID_SERVICE"; // example -> char* uuidOfService = "180F"
char* nameofPeripheral = "NAME_PERIPHERAL"; // example -> char* nameofPeripheral = "BLESender"
int RX_BUFFER_SIZE = 220;
bool RX_BUFFER_FIXED_LENGTH = false;
char prediction_status[]="";
unsigned long prevNow = 0;
bool wasConnected = false;
byte tmp[256];
BLEService bleService(uuidOfService);
BLECharacteristic txChar(uuidOftxChar, BLEIndicate, RX_BUFFER_SIZE, RX_BUFFER_FIXED_LENGTH);
BLECharacteristic rxChar(uuidOfrxChar, BLEWriteWithoutResponse | BLEWrite, RX_BUFFER_SIZE, RX_BUFFER_FIXED_LENGTH);

int8_t person_score;
int8_t no_person_score;

void app_init()
{
    application[F_camera].task_name = F_camera;
    application[F_camera].execution_time = 1049;
    application[F_camera].task_priority = 3;
    application[F_camera].first_task = 1;
    application[F_camera].children = 3;
    application[F_camera].required_voltage = 4060;

    application[F_camera].child[0].task_id = F_camera;
    application[F_camera].child[0].constraint_value = 10000;
    application[F_camera].child[0].type = wait;

    application[F_camera].child[1].task_id = F_image;
    application[F_camera].child[1].constraint_value = 3;
    application[F_camera].child[1].type = lowerorequal;

    application[F_camera].child[2].task_id = F_local;
    application[F_camera].child[2].constraint_value = 1;
    application[F_camera].child[2].type = avb;

    application[F_image].task_name = F_image;
    application[F_image].execution_time = 8660;
    application[F_image].task_priority = 10;
    application[F_image].first_task = 0;
    application[F_image].children = 1;
    application[F_image].required_voltage = 4000;

    application[F_image].child[0].task_id = F_led2;
    application[F_image].child[0].constraint_value = 0;
    application[F_image].child[0].type = nocondition;

    application[F_led2].task_name = F_led2;
    application[F_led2].execution_time = 510;
    application[F_led2].task_priority = 5;
    application[F_led2].first_task = 0;
    application[F_led2].children = 0;
    application[F_led2].required_voltage = 4000;

    application[F_local].task_name = F_local;
    application[F_local].execution_time = 1148;
    application[F_local].task_priority = 8;
    application[F_local].first_task = 0;
    application[F_local].children = 1;
    application[F_local].required_voltage = 3960;

    application[F_local].child[0].task_id = F_led;
    application[F_local].child[0].constraint_value = 0;
    application[F_local].child[0].type = nocondition;

    application[F_led].task_name = F_led;
    application[F_led].execution_time = 510;
    application[F_led].task_priority = 5;
    application[F_led].first_task = 0;
    application[F_led].children = 0;
    application[F_led].required_voltage = 3960; 
}

struct Task *get_task_ti(struct TaskInstance *ti)
{
    return &(application[ti->task_id]);
}

struct Task *get_task_e(struct Edge *e)
{
    return &(application[e->task_id]);
}

void blePeripheralConnectHandler(BLEDevice central)
{
  //empty callback
}

void blePeripheralDisconnectHandler(BLEDevice central)
{
  //empty callback
}

void onRxCharValueUpdate(BLEDevice central, BLECharacteristic characteristic) {
  
  int dataLength = rxChar.readValue(tmp, 256);
}

void initBLE(void){
  BLE.begin();
  BLE.setLocalName(nameofPeripheral);
  BLE.setAdvertisedService(bleService);
  bleService.addCharacteristic(rxChar);
  bleService.addCharacteristic(txChar); 
  BLE.addService(bleService);
  BLE.setEventHandler(BLEConnected, blePeripheralConnectHandler);
  BLE.setEventHandler(BLEDisconnected, blePeripheralDisconnectHandler);
  rxChar.setEventHandler(BLEWritten, onRxCharValueUpdate);
  BLE.advertise(); 
}

void send_image()
{
    initBLE();
    while(wasConnected == false)
    {
        BLE.poll();
        long now = millis();
        if(now-prevNow >= 5000)
        {
            prevNow = now;
            if(BLE.connected() && wasConnected == false)
            {
                if(jpeg_length == 3080)
                {
                  int i=0;
                  while(i<jpeg_length)
                  {
                    txChar.writeValue(jpeg_buffer+i, 220);
                    i+=220;
                  }
                  wasConnected = true;
                }
                else
                {
                  int i=0;
                  while(i<jpeg_length)
                  {
                    txChar.writeValue(jpeg_buffer+i, 220);
                    i+=220;
                  }
                  int k = 7;
                  sprintf(prediction_status, "%d", k);
                  int j=0;
                  while(j<1)
                  {
                    txChar.writeValue(prediction_status+j);
                    j+=1;
                  }
                  wasConnected = true;
                }

                delay(1000);
                
                BLE.disconnect();
                BLE.end();
            }
        }
    }

    wasConnected = false;
}

void led2_task()
{
  pinMode(LEDR, OUTPUT);
  pinMode(LEDG, OUTPUT);

  if (char(tmp[0])=='5' || char(tmp[0])=='6' || char(tmp[0])=='7' || char(tmp[0])=='8' || char(tmp[0])=='9') {
    digitalWrite(LEDG, LOW);
    digitalWrite(LEDR, HIGH);
    delay(500);
    digitalWrite(LEDG, HIGH);
  } else {
    digitalWrite(LEDG, HIGH);
    digitalWrite(LEDR, LOW);
    delay(500);
    digitalWrite(LEDR, HIGH);
  }
}
