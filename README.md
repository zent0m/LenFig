## Project Len

This is a project I did for someone's birthday, where I added on another "window" to a figurine that acts as an animated GIF Player.

I've used an ESP32 WROOM 32 and an ST7789 1.69" TFT display.

The uploading functionality isn't as seamless as it could be (temperamental depending on the GIF uploaded) but if instructions are followed, it's functional.
GIF playback is limited by the hardware. Could be overclocked.

Due to a limited timeframe, only the essentials were implemented and troubleshooted.

![Image](/LenFig/images/ProjLen.jpg)
![GIF](/LenFig/images/ProjLen.gif)

## Features
- Web page authentication for the hosted web server.
- List, download, delete and upload files with progress bar and appropriate prompts.
- Reboot ESP32 from the webpage.
- Reset the WiFi configuration from the webpage.
- Display GIFs onto the 

## Attributions
[Uploading to ESP32 Filesystem using AsyncWebServer by smford](https://github.com/smford/esp32-asyncwebserver-fileupload-example/tree/master).
[Displaying animated GIFs to a TFT from SPIFFS using AnimatedGIFs by thelastoutpostworkshop](https://github.com/thelastoutpostworkshop/animated_gif_sdcard_spiffs/tree/main)