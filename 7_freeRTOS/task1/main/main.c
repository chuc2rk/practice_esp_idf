/*
  copy from https://kotyara12.ru/iot/esp32_tasks2/

  Assume Case: 
  You have a simple task that periodically measures data 
  (like temperature or sensor readings) and outputs the result. 
  The task should run independently without external input, 
  maintaining a constant interval between executions. 
  You’re not concerned about minor timing inaccuracies or external delays.

  How to Solve: 
  Implement a FreeRTOS task with a basic infinite loop. 
  Use the vTaskDelay() function to pause the task between executions. 
  This delay allows the task to sleep for a specified time while freeing up the CPU for other tasks.
*/


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_random.h"
#include "esp_log.h"

const char* TAG = "TASK1";

#define APP_TASK_STACK_SIZE   4096
#define APP_TASK_PRIORITY     5
#define APP_TASK_CORE         1

// Static buffer for task service data
static StaticTask_t xTaskBuffer;
// Static buffer for task stack
static StackType_t xStack[APP_TASK_STACK_SIZE];

float readSensor1()
{
  return (float)esp_random()/100000000.0;
}

void app_task_exec(void *pvParameters)
{
  uint8_t i = 0;
  while (true)
  {
    //Read data from sensor
    float value = readSensor1();

    // Print message to terminal
    ESP_LOGI(TAG, "Read sensor: value = %.1f", value);

    //delay 10s
    vTaskDelay(pdMS_TO_TICKS(10000));
  }

  // We should never get here. But if "something went wrong" - 
  // we still need to delete the task from memory
  vTaskDelete(NULL);
}

void app_task_start()
{
  // Start a task with static memory allocation
  TaskHandle_t xHandle = xTaskCreateStatic(app_task_exec, "app_task1", APP_TASK_STACK_SIZE, NULL, APP_TASK_PRIORITY, xStack, &xTaskBuffer);
                      

  if (xHandle == NULL) {
    ESP_LOGE(TAG, "Failed to task create");    
  } else {
    ESP_LOGI(TAG, "Task started");
  }
}

void app_main(void)
{
  app_task_start();
}
  