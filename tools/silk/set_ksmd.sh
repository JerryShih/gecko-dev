#!/bin/bash
echo adb shell "echo $1 > /sys/kernel/mm/ksm/run"
adb shell "echo $1 > /sys/kernel/mm/ksm/run"
