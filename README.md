# Alloplace2

`alloplace` is a server that represents a "place" in the Alloverse. A user can connect to a
place to walk around it and interact with people, and also with apps running in the place.

The place manages:

* The connections to places and users
* Forwarding of audio and video data to those clients that request it
* Message router for "interactions"
* A scene graph representing all the entities in the place
* Implements a set of interactions to control all of the above
* Receives "intents" from clients to manage movement
* Transmits the scene graph periodically to connected clients
* Runs simulation and animation on the scenegraph

It does not manage things like

* features such as "whiteboard" or "file viewer" -- they are separate alloapps that are
  launched on demand and connects to the place
* launching apps -- this is managed by the `marketplace` app
* admin features, such as kicking users -- this is managed by the `placesettings` app

# Building and running

You can modify runtime behavior with the following environment variables:

* `ALLOPLACE_NAME="My place"` -- set the display name of the server
* `ALLOPLACE_DISABLE_MARKETPLACE=true` -- don't auto-start the marketplace app
  (only really needed when you're working on the marketplace itself)

## Docker

Run latest from dockerhub:

```
docker run -it --pull=always -e ALLOPLACE_NAME="My place" -p 21337:21337/udp alloverse/alloplace2:latest
```

Build and run with local sources:

```
sudo docker build -t alloplace2 .
sudo docker run -it -e ALLOPLACE_NAME="My place" -p 21337:21337/udp alloplace2:latest
```

## Make on Linux

1. `git submodule update --init --recursive`
2. Run `marketplace/scripts/bootstrap.sh`
3. `cd placesettings && allo/assist fetch`
4. Build the flynncade helper: `cd marketplace/apps/flynncade && make`
5. `mkdir build && cd build && cmake ..`
6. `make alloplace2; and bash -c "cd ..; env ALLOPLACE_NAME=\"My place\" ./build/alloplace2"`


## Xcode on a Mac

1. `git submodule update --init --recursive`
2. Run `marketplace/scripts/bootstrap.sh`
3. `cd placesettings && allo/assist fetch`
4. Build the flynncade helper: `cd marketplace/apps/flynncade && make`
    - On M1 mac this might entail `arch --x86_64 bash` and `CFLAGS="-I/usr/local/Cellar/ffmpeg/4.4.1_5/include/" LDFLAGS="-L/usr/local/Cellar/ffmpeg/4.4.1_5/lib/ -lavresample -lavcodec -lswresample"  make`
5. `mkdir build && cd build && cmake -GXcode ..`
6. Set a custom work directory in "Edit Scheme" -> Options: "$(BUILT_PRODUCTS_DIR)/../.."
7. Run the `alloplace2` target
