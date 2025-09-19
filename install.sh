#install hiredis and libpg
sudo apt update
sudo apt install libhiredis-dev -y
sudo apt install libpq-dev -y

# tao thu muc include chua thu vien
mkdir include
cd include
# cai msquic
git clone https://github.com/microsoft/msquic.git
cd msquic
mkdir build
cd build
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja
cd ../..
# cai redis plus plus
git clone https://github.com/sewenew/redis-plus-plus.git
cd redis-plus-plus
mkdir build && cd build
cmake ..
make
sudo make install
cd ..
sudo apt-get install libuv1-dev libssl-dev
git clone https://github.com/datastax/cpp-driver.git
cd cpp-driver
mkdir build && cd build
cmake ..
make -j$(nproc)
sudo make install
#check hiredis and libgp
dpkg -l | grep hiredis
dpkg -l | grep libpq

#install json.hpp
sudo apt install nlohmann-json3-dev
sudo apt install libcurl4-openssl-dev -y
# Boot.Asio
sudo apt install libboost-all-dev -y
# Standalone Asio:
sudo apt install libasio-dev -y


