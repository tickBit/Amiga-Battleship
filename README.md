# Battleship game for AmigaOS 3

Classic battle ship game with simple AI for AmigaOS 3.x. If all the resources are found and can be intialized, there shouldn't be any bugs. However, if some needed resources can't be opened or allocated, the game might still behave strangely.

To position a ship, first click once on a ship, then simply move mouse to the grid and when a position to place a ship is found, click there.

The game uses 1999 standard of C, so when compiling with VBCC, switch -c99 must be used.
More info in the source code. The source can be ONLY compiled with VBCC due to the usage of backfill hook.

In the LhA archive there are Amiga executable and info (icon) file for the executable and also the graphics.

Sorry for the typo in the "First commit"...

Please notice, that your Amiga system should have big enough resolution: The inner size of the game window is 800x800 pixels.

This version still may have bugs.

## The colors might not differ anymore in different Amiga systems

In this version (1.5.0) ObtainBestPenA() is used to get PenA colors.
The function looks from the system best fit for given RGB codes, so now hopefully the colors are quite a like in different Amiga systems.

For some reason, in all the different Amiga systems I've tested the game, the colors are OK, if not started from icon.

## There's a light version, too

I have added a light version of the game, that might work better than the heavier version. The inner size of the window in the light version is 600x600 pixels and there is only a title picture (that doesn't show up, if the game is started from icon) that is embedded in C code, only 1 bitplane.

## Picture

Picture of the game running on AmiKit:

![Battleship-AmiKit](https://github.com/user-attachments/assets/a24e5642-8173-49a6-b762-2a37f5888b8a)

Picture of the light version:

![Battleship-light](https://github.com/user-attachments/assets/201d2ede-9c66-40f7-a017-137d1ad700b4)


