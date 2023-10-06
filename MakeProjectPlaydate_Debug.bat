mkdir buildDebug
cd buildDebug
cmake .. -G "NMake Makefiles" --toolchain="C:/Users/Julian/Documents/PlaydateSDK/C_API/buildsupport/arm.cmake" 
nmake
cd ..