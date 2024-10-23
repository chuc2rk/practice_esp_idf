/*
  copy from https://kotyara12.ru/iot/esp32_tasks2/

  Assume Case: 
  You need a periodic task but require more accurate and uniform timing. 
  The task execution might take longer sometimes (due to network delays, heavy processing, etc.), 
  which could cause timing inaccuracies in a fixed delay system.

  How to Solve: 
  Use vTaskDelayUntil(), which ensures that the task runs exactly every 10 seconds, 
  regardless of how long the task itself takes to execute. 
  This method ensures a constant period between task executions.
*/


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_random.h"
#include "esp_log.h"

const char* TAG = "TASK2";

#define APP_TASK_STACK_SIZE   4096
#define APP_TASK_PRIORITY     5
//#define APP_TASK_CORE         1

// Static buffer for task service data
static StaticTask_t xTaskBuffer;
// Static buffer for task stack
static StackType_t xStack[APP_TASK_STACK_SIZE];

float readSensor1()
{
  return (float)esp_random()/100000000.0;
}


void sendHttpRequest(const float value)
{
  //assume time delay send http request
  uint32_t delay = esp_random() / 100000000.0;
  ESP_LOGI(TAG, "Send data, delay %u ms", (uint)delay);
  vTaskDelay(pdMS_TO_TICKS(delay));
}


void app_task_exec(void *pvParameters)
{
  uint8_t i = 0;

  TickType_t prevWakeup = 0;

  while (true)
  {
    //Read data from sensor
    float value = readSensor1();

    // Print message to terminal
    ESP_LOGI(TAG, "Read sensor: value = %.1f", value);

    // Send data to the server once every 30 seconds
    i++;
    if (i >= 3) {
      i = 0;
      sendHttpRequest(value);
    }

    //delay 10s
    
    vTaskDelayUntil(&prevWakeup, pdMS_TO_TICKS(10000)) 
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
  