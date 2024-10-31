/*
  copy from https://kotyara12.ru/iot/esp32_tasks2/

  Assume Case: You need to handle data of variable length (e.g., strings) and transmit it between tasks using a queue. 
  Strings can vary in size and must be managed using dynamic memory allocation, with proper memory deallocation to avoid leaks.

  How to Solve: Allocate memory dynamically for variable-length data, and pass a pointer through the queue. 
  Once the task processes the data, it must free the memory to avoid memory overflows. 
  This allows flexible data handling, such as sending messages, HTTP requests, or formatted data.
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
