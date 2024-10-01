# centos8

```bash
docker run -it centos
cd /etc/yum.repos.d/
sed -i 's/mirrorlist/#mirrorlist/g' /etc/yum.repos.d/CentOS-*
sed -i 's|#baseurl=http://mirror.centos.org|baseurl=http://vault.centos.org|g' /etc/yum.repos.d/CentOS-*
yum install -y epel-release
yum install cmake gcc gcc-c++ make git -y
yum install openssl-devel -y
dnf --enablerepo=powertools install protobuf -y
dnf --enablerepo=powertools install protobuf-devel -y
git clone https://github.com/crust-hub/avant.git
cd avant
cd protocol
make
cd ..
mkdir build
rm -rf ./build/*
cd build
cmake ..
make -j4
```
