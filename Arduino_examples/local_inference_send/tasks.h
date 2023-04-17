#ifndef TENSORFLOW_LITE_MICRO_EXAMPLES_PERSON_DETECTION_TASKS_H_
#define TENSORFLOW_LITE_MICRO_EXAMPLES_PERSON_DETECTION_TASKS_H_

#define MAX_CHILD_EDGES 2
typedef enum constraintType
{
    nocondition,
    wait,
    avb
};

struct Edge
{
    enum constraintType type;
    int constraint_value;
    unsigned int task_id;
};

struct Task
{
    char task_name;
    int task_energy;
    int execution_time;
    int task_priority;
    unsigned int children;
    struct Edge child[MAX_CHILD_EDGES];
    int first_task;
    int required_voltage;
};

struct TaskInstance
{
    int start_time;
    unsigned int task_id;
};

#endif
