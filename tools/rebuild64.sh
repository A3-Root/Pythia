set -e

# Native Linux x64 build. Links the system Python (install python3.<minor>-dev).
# No Docker / bundled interpreter is involved anymore.
rm -rf ninja
mkdir ninja
cd ninja
cmake -G Ninja -DUSE_64BIT_BUILD=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
ninja
