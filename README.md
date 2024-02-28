# CasaMaquinas
This project manages a swimming pool machine room with several features. It works with Esp32 and RainMaker framework.

# Features
Remembering that activation is done with logic level 0. Deactivation is done with logic level 1.

## Filtering pump 
#### Activates I/O 16
Manages the pool main pump.  

## LEDs
#### Activates I/O 17
Manages pool ilumination. 

## Drain Pump
#### Activates I/O 18
Manages a small pump to drain the whole where the pump main pump is placed. For the case where yout main pump is under ground level. It works together with a level sensor.  

## Warming Pump
#### Activates I/O 4
Manages the pump that pumps water to the solar warming system. It works together with atwo NTC sensors. One is placed in the return path, and the other is placed in the solar plates. In the RainMaker app, it will appear as a thermostat and the user will be abçe to select the pool's temperatures. The firmware will then control this pump properly. If the set desired temperature is lower than the solar plates temperature, then this pump will be activated. Otherwise, it will deactivate.

## Cupper Ionizer
#### Activates I/O 19
Manages the Cupper ionizer.

## Temperature sensors (NTC)

![PDF Image](figures/figNTC.pdf)
\begin{circuitikz} 
    \draw (0,0) to[V, v_=5V, invert] (0,3)
                to[R, l_=NTC] (3,3)
                to[short, -*] (4,3) node[anchor=south] {To ESP32 Pin}
                to[R, l_=1k\Omega] (4,0)
                -- (0,0);
    \draw (0,0) node[ground]{};
\end{circuitikz}
### Characteristics
Both NTCs are 10kOhm @ 25 degrees Celsius. Beta factor is 3950.
#### Solar Plates NTC
It is placed in the solar plates. In the ESP32 board, it is connected in pin ADC1, channel 3. In the RainMaker app, it appears as a temperatures sensor. 

#### Pool NTC
It is placed in the pool's return path. In the ESP32 board, it is connected in pin ADC1, channel 3.
