FROM ubuntu:20.04
# was gcc:9, is ubuntu just for libretro until we can pull out apps out of marketplace

WORKDIR /alloplace2
EXPOSE 21337/udp

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y build-essential \
    cmake curl \
    libgme-dev libcairo2 libpoppler-glib-dev \
    retroarch libretro-nestopia libretro-genesisplusgx libretro-snes9x \
    libavcodec-dev libavformat-dev libswresample-dev libswscale-dev \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

# 1. configure marketplace
ADD marketplace /alloplace2/marketplace
RUN cd marketplace && bash bootstrap.sh
RUN cd marketplace/apps/flynncade && make
RUN cd placesettings && allo/assist fetch

COPY deps /alloplace2/deps
COPY src /alloplace2/src
COPY CMakeLists.txt /alloplace2

# We can't run generate-version.sh inside of the container :S
# This means you MUST run make version_header outside of the container beforebuilding!!
COPY build/deps/allonet/include /alloplace2/build/deps/allonet/include

RUN cd build; cmake ..; make

CMD cd /alloplace2; ./build/alloplace2
