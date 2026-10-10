# Communication Router

`comm_router` is a transport-neutral broker between communication interfaces and application
customers. It does not initialize peripherals or define a wire format. Interfaces exchange logical
messages with the router; a transport adapter owns any encoding, framing, or fragmentation needed by
its physical link.

```text
Communication interface                         Customer
        |                                            ^
        | comm_router_receive(interface, id, payload, length)
        v                                            |
                  message-ID dispatch

Customer -> comm_router_send(message, payload)
         -> every interface write callback
```

## Components

- `comm_router.c/.h` owns logical-message validation, broadcast transmission, and customer dispatch.
- `uart_transport.c` owns UART byte-stream buffering and Beaver Protocol encoding, decoding, and
        CRC verification. Beaver framing is not required by other transports.
- Interface and customer descriptors are declared at file scope. Zephyr iterable linker sections
  provide the descriptor lists; the router has no runtime registration API or dynamic allocation.

Enable the router with `CONFIG_COMM_ROUTER=y`. The maximum logical payload is configured with
`CONFIG_COMM_ROUTER_MAX_PAYLOAD` (default 64 bytes, range 1–1024). `CONFIG_UART_TRANSPORT=y` enables
UART and selects the CRC subsystem required by its Beaver framing.

## Beaver Frame

All multi-byte fields are little-endian.

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 byte | Start marker, `0xA5` |
| 1 | 1 byte | Protocol version, `1` |
| 2 | 2 bytes | Message ID |
| 4 | 2 bytes | Payload length |
| 6 | Variable | Payload, up to `CONFIG_COMM_ROUTER_MAX_PAYLOAD` bytes |
| `6 + payload length` | 2 bytes | CRC-16/CCITT-FALSE |

This frame format is used by UART only. The CRC is calculated over the version, message ID, payload
length, and payload. It excludes the start marker and CRC field. The parameters are polynomial
`0x1021`, initial value `0xFFFF`, no input or output reflection, and xor-out `0x0000`. The encoded
CRC is stored little-endian.

## Declaring Endpoints

Declare descriptors at file scope using the macros in `comm_router.h`. Customer message IDs must be
unique.

```c
static int interface_write(void *context, uint16_t message_id, const uint8_t *payload,
        size_t payload_length);
static void handle_status(void *context, uint16_t message_id, const uint8_t *payload,
        size_t payload_length);

COMM_ROUTER_INTERFACE_DEFINE(uart_interface, interface_write, NULL);
COMM_ROUTER_CUSTOMER_DEFINE(status_customer, 0x0100U, handle_status, NULL);
```

`COMM_ROUTER_INTERFACE_DEFINE` adds a transport endpoint; its `write` callback receives a logical
message ID and payload. `COMM_ROUTER_CUSTOMER_DEFINE` maps one message ID to a handler. The
interface adapter is responsible for hardware setup and any wire-format conversion.

### UART Transport

Enable `CONFIG_UART_TRANSPORT` to build the UART adapter. It uses the Zephyr UART device referenced
by the `comm` devicetree alias. Define a transport at file scope with `UART_TRANSPORT_DEFINE`; the
application declares and starts this endpoint from `main.c`. The generic backend uses the
interrupt-driven FIFO API. The STM32 backend uses Zephyr's asynchronous UART API, normally backed by
DMA. Both backends enqueue received chunks and forward them to the UART adapter from the system
workqueue. The UART adapter assembles and decodes Beaver frames before submitting logical messages
to the router, and encodes router messages before writing bytes to the backend.

The alias must identify a ready UART that supports the selected backend. Configure its baud rate,
pins, and DMA resources in board devicetree. The STM32 backend uses Zephyr's UART driver to retain
exclusive peripheral ownership; do not access that UART directly through HAL. Both RX queues are
bounded, so size them for the maximum expected burst and processing latency.

## Receiving

Non-UART adapters call `comm_router_receive(&interface, message_id, payload, payload_length)` once
they have decoded a logical message. UART adapters instead pass each byte chunk to
`uart_transport_receive()`. Chunks may contain part of a frame or several frames; UART buffers each
stream, validates complete Beaver frames, and then submits decoded messages to the router.

For UART, bytes before the next start marker are discarded. Unsupported versions, excessive payload
lengths, and malformed frames are skipped while the adapter searches for the next frame. The router
sends each valid message to the first declared customer with a matching message ID; unknown IDs are
ignored. The customer receives only the message ID and payload; source-interface identity is not
exposed.

The payload pointer passed to the customer is borrowed and valid only for the duration of the
callback. Copy it if it needs to be retained.

## Sending

Call `comm_router_send(message_id, payload, payload_length)`. The router invokes every declared
interface's write callback with that logical message. Each callback must consume or copy the payload
before returning. The router attempts every interface even if one write fails; it returns zero if
all writes succeed, otherwise the first errno-style write error. It returns `-ENOENT` if no
interfaces are declared.

## Execution Context

Router receive processing and customer callbacks are synchronous in thread context; the API is not
ISR-safe. Do not call `comm_router_receive()` recursively from a customer callback. The router
serializes receive processing with a mutex. UART stream processing is also serialized; do not call
`uart_transport_receive()` recursively from a customer callback. Callbacks should return promptly
and must not block waiting for another receive operation.

## Commander Messages

Commander registers message ID `0x0100` for operator commands. Its payload is exactly seven bytes:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 byte | Mode: `0` off, `1` voltage, `2` torque, `3` speed, `4` position, `5` calibration |
| 1 | 4 bytes | Signed Q31 normalized mechanical speed, little-endian |
| 5 | 2 bytes | Signed current value, little-endian; units and meaning are not defined yet |

The setpoint uses a signed 1.31 fixed-point representation of mechanical revolutions per second;
the current value uses 16-bit two's-complement. Setpoint
interpretation depends on the selected mode. Commander
acknowledges commands using message ID `0x0101` with a one-byte
status: `0` accepted, `1` invalid payload length, or `2` invalid mode. Valid commands update the
latest-command snapshot. The receive callback queues command and status events without waiting;
the commander thread updates the snapshot and sends the acknowledgment. If its bounded event queue
is full, the event is dropped and no acknowledgment is sent. Acknowledgment transmission errors are
logged by commander.

`voltage` drives the existing open-loop motor waveform. Motor dispatch for torque, speed, position,
and calibration modes is not implemented yet.