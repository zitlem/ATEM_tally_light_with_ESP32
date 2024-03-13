# ATEM-tally-light-server ESP32-ETH01

Wired tally light server for use with ATEM switchers. Connects over Ethernet using only a ESP32-ETH01 and a LED strip (WS2812B). This solution is not limited by the ATEM switchers' connection limit, making it possible to connect as many as you need.

# What does it do?
Once set up, it will automatically connect to an ATEM switcher over Ethernet and function as a tally light server (showing all Preview And Program)

When the program is uploaded to the ESP32-ETH01 the setup is done with a webpage it serves over Ethernet where you are able to see status details, and perform the basic setup.

As Atem swithcers only allow for 5-8 simultanious clients (dependant on the model) v2.0 introduced Tally Server functionality. This makes the system only require one connection from the switcher, as the ESP32-ETH01 can retransmit data to other wireless tallys. An example setup is shown in the diagram below, where arrows indicate the direction of tally data from swtcher/tally unit to client tally unit.


## Connection and tally state indication
The different states of connection is signalled with LED colors.

Color | Description
------|--------
BLUE | Connecting to Ethernet
PINK | Connecting to ATEM Swithcher (Connected to WiFi).
RED | Tally program
GREEN | Tally preview
OFF | Tally neither live or preview
ORANGE | Connected and running (Only shown by status LED on addressable LED strip).
