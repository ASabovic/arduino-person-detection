/*
This is the script that defines all application tasks and their parameters. These parameter values are very important for the task scheduler implementation. 
The execution time can be measure using the Power Profiler Kit 2 -> https://www.nordicsemi.com/Products/Development-hardware/Power-Profiler-Kit-2
The task priority and first task values depend on the application and requirements we set before running the code. The task priority value dictates which application task will be 
executed first. The first task parameter is important as it defines the starting task(s) (parent tasks) of the application. 
Finally, the required voltage thresholds can be calculated based on equations presented in -> 
https://www.researchgate.net/publication/361729293_An_Energy-Aware_Task_Scheduler_for_Energy_Harvesting_Battery-Less_IoT_Devices (Equation 1, page 10)
*/

#include "application.h"
#include <ArduinoBLE.h>

struct Task application[TASK_AMOUNT];

int8_t person_score;
int8_t no_person_score;

void app_init()
{

    //Camera task 
    application[F_camera].task_name = F_camera;
    application[F_camera].execution_time = 1017;
    application[F_camera].task_priority = 3;
    application[F_camera].first_task = 1;
    application[F_camera].children = 2;
    application[F_camera].required_voltage = 4161;

    //Camera child task
    application[F_camera].child[0].task_id = F_camera;
    application[F_camera].child[0].constraint_value = 10000;
    application[F_camera].child[0].type = wait;

     //BLE image transfer child task
    application[F_camera].child[1].task_id = F_local;
    application[F_camera].child[1].constraint_value = 1;
    application[F_camera].child[1].type = avb;
    
    //Local inference task
    application[F_local].task_name = F_local;
    application[F_local].execution_time = 648;
    application[F_local].task_priority = 7;
    application[F_local].first_task = 0;
    application[F_local].children = 1;
    application[F_local].required_voltage = 3957;

    //LED child task
    application[F_local].child[0].task_id = F_led;
    application[F_local].child[0].constraint_value = 0;
    application[F_local].child[0].type = nocondition;

    //LED task
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
