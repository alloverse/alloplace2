# Running

## XCODE 

1. Get all the submodules
2. Run marketplace/scripts/bootstrap.sh
3. `cd placesettings && allo/assist fetch`
3. Build the flynncade helper `cd marketplace/apps/flynncade && make`
    - On M1 mac this might entail `arch --x86_64 bash` and `CFLAGS="-I/usr/local/Cellar/ffmpeg/4.4.1_5/include/" LDFLAGS="-L/usr/local/Cellar/ffmpeg/4.4.1_5/lib/ -lavresample -lavcodec -lswresample"  make`
4. `mkdir build && cd build && cmake -GXcode ..`
5. Set a custom work directory in "Edit Scheme" -> Options: "$(BUILT_PRODUCTS_DIR)/../.."

