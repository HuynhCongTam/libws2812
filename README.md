Lib WS2812
====

An easy to use DMA library to control up to 16 channels of ws2812 based lights. Since this uses DMA very little time is taken to update the lights leaving many more cycles free to do what ever else you need!  
  
###Quick Usage  
  
  
`StartLights(NUM LIGHTS);`  
NUM Lights is the maximum number of lights on any single channel. Important for example if you have 8 lights on channel 0 and 16 lights on channel 1 then NUM LIGHTS must be 16.

`void SetColor(uint8_t port_num, uint16_t led_num, uint8_t r, uint8_t g, uint8_t b)`  
Sets the color of a given light on port_num

`uint32_t GetColor(uint8_t prot_num, uint16_t led_num)`  
Returns the color currently in memory for a given light. This could vary from what is actually being displayed if the light color has been set but not sent to light.  

`void AddColor(uint8_t port_num, uint16_t led_num, uint8_t r, uint8_t g, uint8_t b)`  
Like Set color but instead of overriding the current color it adds each of the descrete values.

'void SubtractColor(uint8_t port_num, uint16_t led_num, uint8_t r, uint8_t g, uint8_t b)'  
Like Add Color but subtracts instead.

`void ClearAll()`  
Sets all lights to 0, 0, 0

`void Start()`  
Outputs the current values to the lights. This first checks if there is currently an update happening if there is it waits until it completes then starts the next one. Since the shifting of data to the lights is handled by DMA the function will return immediatly. Inspection of the update_pending variable can be used to determine the current state of the DMA function.

