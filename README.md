# LeitPegA
WiFi connected level meter using conductibility

To be honest, the principle is not new and there are already some descriptions of this kind of level measurement method for cisterns, wells and pump shafts on the Internet. However, what these COTS systems have in common is that the water level can only be read at fixed intervals when a button is pressed (to protect the batteries and prevent corrosion of or electrolysis on the electrodes). 

In the LeitPegA (Leitfaehigkeitsbasierte PegelAnzeige - conductivity-based level display) described here, this principle was combined with a Mega 2560 in order to enable evaluation by a microcontroller in addition to the graphic LED display via transistor circuit.
The levels are queried directly through the analog inputs of the Mega 2560. If the applied voltage exceeds a threshold value (0.7V due to the design), it evaluates this as "HIGH" and counts a variable upwards until this condition is no longer met. Then this loop breaks and the value of the variable is returned.

In addition, a potted KTY81-210 temperature sensor is installed in the cistern. These two values, the level and the water temperature, are shown on a Densitron PC-6749-AAW display in a three-second alternation.
In addition to being controlled via a button, the level can be measured using a UDP command via a WLAN connection using an ESP-01 module. In both cases, both the water level in percent and the temperature as an ASCII string are sent to the UDP server (home server) via UDP.

Another function of the LeitPegA is to switch from the cistern to the emergency feed tank if the level has dropped to 20 percent (or less). For this purpose, a solid-state relay is activated, which in turn operates a switching valve to enable water to be drawn from the emergency feed tank. To do this, the home server sends a corresponding UDP command to the LeitPegA in order to switch the relay on and off.

The device has been doing its job for almost a year now and works reliably for the most part. If, however, the water quality of the inlet is too good, i.e. the degree of pollution is too low, the resistance of the water is too high for a short time and the level returned is too low. However, this condition only lasts for a few hours, as existing deposits are loosened from the cistern walls and the resistance is reduced again. 
