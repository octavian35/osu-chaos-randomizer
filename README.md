# osu-chaos-randomizer
literally making your life worse for fun

# Steps for use
1. Disable raw input in osu!
2. Remember and change the default restore value at line 98 (SystemParametersInfo(SPI_SETMOUSESPEED, 0, (PVOID)(intptr_t)10 <- THIS , SPIF_SENDCHANGE);
3. Change the default keybinds at the top of the cpp
4. Enjoy!

# Build steps
1. Change configuration type to .exe, Windows SDK ver to 10.0, Toolkit v145 and C++ 20
2. Change from linker/system the subsystem to Console
3. Build!
