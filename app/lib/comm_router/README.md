# Communication Router

The router is a transport-neutral message broker. It broadcasts logical messages and dispatches
received messages by ID; wire framing belongs to each transport. The UART adapter owns Beaver
Protocol v1 encoding, decoding, CRC validation, and byte-stream buffering. Other transports do not
need to use Beaver Protocol. Interfaces and customers are declared at file scope with
`COMM_ROUTER_INTERFACE_DEFINE()` and `COMM_ROUTER_CUSTOMER_DEFINE()`.

## API behavior

- Customer message IDs must be unique among their compile-time declarations.
- Interface/customer descriptors live in Zephyr iterable linker sections; there are no runtime
  registration APIs, registry limits, or dynamic allocations.
- Interface write callbacks receive a message ID and payload; the payload is borrowed only until
  the callback returns. Customer payload pointers are borrowed only until their callback returns.
- Incoming handlers receive message ID and payload only; source-interface identity is not exposed.
- Framing and reassembly are transport responsibilities, not router responsibilities.
- Calls and callbacks execute synchronously in thread context. The API is not ISR-safe, and a
  customer callback must not recursively call `comm_router_receive()`.
- Maximum logical payload is fixed at build time through `CONFIG_COMM_ROUTER_MAX_PAYLOAD`.