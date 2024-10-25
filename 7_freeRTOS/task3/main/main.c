/*
  copy from https://kotyara12.ru/iot/esp32_tasks2/

  Assume Case: You have a task that should only execute when data is available from an external source 
  (like sensor data or network packets). The task should stay idle when no data is available, reducing CPU usage.

  How to Solve: Use a FreeRTOS queue to pass data from one part of the program to another. 
  The task will wait (block) on the queue, only waking up to process data when it’s available. 
  This approach avoids busy-waiting and conserves system resources.
*/


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_random.h"
#include "esp_log.h"

const char* TAG = "TASK3";

#define APP_TASK_STACK_SIZE   4096
#define APP_TASK_PRIORITY     5
//#define APP_TASK_CORE         1
#define APP_QUEUE_LENGTH  10
#define APP_QUEUE_ITEM_SIZE sizeof(float)

// Static buffer for task service data
static StaticTask_t xTaskBuffer;
//static StaticQueue_t queue_storage_buffer;
static StaticQueue_t queue_buffer;

// Static buffer for task stack
static StackType_t xStack[APP_TASK_STACK_SIZE];

static QueueHandle_t data_queue = NULL;
static uint8_t queue_storage[APP_QUEUE_LENGTH * APP_QUEUE_ITEM_SIZE];

void insert_value_into_queue(const float value)
{
  xQueueSend(data_queue, &value, portMAX_DELAY);
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
  float value = 0.0;

  while (true)
  {
    while (xQueueReceive(data_queue, &value, portMAX_DELAY) == pdPASS)
    {
      // Output a message to the terminal
      ESP_LOGI(TAG, "Value recieved: %.1f", value);
      sendHttpRequest(value);
    }   
  }

  // We should never get here. But if "something went wrong" - 
  // we still need to delete the task from memory
  vTaskDelete(NULL);
}

void app_task_start()
{
  // first create an input queue first, if not program crash
  data_queue = xQueueCreateStatic(APP_QUEUE_LENGTH, APP_QUEUE_ITEM_SIZE, &queue_storage[0], &queue_buffer);   
  if (data_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create queue");        
    }

  // Start a task with static memory allocation
  TaskHandle_t xHandle = xTaskCreateStatic(app_task_exec, "app_task3", APP_TASK_STACK_SIZE, NULL, APP_TASK_PRIORITY, xStack, &xTaskBuffer);
                    
  if (xHandle == NULL) {
    ESP_LOGE(TAG, "Failed to task create");    
  } else {
     
    ESP_LOGI(TAG, "Task started");
  }
}

void app_main(void)
{
  app_task_start();

  // Infinite loop of data transfer to task queue
  while (true)
  {
    insert_value_into_queue((float)esp_random() / 100000000.0);
    vTaskDelay(pdMS_TO_TICKS(10000));
  }
}
