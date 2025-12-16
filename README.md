# globe
A 3d render of the earth in ~100 lines of C code.

https://github.com/user-attachments/assets/4dbf95de-6352-46a7-afa3-b3a3e925f7f7

## Building
Uses a renderer based on [RGFW](https://github.com/ColleagueRiley/RGFW/blob/main/RGFW.h). As such, requires only system specific window handler.

```
linux : gcc -O3 main.c -lm -lX11 -lXrandr
windows : gcc -O3 main.c -lm -lgdi32
macos : gcc -O3 main.c -lm -framework Cocoa -framework CoreVideo -framework IOKit
```
> [!NOTE]  
> Has only been tested on linux, but should work on other platforms.

## Data
Uses data from [Natural Earth](https://www.naturalearthdata.com/), specifially their 1:10m countries. The original data is "ne_10m_admin_0_countires.shp" and the simplified is 10m_countries.

The simplified data format stores the number of polygons (64 bit unsigned int), and that amount of polygons, where each polygon contains the number of points (64 bit unsigned int) followed by each point (2 x 64 bit float).
