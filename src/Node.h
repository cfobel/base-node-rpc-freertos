#ifndef ___NODE__H___
#define ___NODE__H___

#include <Arduino_FreeRTOS.h>
#include <math.h>
#include <string.h>
#include <stdint.h>
#include <Arduino.h>
#include <NadaMQ.h>
#include <CArrayDefs.h>
#include "RPCBuffer.h"  // Define packet sizes
#include "BaseNodeRpcFreeRtos/Properties.h"  // Define package name, URL, etc.
#include <BaseNodeRpc/BaseNode.h>
#include <BaseNodeRpc/BaseNodeEeprom.h>
#include <BaseNodeRpc/BaseNodeI2c.h>
#include <BaseNodeRpc/BaseNodeConfig.h>
#include <BaseNodeRpc/BaseNodeState.h>
#include <BaseNodeRpc/BaseNodeI2cHandler.h>
#include <BaseNodeRpc/BaseNodeSerialHandler.h>
#include <BaseNodeRpc/SerialHandler.h>
#include <pb_cpp_api.h>
#include <pb_validate.h>
#include <pb_eeprom.h>
#include <LinkedList.h>
#include <AlignedAlloc.h>
#include "base_node_rpc_freertos_config_validate.h"
#include "base_node_rpc_freertos_state_validate.h"
#include "BaseNodeRpcFreeRtos/config_pb.h"
#include "BaseNodeRpcFreeRtos/state_pb.h"

extern TaskHandle_t task_blink_handle;
extern TaskHandle_t task_serial_rx_handle;

namespace base_node_rpc_freertos {

const size_t FRAME_SIZE = (3 * sizeof(uint8_t)  // Frame boundary
                           - sizeof(uint16_t)  // UUID
                           - sizeof(uint16_t)  // Payload length
                           - sizeof(uint16_t));  // CRC


class Node;
const char HARDWARE_VERSION_[] = "0.1.0";

typedef nanopb::EepromMessage<base_node_rpc_freertos_Config,
                              config_validate::Validator<Node> > config_t;
typedef nanopb::Message<base_node_rpc_freertos_State,
                        state_validate::Validator<Node> > state_t;

class Node :
  public BaseNode,
  public BaseNodeEeprom,
  public BaseNodeI2c,
  public BaseNodeConfig<config_t>,
  public BaseNodeState<state_t>,
#ifndef DISABLE_SERIAL
  public BaseNodeSerialHandler,
#endif  // #ifndef DISABLE_SERIAL
  public BaseNodeI2cHandler<base_node_rpc::i2c_handler_t> {
public:
  typedef PacketParser<FixedPacket> parser_t;

  static const uint8_t STEP_PIN    =    46;
  static const uint8_t DIR_PIN     =    48;
  static const uint8_t ENABLE_PIN  =    62;
  static const uint32_t BUFFER_SIZE = 128;  // >= longest property string

  uint8_t buffer_[BUFFER_SIZE];

  LinkedList<uint32_t> allocations_;
  LinkedList<uint32_t> aligned_allocations_;

  Node() : BaseNode(),
           BaseNodeConfig<config_t>(base_node_rpc_freertos_Config_fields),
           BaseNodeState<state_t>(base_node_rpc_freertos_State_fields) {
    // XXX Turn on LED by default to indicate power is on.
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(STEP_PIN, OUTPUT);
    pinMode(DIR_PIN, OUTPUT);
    pinMode(ENABLE_PIN, OUTPUT);
  }

  UInt8Array get_buffer() { return UInt8Array_init(sizeof(buffer_), buffer_); }
  /* This is a required method to provide a temporary buffer to the
   * `BaseNode...` classes. */

  void begin();
  /****************************************************************************
   * # User-defined methods #
   *
   * Add new methods below.  When Python package is generated using the
   * command, `paver sdist` from the project root directory, the signatures of
   * the methods below will be scanned and code will automatically be generated
   * to support calling the methods from Python over a serial connection.
   *
   * e.g.
   *
   *     bool less_than(float a, float b) { return a < b; }
   *
   * See [`arduino_rpc`][1] and [`base_node_rpc`][2] for more details.
   *
   * [1]: https://github.com/wheeler-microfluidics/arduino_rpc
   * [2]: https://github.com/wheeler-microfluidics/base_node_rpc
   */
  UInt8Array hardware_version() {
    return UInt8Array_init(strlen(HARDWARE_VERSION_),
                           (uint8_t *)&HARDWARE_VERSION_[0]);
  }

  bool set_id(UInt8Array id) {
    if (id.length > sizeof(config_._.id) - 1) {
      return false;
    }
    memcpy(config_._.id, &id.data[0], id.length);
    config_._.id[id.length] = 0;
    config_._.has_id = true;
    config_.save();
    return true;
  }

  // # Callback methods
  void on_tick() {}

  uint32_t task_high_water_mark(uint8_t task_id) {
    const uint8_t BLINK_TASK = 1;
    const uint8_t SERIAL_RX_TASK = 2;

    switch (task_id) {
      case BLINK_TASK:
        return uxTaskGetStackHighWaterMark(task_blink_handle);
      case SERIAL_RX_TASK:
        return uxTaskGetStackHighWaterMark(task_serial_rx_handle);
      default:
        return 0;
    }
  }

  uint32_t step(uint32_t count, bool clockwise, uint32_t delay_ms) {
    digitalWrite(DIR_PIN, !clockwise);
    digitalWrite(ENABLE_PIN, 0);
    for (uint32_t i = 0; i < count; i++) {
      digitalWrite(STEP_PIN, 1);
      delay_us(delay_ms);
      digitalWrite(STEP_PIN, 0);
      delay_us(delay_ms);
    }

    digitalWrite(ENABLE_PIN, 1);
    return task_high_water_mark();
  }

  /** Called periodically from the main program loop. */
  void loop() {}

  // ##########################################################################
  // # Accessor methods
  uint16_t analog_input_to_digital_pin(uint16_t pin) { return analogInputToDigitalPin(pin); }
  uint16_t digital_pin_has_pwm(uint16_t pin) { return digitalPinHasPWM(pin); }
  uint16_t digital_pin_to_interrupt(uint16_t pin) { return digitalPinToInterrupt(pin); }

  // ##########################################################################
  // # Mutator methods
  uint32_t mem_alloc(uint32_t size) {
    uint32_t address = (uint32_t)malloc(size);
    // Save to list of allocations for memory management.
    allocations_.add(address);
    return address;
  }
  uint32_t mem_aligned_alloc(uint32_t alignment, uint32_t size) {
    uint32_t address = (uint32_t)aligned_malloc(alignment, size);
    // Save to list of allocations for memory management.
    aligned_allocations_.add(address);
    return address;
  }
  uint32_t mem_aligned_alloc_and_set(uint32_t alignment, UInt8Array data) {
    // Allocate aligned memory.
    const uint32_t address = mem_aligned_alloc(alignment, data.length);
    if (!address) { return 0; }
    // Copy data to allocated memory.
    mem_cpy_host_to_device(address, data);
    return address;
  }
  void mem_aligned_free(uint32_t address) {
    for (int i = 0; i < aligned_allocations_.size(); i++) {
      if (aligned_allocations_.get(i) == address) {
        aligned_allocations_.remove(i);
      }
    }
    aligned_free((void *)address);
  }
  void mem_cpy_host_to_device(uint32_t address, UInt8Array data) {
    memcpy((uint8_t *)address, data.data, data.length);
  }
  void mem_fill_uint8(uint32_t address, uint8_t value, uint32_t size) {
    mem_fill((uint8_t *)address, value, size);
  }
  void mem_fill_uint16(uint32_t address, uint16_t value, uint32_t size) {
    mem_fill((uint16_t *)address, value, size);
  }
  void mem_fill_uint32(uint32_t address, uint32_t value, uint32_t size) {
    mem_fill((uint32_t *)address, value, size);
  }
  void mem_fill_float(uint32_t address, float value, uint32_t size) {
    mem_fill((float *)address, value, size);
  }
  void mem_free(uint32_t address) {
    for (int i = 0; i < allocations_.size(); i++) {
      if (allocations_.get(i) == address) { allocations_.remove(i); }
    }
    free((void *)address);
  }
};
}  // namespace base_node_rpc_freertos


#endif  // #ifndef ___NODE__H___
