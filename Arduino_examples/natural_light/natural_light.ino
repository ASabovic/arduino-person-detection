/* Copyright 2019 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

/*
This script includes two main parts: 

1.) Local inference - inferencing task can be done locally on the Arduino board and results are confirmed by briefly turning on the appropriate LED (i.e., green if person is
detected on the captured image and red if it is not)
2.) Remote inference - once the image is captured and completely processed, it will be sent to the gateway (or Cloud) for remote inferencing. When the remote inference task is 
completed, the final results are sent back to the device, received, and confirmed by briefly turning on the appropriate LED (as explained above).

The decision which of these two inference strategies will be selected and executed is done by our optimization algorithm
(more information can be found here: https://www.researchgate.net/publication/368790518_Towards_energy-aware_tinyML_on_battery-less_IoT_devices).

Based on the defined deadline, until which inference results have to been confirmed on the Arduino board, our optimization algorithm will first check if any of available inference
strategy paths can be done within it. There are multiple cases:

1.) If both inference paths, local and remote, can be done within defined deadline, the remote inference strategy will be selected as optimal one (in this case the longer execution 
time and higher current consumption will be traded off for more accurate results)
2.) As remote inference path needs more time to be executed, it is obvious, that in some cases (when the harvesting current is lower), only the local inference strategy can be 
done within the deadline. In this case, the local inference path is selected as optimal one. 
3.) If none of available inference strategies can be done within the set deadline, the captured image will be removed from memory, and camera task will be repeated again in order 
to get new (fresh) data for next cycle.
*/

#include "application.h"
#include <math.h> 

int E = 3.3;
int C = 1500;
int Ih = 0;
int Req = 2946;

//Globals used for compatibility with Arduino-style sketches
namespace{
tflite::ErrorReporter* error_reporter = nullptr;
const tflite::Model* model = nullptr;
tflite::MicroInterpreter* interpreter = nullptr;
TfLiteTensor* input = nullptr;   

//An area of memory used for input, output, and intermediate arrays
constexpr int kTensorArenaSize = 136*1024;
static uint8_t tensor_arena[kTensorArenaSize];
}

#define MAX_TASK_AMOUNT 10
struct TaskInstance TSK_LIST[MAX_TASK_AMOUNT];
int OCC_LIST[MAX_TASK_AMOUNT] = {0};
int V_0;
int V_req;
int t_deadline = 40000;
int t_local;
int t_remote;
//t_local = 32.4;
//t_remote = 15.7;

int main(void){

  init();
  initVariant();

  NRF_UART0->TASKS_STOPTX = 1;
  NRF_UART0->TASKS_STOPRX = 1;
  NRF_UART0->ENABLE = 0;
  *(volatile uint32_t *)0x40002FFC = 0;
  *(volatile uint32_t *)0x40002FFC; 
  *(volatile uint32_t *)0x40002FFC = 0;

  setup();
  for(;;){
    loop();
  }
  return 0;
}

//Execute function 
void execute(struct TaskInstance *selected_task)
{
    switch (get_task_ti(selected_task)->task_name)
    {
    case F_camera:
        camera_task();
        //delay(2000);
        break;

    case F_image:
        send_image();
        break;

    case F_local:
        inference();
        break;
    
    case F_led:
        led_task();
        break;
        
    case F_led2:
        led2_task();
        break;
        
    default:
        break;
    }
}

void low_power()
{
  digitalWrite(LED_PWR, LOW);
  digitalWrite(PIN_ENABLE_SENSORS_3V3, LOW);
  digitalWrite(PIN_ENABLE_I2C_PULLUP, LOW);
}

void getfirstTask()
{
  struct TaskInstance temporarily;
  int position = 0;

  for(int i = 0; i < (sizeof(application) / sizeof(application[0])); i++)
  {
    if(application[i].first_task == 1)
    {
      temporarily.start_time = 0;
      temporarily.task_id = i;
      TSK_LIST[position] = temporarily;

      OCC_LIST[position] = 1;

      position++;
    }
  }
}

void setupScheduler()
{
  app_init();
  getfirstTask();
}

void removeTask(int active_task)
{
  OCC_LIST[active_task] = 0;
}

void addTask(int active_task)
{
  int m = 0;
  struct TaskInstance *selected_task = &(TSK_LIST[active_task]);
  for(int i = 0; i < MAX_TASK_AMOUNT; i++)
  {
    if(!OCC_LIST[i])
    {
      struct Edge *curr_child;
      do
      {
        if(m == get_task_ti(selected_task)->children)
          return;
        curr_child = &(get_task_ti(selected_task)->child[m]);
        m++;
      }while (get_task_e(curr_child)->task_priority < 0 || get_task_e(curr_child)->task_priority > 10);
      
      switch (curr_child->type)
      {
        case nocondition:

          TSK_LIST[i].start_time = get_task_ti(selected_task)->execution_time;
          TSK_LIST[i].task_id = curr_child->task_id;

          OCC_LIST[i] = 1;
          break;

        case wait:

          TSK_LIST[i].start_time = get_task_ti(selected_task)->execution_time + curr_child->constraint_value;
          TSK_LIST[i].task_id = curr_child->task_id;

          OCC_LIST[i] = 1;
          break;
        
        case avb:
        
          V_0 = read_voltage();
          V_req = get_task_ti(&(TSK_LIST[i]))->required_voltage;          
          t_local = time_function(Ih, Req, C, V_0, V_req);
          t_local = t_local+1148+4000;
          
          if(t_local < 0 )
          {
            t_local = 0;
          }
          
          if(t_local <= t_deadline)
          {
            TSK_LIST[i].start_time = get_task_ti(selected_task)->execution_time;
            TSK_LIST[i].task_id = curr_child->task_id;
            OCC_LIST[i] = 1;
          }
          else
          {
            //
          }
          break;

        case lowerorequal:

          V_0 = read_voltage();
          V_req = get_task_ti(&(TSK_LIST[i]))->required_voltage;
          
          t_remote = time_function(Ih, Req, C, V_0, V_req);
          t_remote = t_remote + 8660 + 4000;

          if(t_remote < 0 )
          {
            t_remote = 0;
          }

          if(t_remote <= t_deadline)
          {
            TSK_LIST[i].start_time = get_task_ti(selected_task)->execution_time;
            TSK_LIST[i].task_id = curr_child->task_id;
            OCC_LIST[i] = 1;
          }
          else
          {
            //empty
          }
          break;

        default:
          break;
        }   
    }
  }
}

int select_task()
{
  int loc = -1;
  int max = -1;

  for (int i = 0; i < MAX_TASK_AMOUNT; i++)
  {
    if (OCC_LIST[i] == 1)
    {
      if (max < get_task_ti(&(TSK_LIST[i]))->task_priority)
      {
        loc = i;
        max = get_task_ti(&(TSK_LIST[i]))->task_priority;
      }
    }
  }

  return loc;
}

unsigned int get_time()
{
  unsigned int time = 240000; // Max time of 4 min

  for (int i = 0; i < MAX_TASK_AMOUNT; i++)
  {
    if (OCC_LIST[i])
    {
      if (time < 0 || time > TSK_LIST[i].start_time)
      {
        time = TSK_LIST[i].start_time;
      }
    }
  }
  for (int i = 0; i < MAX_TASK_AMOUNT; i++)
  {
    if (OCC_LIST[i])
    {
      TSK_LIST[i].start_time -= time;
    }
  }
  
  return time;
}

void scheduleTask()
{
  int loc = select_task();
  if (loc != -1)
  {
    V_0 = read_voltage();
    V_req = get_task_ti(&(TSK_LIST[loc]))->required_voltage;
    
    if(get_task_ti(&(TSK_LIST[loc]))->task_name == F_local)
    {
      while(V_0 < V_req)
      {
        delay(3000);
        V_0 = read_voltage();
      }
      execute(&TSK_LIST[loc]);
      addTask(loc);
      removeTask(loc);
      int b = 0;
      while(b < 1)
      {
        int loc = select_task();
        execute(&TSK_LIST[loc]);
        addTask(loc);
        removeTask(loc);
        b+=1;
      }
    }
    else if(get_task_ti(&(TSK_LIST[loc]))->task_name == F_image)
    {
      while(V_0 < V_req)
      {
        delay(3000);
        V_0 = read_voltage();
      }
      execute(&TSK_LIST[loc]);
      addTask(loc);
      removeTask(loc);
      int s = 0;
      while(s < 1)
      {
        int loc = select_task();
        if(get_task_ti(&(TSK_LIST[loc]))->task_name == F_local)
        {
          removeTask(loc);
        }
        else
        {
          execute(&TSK_LIST[loc]);
          addTask(loc);
          removeTask(loc);
          s+=1;
        }
      }
    }
    else
    {
      while(V_0 < V_req)
      {
        delay(3000);
        V_0 = read_voltage();
      }
      execute(&TSK_LIST[loc]);
      addTask(loc);
      removeTask(loc);
    }
  }
}

int time_function(int Ih, int Req, int C, int V2, int V1)
{
  float t = (-Req*C*log((V2-Ih*Req)/(V1-Ih*Req)));
  return t;
}

int read_voltage()
{
    digitalWrite(4, HIGH);
    int sensorValue = analogRead(A0);
    int vol = (sensorValue * 3600 / 1024) * 2.62;
    digitalWrite(4, LOW);
    return vol;
}

void camera_task()
{
    digitalWrite(2, HIGH);
    initialize_camera(error_reporter, kNumCols, kNumRows, kNumChannels, input->data.int8);
    digitalWrite(2, LOW);
}

void inference()
{
    if(kTfLiteOk != interpreter->Invoke())
    {
      TF_LITE_REPORT_ERROR(error_reporter, "Invoke failed!");
    }
    TfLiteTensor* output = interpreter->output(0);
    person_score = output->data.uint8[kPersonIndex];
    no_person_score = output->data.uint8[kNotAPersonIndex]; 
}

void led_task()
{
  pinMode(LEDR, OUTPUT);
  pinMode(LEDG, OUTPUT);

  if (person_score > no_person_score) {
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

void setup() 
{
//  Serial.begin(9600);
//  while(!Serial);
  low_power();
  pinMode(2, OUTPUT);
  digitalWrite(2, LOW);
  pinMode(4, OUTPUT);
  digitalWrite(4, LOW);

  tflite::MicroErrorReporter micro_error_reporter;
  error_reporter = &micro_error_reporter;
  //Map the model into a usable data structure. This is a very lightweight operation!
  model = tflite::GetModel(g_person_detect_model_data);
  if(model->version() != TFLITE_SCHEMA_VERSION){
    TF_LITE_REPORT_ERROR(error_reporter, "Model provided is schema version %d not equal "
                          "to supported version %d.", model->version(), TFLITE_SCHEMA_VERSION);
    return;
  }
  static tflite::MicroMutableOpResolver<5> micro_op_resolver;
  micro_op_resolver.AddAveragePool2D();
  micro_op_resolver.AddConv2D();
  micro_op_resolver.AddDepthwiseConv2D();
  micro_op_resolver.AddReshape();
  micro_op_resolver.AddSoftmax();

  //Interpreter used to run the model 
  static tflite::MicroInterpreter static_interpreter(model, micro_op_resolver, tensor_arena, kTensorArenaSize, error_reporter);
  interpreter = &static_interpreter;

  //Allocate the memory from the tensor_arena for the model's tensors.
  TfLiteStatus allocate_status = interpreter->AllocateTensors();
  if(allocate_status != kTfLiteOk){
    TF_LITE_REPORT_ERROR(error_reporter, "AllocateTensors () failed!");
    return;
  }

  //Information about the memory area used for the model's input 
  input = interpreter->input(0);
  setupScheduler();
}

void loop()
{
  scheduleTask();
  unsigned int next_time = fmax(get_time(), 5);
  delay(next_time);
}
