# Overview
Extension of the default [Scheduler library](https://github.com/arduino/ArduinoCore-mbed/tree/main/libraries/Scheduler) that allows 
a setup task alongside the main loop task. Enables both setup and loop functions to take place on the same (new) thread.

This utility is a extending clone rather than a subclass of the initial `SchedulerClass` due to the need to access private members of the scheduler - namely the threads array.

# Use
Same API as described in the [standard documentation](http://www.arduino.cc/en/Reference/Scheduler) with the additional `startLoop` overload that takes an extra task to run once for setup, before the loop task.

# References
Have a look at [ArduinoThread](https://github.com/ivanseidel/ArduinoThread) library for a more complex way to interact with threads.