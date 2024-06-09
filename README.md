the zip compiled binary is outdated. Compile from source to get the updated code
compile command: g++ -O3 -std=c++17 -IpathToFile/include -IpathToFile/headers -LpathToFIle/lib pathToFile/main.cpp pathToFile/definitions_of_headers/*.cpp pathToFile/src/glad.c -lglfw3dll -o pathToFile/outputName.exe
