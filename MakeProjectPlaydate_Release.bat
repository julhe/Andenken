mkdir buildRelease
cd buildRelease
cmake .. -G "NMake Makefiles" --toolchain="C:/Users/Julian/Documents/PlaydateSDK/C_API/buildsupport/arm.cmake" -DCMAKE_BUILD_TYPE=Release
nmake
cd ..