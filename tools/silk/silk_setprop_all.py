#!/usr/bin/env python

import os

def silk_setprop(t, v):
    for i in range(len(v)):
        os.system('adb shell setprop ' + t[i] + ' ' + str(v[i]))

def silk_getprop():
    os.system('adb shell getprop | grep silk')

if __name__ == '__main__':
    value = [
            0,  #hw2vsync
            0,  #vd
            0,  #input.scope
            0,  #compositor.scope
            0,  #rd.scope
            0,  #rd.scope.client
            0,  #input.latency
            0,  #compositor.latency
            0,  #rd.latency
            0,  #ipc
            0,  #vsync.duration
            0   #time.log
            ]
    tag = [
            'silk.hw2vsync',
            'silk.vd',
            'silk.i.scope',
            'silk.c.scope',
            'silk.r.scope',
            'silk.r.scope.client',
            'silk.i.lat',
            'silk.c.lat',
            'silk.r.lat',
            'silk.ipc',
            'silk.vsync',
            'silk.timer.log'
            ]
    print('Set properities...');
    silk_setprop(tag, value)
    print('Set properities done');
    print('results:')
    silk_getprop()
