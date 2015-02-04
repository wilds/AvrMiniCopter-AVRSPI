# AvrMiniCopter-AVRSPI

AVRSPI provides a TCP/UDP service for clients to connect to in order to communicate with the controller (AvrMiniCopter-Arduino). It is mainly used to query current status, issue target position request, forward altitude reading etc.

By default AVRSPI binds to UDP and TCP port 1030.

#### TCP/UDP message structure
Each message is of a fixed length - 4 bytes:
- control (uint8_t)
- type (uint8_t)
- value (int16_t)

Control (for incoming messages):
- currently only used when using UDP
- 0: do not remember the client, just process the incoming message
- 1: remember the client and process the incoming message
- 2: remember the client and ping back

Control (for outgoing messages):
- 0: a regular message
- 1: disconnect request

Type:
- messages with type 0 will be handled by AVRSPI
  Values:
   - 0: dummy message
   - 1: reset backend and initiate autoconfig
   - 2: reset backend but do not initiate autoconfig
   - 3: get number of CRC errors for incoming SPI transfers
   - 4: save flight log
- all other types will be forwarded to the backend; see the backend documentation




#### TODO
- implement more efficient transmission 
- remove flight log and configurator 

