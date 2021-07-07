FROM gcc:9

WORKDIR /alloplace2
EXPOSE 21337/udp

RUN apt-get update && apt-get install -y build-essential \
    cmake \
    libgme-dev libcairo2 libpoppler-glib-dev \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

# 1. configure marketplace
ADD marketplace /alloplace2/marketplace
RUN cd marketplace && bash bootstrap.sh

COPY deps /alloplace2/deps
COPY src /alloplace2/src
COPY CMakeLists.txt /alloplace2

# We can't run generate-version.sh inside of the container :S
# This means you MUST run make version_header outside of the container beforebuilding!!
COPY build/deps/allonet/include /alloplace2/build/deps/allonet/include

RUN cd build; cmake ..; make

COPY build /alloplace2/build

CMD cd /alloplace2; ./build/alloplace2
