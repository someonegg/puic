PUIC, Pure-QUIC, QUIC without SPDY
==================================

See [Chromium QUIC Page](http://www.chromium.org/quic) for detailed information about QUIC.

The QUIC's implementation code comes from 
[Chromium's QUIC Implementation](https://chromium.googlesource.com/chromium/src.git/+/master/net/quic/),
the version used is "65.0.3300".
We will update to [quiche](https://quiche.googlesource.com/quiche/) when it is ready.

We made serveral modifications to minimize dependencies, notable dependencies are [BoringSSL](https://boringssl.googlesource.com/).

The BoringSSL sources is already embedded in this repository and built by default, you can
trun off the build by setting "PUIC_NO_EMBEDDED_BORINGSSL".

## Cross Compiling for Android with the NDK

```
1. export ANDROID_NDK=path/to/ndk
2. mkdir build.android.xxx
3. cd build.android.xxx
4. cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=1 -DCMAKE_TOOLCHAIN_FILE=../cmake_toolchain/android.xxx ..
5. make
```

## Cross Compiling for iOS with XCode

```
1. mkdir build.ios.std
2. cd build.ios.std
3. cmake -DCMAKE_TOOLCHAIN_FILE=../cmake_toolchain/ios.std -GXcode ..
4. Open XCode and build the puicxxx target
```

## Compiling for Windows

```
1. Windows and VS2015+
2. * See boringssl/BUILDING.md, install perl + ninja + yasm + go.
3. Run "VS2015 xxx xxxx Tools Command Prompt" from "Start Menu"
4. mkdir build.windows.xxx
5. cd build.windows.xxx
6. cmake .. -GNinja
7. ninja
```
