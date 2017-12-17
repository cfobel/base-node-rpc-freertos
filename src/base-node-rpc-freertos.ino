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

// define two tasks for Blink & AnalogRead
void TaskBlink( void *pvParameters );
void TaskSerialListen( void *pvParameters );

void *maintask_handle;

void setup() {
  node_obj.begin();
  // Register callback function to process any new data received via I2C.
  Wire.onReceive(i2c_receive_event);

  // Now set up two tasks to run independently.
  xTaskCreate(
    TaskBlink
    ,  (const portCHAR *)"Blink"   // A name just for humans
    ,  128  // This stack size can be checked & adjusted by reading the Stack Highwater
    ,  NULL
    ,  2  // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
    ,  NULL );

  xTaskCreate(
    TaskSerialListen
    ,  (const portCHAR *) "Serial listen"
    ,  128  // Stack size
    ,  NULL
    ,  1  // Priority
    ,  NULL );

  // Now the task scheduler, which takes over control of scheduling individual tasks, is automatically started.
}


void loop() {}


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
    vTaskDelay( 1000 / portTICK_PERIOD_MS ); // wait for one second
    digitalWrite(LED_BUILTIN, LOW);    // turn the LED off by making the voltage LOW
    vTaskDelay( 1000 / portTICK_PERIOD_MS ); // wait for one second
  }
}

void TaskSerialListen(void *pvParameters)  // This is a task.
{
  (void) pvParameters;

  for (;;)
  {
    // Parse any new serial data using `SerialHandler` for `Node` object.
    if (Serial.available()) {
      node_obj.serial_handler_.receiver()(Serial.available());
      if (node_obj.serial_handler_.packet_ready()) {
        /* A complete packet has successfully been parsed from data on the serial
         * interface.
         * Pass the complete packet to the command-processor to process the request.
         * */
        node_obj.serial_handler_.process_packet(command_processor);
      }
    }
    vTaskDelay(1);  // one tick delay (15ms) in between reads for stability
  }
}
