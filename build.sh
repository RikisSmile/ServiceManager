mkdir build

cd src/Server
g++ -o ServiceMN main.cpp ProcessRunner.cpp -O3
cd ..
cd ..

cd src/Interface 
g++ -o interface main.cpp -O3
cd ..
cd ..
