# tao thu muc extern chua thu vien

mkdir extern
cd extern

# cai msquic

git clone https://github.com/microsoft/msquic.git
cd msquic
mkdir build
cd build
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja

# cai hiredis and libpg (redis va postgres).

# install hiredis and libpg
sudo apt update
sudo apt install libhiredis-dev -y
sudo apt install libpq-dev -y

# check hiredis and libgp
dpkg -l | grep hiredis
dpkg -l | grep libpq

# Boot.Asio
sudo apt install libboost-all-dev -y
# Standalone Asio:
sudo apt install libasio-dev -y