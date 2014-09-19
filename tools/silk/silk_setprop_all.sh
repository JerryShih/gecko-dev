#!/bin/bash

value[0]=0  #hw2vsync
value[1]=0  #vd
value[2]=0  #input.scope
value[3]=0  #compositor.scope
value[4]=0  #rd.scope
value[5]=0  #rd.scope.client
value[6]=0  #input.latency
value[7]=0  #compositor.latency
value[8]=0  #rd.latency
value[9]=0  #ipc
value[10]=0 #vsync.duration
value[11]=0 #time.log
value[12]=0 #time.log.raw
value[13]=0 #time.log,input

silk_setprop() {
  adb shell setprop silk.hw2vsync ${1}
  adb shell setprop silk.vd ${2}
  adb shell setprop silk.i.scope ${3}
  adb shell setprop silk.c.scope ${4}
  adb shell setprop silk.r.scope ${5}
  adb shell setprop silk.r.scope.client ${6}
  adb shell setprop silk.i.lat ${7}
  adb shell setprop silk.c.lat ${8}
  adb shell setprop silk.r.lat ${9}
  adb shell setprop silk.ipc ${10}
  adb shell setprop silk.vsync ${11}
  adb shell setprop silk.timer.log ${12}
  adb shell setprop silk.timer.log.raw ${13}
  adb shell setprop silk.timer.log.input ${14}
}

echo "set properities..."
silk_setprop ${value[@]}
echo "set properities finish"

echo "results:"
adb shell getprop | grep silk
