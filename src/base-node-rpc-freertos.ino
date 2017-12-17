#include <Arduino_FreeRTOS.h>
#include "Arduino.h"
#include "EEPROM.h"
#include "Wire.h"
#include "LinkedList.h"
#include "Memory.h"  // Memory utility functions, e.g., ram_free()
#include <AlignedAlloc.h>
#include "ArduinoRpc.h"
#include "nanopb.h"
#include "NadaMQ.h"  // Required replacing `#ifndef AVR` with `#if !defined(AVR) && !defined(__arm__)`
#include "CArrayDefs.h"
#include "RPCBuffer.h"
#include "BaseNodeRpc.h"  // Check for changes (may have removed some include statements...
#include "BaseNodeRpcFreeRtos.h"
#include "NodeCommandProcessor.h"
#include "Node.h"

base_node_rpc_freertos::Node node_obj;
base_node_rpc_freertos::CommandProcessor<base_node_rpc_freertos::Node> command_processor(node_obj);

TaskHandle_t task_blink_handle;

void TaskBlink( void *pvParameters );

int available_bytes = 0;

void *maintask_handle;

void setup() {
  node_obj.begin();

  // Now set up two tasks to run independently.
  xTaskCreate(
    TaskBlink
    ,  (const portCHAR *)"Blink"   // A name just for humans
    ,  64  // This stack size can be checked & adjusted by reading the Stack Highwater
    ,  NULL
    ,  2  // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
    ,  &task_blink_handle);
}

void loop () {
  if (Serial.available()) {
    node_obj.serial_handler_.receiver()(Serial.available());
    if (node_obj.serial_handler_.packet_ready()) {
      node_obj.serial_handler_.process_packet(command_processor);
    }
  }
}

void TaskBlink(void *pvParameters)  // This is a task.
{
  (void) pvParameters;

/*
  Blink
  Turns on an LED on for one second, then off for one second, repeatedly.
*/

  // initialize digital LED_BUILTIN on pin 13 as an output.
  pinMode(LED_BUILTIN, OUTPUT);

  for (;;) // A Task shall never return or exit.
  {
    digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
    vTaskDelay( 100 / portTICK_PERIOD_MS ); // wait for one second
    digitalWrite(LED_BUILTIN, LOW);    // turn the LED off by making the voltage LOW
    vTaskDelay( 1000 / portTICK_PERIOD_MS ); // wait for one second
  }
}
