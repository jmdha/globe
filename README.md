# globe
A 3d render of the earth.

https://github.com/user-attachments/assets/4dbf95de-6352-46a7-afa3-b3a3e925f7f7

## Building
Uses a renderer based on [RGFW](https://github.com/ColleagueRiley/RGFW/blob/main/RGFW.h). As such, requires only system specific window handler.

```
linux : gcc -O3 main.c io.c -lm -lX11 -lXrandr
windows : gcc -O3 main.c io.c -lm -lgdi32
macos : gcc -O3 main.c io.c -lm -framework Cocoa -framework CoreVideo -framework IOKit
```
> [!NOTE]  
> Has only been tested on linux, but should work on other platforms.
