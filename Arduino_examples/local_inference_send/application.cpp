/*
This is the script that defines all application tasks and their parameters. These parameter values are very important for the task scheduler implementation. 
The execution time can be measure using the Power Profiler Kit 2 -> https://www.nordicsemi.com/Products/Development-hardware/Power-Profiler-Kit-2
The task priority and first task values depend on the application and requirements we set before running the code. The task priority value dictates which application task will be 
executed first. The first task parameter is important as it defines the starting task(s) (parent tasks) of the application. 
Finally, the required voltage thresholds can be calculated based on equations presented in -> 
https://www.researchgate.net/publication/361729293_An_Energy-Aware_Task_Scheduler_for_Energy_Harvesting_Battery-Less_IoT_Devices (Equation 1, page 10)
Also, this script contains definition of all parameters for the BLE communication, BLE initialization functions as well as function that enables local inference results to 
IoT gateway via BLE.
*/

#include "application.h"
#include <ArduinoBLE.h>

struct Task application[TASK_AMOUNT];

//Parameters used for the BLE communication
char* uuidOftxChar = "UUID_TX_CHAR"; //example -> char* uuidOftxChar = "2d2F88c4-f244-5a80-21f1-ee0224e80658"
char* uuidOfService = "UUID_SERVICE"; //example -> char* uuidOfService = "180F";
char* nameofPeripheral = "NAME_PERIPHERAL"; // example -> char* nameofPeripheral = "BLESender";
int RX_BUFFER_SIZE = 220;
bool RX_BUFFER_FIXED_LENGTH = false;
char prediction_status[]="";
unsigned long prevNow = 0;
bool wasConnected = false;

int8_t person_score;
int8_t no_person_score;
int prediction;

BLEService bleService(uuidOfService);
BLECharacteristic txChar(uuidOftxChar, BLEIndicate, RX_BUFFER_SIZE, RX_BUFFER_FIXED_LENGTH);

void app_init()
{

    application[F_camera].task_name = F_camera;
    application[F_camera].execution_time = 1017;
    application[F_camera].task_priority = 3;
    application[F_camera].first_task = 1;
    application[F_camera].children = 2;
    application[F_camera].required_voltage = 4400;

    application[F_camera].child[0].task_id = F_camera;
    application[F_camera].child[0].constraint_value = 10000;
    application[F_camera].child[0].type = wait;

    application[F_camera].child[1].task_id = F_local;
    application[F_camera].child[1].constraint_value = 1;
    application[F_camera].child[1].type = avb;

    application[F_local].task_name = F_local;
    application[F_local].execution_time = 648;
    application[F_local].task_priority = 7;
    application[F_local].first_task = 0;
    application[F_local].children = 2;
    application[F_local].required_voltage = 4000;

    application[F_local].child[0].task_id = F_led;
    application[F_local].child[0].constraint_value = 0;
    application[F_local].child[0].type = nocondition;

    application[F_local].child[1].task_id = F_results;
    application[F_local].child[1].constraint_value = 1;
    application[F_local].child[1].type = avb;

    application[F_results].task_name = F_results;
    application[F_results].execution_time = 4334;
    application[F_results].task_priority = 5;
    application[F_results].first_task = 0;
    application[F_results].children = 0;
    application[F_results].required_voltage = 4000;

    application[F_led].task_name = F_led;
    application[F_led].execution_time = 502;
    application[F_led].task_priority = 6;
    application[F_led].first_task = 0;
    application[F_led].children = 0;
    application[F_led].required_voltage = 3957; 
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

void initBLE(void){
  BLE.begin();
  BLE.setLocalName(nameofPeripheral);
  BLE.setAdvertisedService(bleService);
  bleService.addCharacteristic(txChar); 
  BLE.addService(bleService);
  BLE.setEventHandler(BLEConnected, blePeripheralConnectHandler);
  BLE.setEventHandler(BLEDisconnected, blePeripheralDisconnectHandler);
  BLE.advertise(); 
}

void send_results()
{
    initBLE();
    while(wasConnected == false)
    {
        BLE.poll();
        long now = millis();
        if(now-prevNow >= 4000)
        {
            prevNow = now;
            if(BLE.connected() && wasConnected == false)
            {
                if (person_score > no_person_score)
                {
                    prediction = 1;
                    int j = 0;
                    sprintf(prediction_status, "%d", prediction);
                    while(j<1)
                    {
                        txChar.writeValue(prediction_status+j);
                        j+=1;
                    }
                }
                else
                {
                    prediction = 0;
                    int k = 0;
                    sprintf(prediction_status, "%d", prediction);
                    while(k<1)
                    {
                        txChar.writeValue(prediction_status+k);
                        k+=1;
                    }
                }

                BLE.disconnect();
                BLE.end();
                wasConnected = true;
            }
        }
    }

    wasConnected = false;
}
