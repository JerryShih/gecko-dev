#!/bin/bash

echo "adb shell setprop silk.hw2vsync 1"
adb shell setprop silk.hw2vsync 1
echo "press enter to continue.."
read input
adb shell setprop silk.hw2vsync 0

echo "adb shell setprop silk.vd 1"
adb shell setprop silk.vd 1
echo "press enter to continue.."
read input
adb shell setprop silk.vd 0

echo "adb shell setprop silk.i.scope 1"
adb shell setprop silk.i.scope 1
echo "press enter to continue.."
read input
adb shell setprop silk.i.scope 0

echo "adb shell setprop silk.c.scope 1"
adb shell setprop silk.c.scope 1
echo "press enter to continue.."
read input
adb shell setprop silk.c.scope 0

echo "adb shell setprop silk.r.scope 1"
adb shell setprop silk.r.scope 1
echo "press enter to continue.."
read input
adb shell setprop silk.r.scope 0

echo "adb shell setprop silk.r.scope.client 1"
adb shell setprop silk.r.scope.client 1
echo "press enter to continue.."
read input
adb shell setprop silk.r.scope.client 0

echo "adb shell setprop silk.i.lat 1"
adb shell setprop silk.i.lat 1
echo "press enter to continue.."
read input
adb shell setprop silk.i.lat 0

echo "adb shell setprop silk.c.lat 1"
adb shell setprop silk.c.lat 1
echo "press enter to continue.."
read input
adb shell setprop silk.c.lat 0

echo "adb shell setprop silk.r.lat 1"
adb shell setprop silk.r.lat 1
echo "press enter to continue.."
read input
adb shell setprop silk.r.lat 0

echo "adb shell setprop silk.ipc 1"
adb shell setprop silk.ipc 1
echo "press enter to continue.."
read input
adb shell setprop silk.ipc 0

echo "adb shell setprop silk.vsync 1"
adb shell setprop silk.vsync 1
echo "press enter to continue.."
read input
adb shell setprop silk.vsync 0
