#include <Arduino_FreeRTOS.h>
#include <queue.h>
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

using base_node_rpc_freertos::MoveRequest;

base_node_rpc_freertos::Node node_obj;
base_node_rpc_freertos::CommandProcessor<base_node_rpc_freertos::Node> command_processor(node_obj);

TaskHandle_t task_blink_handle;
TaskHandle_t task_serial_rx_handle;
TaskHandle_t task_motor_handle;

void TaskBlink( void *pvParameters );
void TaskSerialRx( void *pvParameters );
void TaskMotor( void *pvParameters );

int available_bytes = 0;

void *maintask_handle;

extern QueueHandle_t motor_queue;

const base_node_rpc_freertos::MotorConfig motor_config = {46, 48, 62};

void setup() {
  motor_queue = xQueueCreate(1, sizeof(MoveRequest));

  node_obj.begin();

  // Now set up two tasks to run independently.
  xTaskCreate(
    TaskBlink
    ,  (const portCHAR *)"Blink"   // A name just for humans
    ,  50  // This stack size can be checked & adjusted by reading the Stack Highwater
    ,  NULL
    ,  2  // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
    ,  &task_blink_handle);

  xTaskCreate(
    TaskSerialRx
    ,  (const portCHAR *)"SerialRx"   // A name just for humans
    ,  315  // This stack size can be checked & adjusted by reading the Stack Highwater
    ,  NULL
    ,  1  // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
    ,  &task_serial_rx_handle);

  xTaskCreate(
    TaskMotor
    ,  (const portCHAR *)"Motor"   // A name just for humans
    ,  71  // This stack size can be checked & adjusted by reading the Stack Highwater
    ,  (void *) &motor_config
    ,  0  // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
    ,  &task_motor_handle);
}

void loop () {}

void TaskSerialRx(void *pvParameters) {
  // Listen on serial port for incoming command requests.
  for (;;) {
    if (Serial.available()) {
      node_obj.serial_handler_.receiver()(Serial.available());
      if (node_obj.serial_handler_.packet_ready()) {
        node_obj.serial_handler_.process_packet(command_processor);
      }
    } else {
      vTaskDelay(1);
    }
  }
}

void TaskBlink(void *pvParameters)  // This is a task.
{
  (void) pvParameters;

/*
  Blink an LED mimicking a heartbeat
  (i.e., twice fast followed by a 1 second pause)
*/

  // initialize digital LED_BUILTIN pin as an output.
  pinMode(LED_BUILTIN, OUTPUT);

  for (;;) // A Task shall never return or exit.
  {
    digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
    vTaskDelay( 100 / portTICK_PERIOD_MS ); // wait for one second
    digitalWrite(LED_BUILTIN, LOW);   // turn the LED on (HIGH is the voltage level)
    vTaskDelay( 200 / portTICK_PERIOD_MS ); // wait for one second
    digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
    vTaskDelay( 100 / portTICK_PERIOD_MS ); // wait for one second
    digitalWrite(LED_BUILTIN, LOW);    // turn the LED off by making the voltage LOW
    vTaskDelay( 1000 / portTICK_PERIOD_MS ); // wait for one second
  }
}

void TaskMotor(void *pvParameters) {
  const base_node_rpc_freertos::MotorConfig config =
    *((const base_node_rpc_freertos::MotorConfig *)pvParameters);

  MoveRequest request;

  pinMode(config.STEP_PIN, OUTPUT);
  pinMode(config.DIR_PIN, OUTPUT);
  pinMode(config.ENABLE_PIN, OUTPUT);

  for (;;) {
    if (xQueueReceive(motor_queue, &request, 0)) {
      digitalWrite(config.DIR_PIN, !request.clockwise);
      digitalWrite(config.ENABLE_PIN, 0);
      for (uint32_t i = 0; i < request.count; i++) {
        digitalWrite(config.STEP_PIN, 1);
        // Pulse maximum of 100 microseconds.
        delayMicroseconds((100 > request.delay_us_) ? request.delay_us_ : 100);
        digitalWrite(config.STEP_PIN, 0);
        /* According to [here][1], the largest useful delay value for
         * `delayMicroseconds` is 16383.
         *
         * As a workaround, divide specified microsecond delay by ~1000
         * (actually by 1024, since this can be done using a shift operation
         * instead of actual division) and use the millisecond delay function
         * for the result and the microsecond delay function for the remainder.
         *
         * [1]: https://stackoverflow.com/questions/34532941/in-arduino-is-there-a-maximum-delay-time-when-using-the-fuctiondelay/34533207#34533207
         */
        delay(request.delay_us_ >> 10);
        delayMicroseconds(request.delay_us_ & 0b01111111111);
      }

      digitalWrite(config.ENABLE_PIN, 1);
    }
  }
}
