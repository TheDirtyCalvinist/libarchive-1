#!/bin/sh

# Quick hack to build libarc.a suitable for both simulator and device.

SDK_VERSION=8.1

cd arc
xcodebuild -sdk iphoneos$SDK_VERSION "ARCHS=armv7 armv7s arm64" clean build
xcodebuild -sdk iphonesimulator$SDK_VERSION "ARCHS=i386 x86_64" "VALID_ARCHS=i386 x86_64" clean build
lipo -output build/libarc.a -create build/Release-iphoneos/libarc.a build/Release-iphonesimulator/libarc.a
