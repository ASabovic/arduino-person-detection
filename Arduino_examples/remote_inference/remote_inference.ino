/*
This script enables different things required for enabling data for remote inferencing:

1.) It enables part where data is captured, read, decoded and processed. 
2.) Once this part is done, data (captured image) is sent to the IoT gateway via BLE for remote inferencing. 
3.) Once the remote inference task is done on the gateway (or Cloud), data is sent back and received on the Arduino board. 
4.) Finally, the inference result is confirmed by briefly turning on the appropriate LED (i.e., green if person is detected on the image and red if it is not)

For this example, we integrated the task scheduler, which is described in more detail here -> 
https://www.researchgate.net/publication/361729293_An_Energy-Aware_Task_Scheduler_for_Energy_Harvesting_Battery-Less_IoT_Devices
The energy-aware task scheduler is able to select tasks based on their priorities, dependencies and constraints. These tasks will be executed only if there its required 
voltage threshold (if there is enough energy for their execution) is reached. Otherwise, the device will go into sleep state, wait the predefined period of time, and check again.
Once the enough energy is collected in the capacitor, the scheduled application task can be safely executed. 

*/
#include "application.h"

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
        delay(2000);
        break;

    case F_image:
        send_image();
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
        
        case lowerorequal:

          TSK_LIST[i].start_time = get_task_ti(selected_task)->execution_time;
          TSK_LIST[i].task_id = curr_child->task_id;

          OCC_LIST[i] = 1;
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

      if(get_task_ti(&(TSK_LIST[loc]))->task_name == F_image)
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

int read_voltage()
{
    digitalWrite(4, HIGH);
    int sensorValue = analogRead(A0);
    int vol = (sensorValue * 3600 / 1024) * 2.6; 
    digitalWrite(4, LOW);
    return vol;
}

void camera_task()
{
    digitalWrite(2, HIGH);
    initialize_camera(error_reporter, kNumCols, kNumRows, kNumChannels, input->data.int8);
    digitalWrite(2, LOW);
}

//Setup function 
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
