# basihetplantje
This is my first attempt playing with the arduino enviroment working with embedded c++ 
Many thanks to https://www.offerzen.com/nl/make 

## used hardware
1. ESP8266 module N
2. Dupont f-f wires
3. Moisture sensor
4. Watering pump
5. TIP31C Transistor
6. Extra led(for dry testing) https://www.benselectronics.nl/3-watt-led-module.html

# first setup
Go to https://lastminuteengineers.com/esp8266-nodemcu-arduino-tutorial/ and first get your board to blink.
Then open basihetplantje.ino with the arduino enviroment and upload it to the board.
Open Serial monitor and upload a wifi.json just copy and paste the whole json and send it.
In the monitor you should now see that a connection will be made with your wifi network. 

Now the you can upload the config.json to the board using postman or any other tool of your liking. 
http://basihetplantje.local/configuration using post with a raw text/json body with the content of your config. F

If you do not have bonjour installed you can use the ip adress of the board instead. The serial monitor will give 
you the ip address when connected to your local network.

# config.json
`````json
{
  "pumpOnTime":  2.25,
  "pumpOffTime": 60,
  "pumpOnValue": 500,
  "checkInterval": 0.1000,
  "dryRun": false
}
`````
1. pumpOnTime how long should the pump be switched on. If this value is to high you might drown your plant. 
2. pumpOffTime how long should the pump be off to give to ground time to get saturated.
3. pumpOnValue When the sensovalue is above this value the pump is switched on. Be aware this is the value from the moisture sensor where 0 (although mine gave 200 when in a glass of water) is wet and 1024 is dry.
4. checkInterval how much time should there be between checks
5. dryRun: do not switch the pump on(port D5)

# extra led and dry run
IF you have a extra led you can connect it to port D6. When the pump is switched on the led will also be switched on. In dry run mode only the led is switched on and not the pump. This is very helpfull since this makes it possible to test without giving your computer water instead of your plants. 

# checking the status of the plant
http://basihetplantje.local will give you back a json with the current status of the plant
````json
{
    saturation: 402,
    saturated: true,
    pump: false,
    totalPumpTime: 83677,
    dryRun: false 
}
````
1. saturation: the current value of the moisture sensor
2. saturated: The digital value of the moisture sensor
3. pump: is the pump currently switched on(also will be true if we are in dry run mode)
4. totalPumpTime: how much milliseconds has the pump been switched on....
5. dryRun: when true the pump never will be actually be switched on.

# manual switch the pump on
http://basihetplantje.local/manual/on will manually switch the pump on. In the dry run mode only the the led is switched on

# manual switch the pump off
http://basihetplantje.local/manual/on will manually switch the pump off.

# todo list
0. Have fun doing what ever you do.
1. Make an interface to configure the wifi using the serial port
2. Make a web interface to configure the plant. Show the status and make some graffics. Should not be a big problem since it's looking like an api interface
3. Add a led display just because it's cool
4. Solve the problem: When the pump has filled up the tube the whole water container is emptied, after the pump has been switched off
5. Make sure the wires don't come off that easy
6. Put everything in a box 

# who can contribute to this project
Anyone who wants to.  I hope somewhere in 2021 we can do it in the same room and have fun....

# disclaimer
Plants love a little bit of water computers and water is not a very good idea


