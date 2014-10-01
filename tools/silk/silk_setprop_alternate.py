#!/usr/bin/env python

import os

def silk_setprop(prop_name, value):
    os.system('adb shell setprop %s %d' % (prop_name, value))

if __name__ == '__main__':
    option = [
            ['silk.timer.log', "Print time log", 0],
            ['silk.timer.log.raw', "Print raw time log", 0],
            ['silk.scrollbar.hide', "Hide scrollbar", 0],
            ]

    pref = [
            ['silk.hw2vsync', "Hwc to VD latency"],
            ['silk.vd', "VD dispatch function cost"],
            ['silk.i.scope', "Input dispatch function cost"],
            ['silk.c.scope', "Compositor dispatch function cost"],
            ['silk.r.scope', "Chrome RD dispatch function cost"],
            ['silk.r.scope.client', "Content RD dispatch functin cost"],
            ['silk.i.lat', "VD to input module latency"],
            ['silk.c.lat', "VD to compositor module latency"],
            ['silk.r.lat', "VD to Chrome RD module latency"],
            ['silk.ipc', "VD to client ipc latency"],
            ['silk.vsync', "Vsync event interval"],
            ['silk.input.pos', "Print input pos log"],
            ]

    #clear all prop
    for i in range(len(option)):
        silk_setprop(option[i][0], 0);
    for i in range(len(pref)):
        silk_setprop(pref[i][0], 0);

    #set test option
    print('Use option:')
    for i in range(len(option)):
        print('\t%s : %d' % (option[i][1], option[i][2]))
        silk_setprop(option[i][0], option[i][2]);

    #start to run all config
    print('Start test....')
    for i in range(len(pref)):
        print('setprop %s : %d (%s)' % (pref[i][0], 1, pref[i][1]))
        silk_setprop(pref[i][0], 1)
        print('press enter to continue')
        os.system('read input')
        silk_setprop(pref[i][0], 0)

