/*
 * copy from https://kotyara12.ru/iot/esp32_tasks2/
 *
 * Assume Case: You need a task that runs periodically but must also respond to external events (e.g., button presses on GPIO). 
 * The task should keep measuring data at regular intervals but handle any external stimuli in real-time.
 * 
 * How to Solve: Use an event group to handle multiple external triggers like GPIO interrupts. 
 * The task will either run its normal measurement cycle or react to external events, depending on what occurs first (the timer or an interrupt). 
 * This allows multitasking without missing external signals.
*/


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_random.h"
#include "esp_log.h"
#include "dyn_strings.h"

const char* TAG = "TASK4";

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

void insertStringIntoQueue(char* text)
{
  xQueueSend(data_queue, &text, portMAX_DELAY);
}

void sendHttpRequest(char* text)
{
  //assume time delay send http request
  uint32_t delay = esp_random() / 100000000.0;
  ESP_LOGI(TAG, "Send data, delay %u ms", (uint)delay);
  vTaskDelay(pdMS_TO_TICKS(delay));
}


void app_task_exec(void *pvParameters)
{
  char* text = NULL;

  while (true)
  {
    while (xQueueReceive(data_queue, &text, portMAX_DELAY) == pdPASS)
    {
      if (text !=NULL) {
        // Output a message to the terminal
        ESP_LOGI(TAG, "Value recieved: %s", text);
        sendHttpRequest(text);
        free(text);
      }      
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
  TaskHandle_t xHandle = xTaskCreateStatic(app_task_exec, "app_task4", APP_TASK_STACK_SIZE, NULL, APP_TASK_PRIORITY, xStack, &xTaskBuffer);
                    
  if (xHandle == NULL) {
    ESP_LOGE(TAG, "Failed to task create");    
  } else {
     
    ESP_LOGI(TAG, "Task started");
  }
}

void app_main(void)
{
  app_task_start();

  uint32_t i = 0;

  // Infinite loop of data transfer to task queue
  while (true)
  {
    i++;
    char* text = malloc_stringf("String value: %i", i);
    insertStringIntoQueue(text);
    vTaskDelay(pdMS_TO_TICKS(10000));
  }
}
