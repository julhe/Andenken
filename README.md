# Andenken
![background](https://github.com/julhe/InnoJam2023_Oct/assets/18037091/d2087338-230e-4b61-ab63-c6a73bb86cf3)
This game was created [on the InnoGames GameJam #14](https://www.igjam.eu/jams/igjam-14/949/ ) for the [Playdate Console](https://play.date/). 
## Credits
* [Amanda - PM](https://www.igjam.eu/users/Marching_Duck/)
* [Nadine - Art](https://www.igjam.eu/users/Trickster/)
* [Jonas - Art](https://www.igjam.eu/users/Anderlicht/)
* [Julian - Code](https://www.igjam.eu/users/schneckers/) 
## Playtrough Video
[andeken_playtrough.webm](https://github.com/julhe/InnoJam2023_Oct/assets/18037091/337fc2ea-7bea-46cc-9306-b9323b0e4ae5)

# Leftover TODOs
* Collision for the world.
* Properly formated text + no missing glyphs.
* No "glitch frames".
* More enemies.
* Dithered lightcone.
* Slightly more organized code...
* ...
# Building
See the release section for a pre-compiled build. If you want to get your hands dirty you need:
* CMake 3.14+
* Playdate SDK 2.02+
* Visual Studio 2022+ with C/C++ Toolchain.
* [gcc-arm-none-eabi](https://developer.arm.com/downloads/-/gnu-rm) toolchain (if you want to build for the Playdate)
## Emulator/Developer Builds
1. Edit ``MakeProject.bat`` to point to your local Playdate SDK instatlation.
2. Run ``MakeProject.bat``.
3. Open ``buildDevelopment/InnoJam2023_Oct.sln``.
4. In the Solution Explorer Window, set InnoJam2023_Oct as the startup project.
5. Run the build. 
## Playdate Builds
1. Edit ``MakeProjectPlaydate_Release.bat`` to point to your local Playdate SDK instatlation.
2. Run ``MakeProjectPlaydate_Release.bat`` from the Visual Studio Commandline Prompt.
3. Open the Playdate Simulator, Device/Upload Game to Device...

