# Pico Log

Minimalistic logging library for debugging purposes. It leverages a dedicated task and a push-pull design.

The log statements are pushed into a buffer as they are created - this is as fast as memory copy speed. The buffer 
is streamed out to Serial port (when enabled) using a dedicated task. This design isolates the limitations of Serial port speed 
from impacting the timing of actions in application - with or without logging enabled. Note, however, that the logging buffer is 
synchronized - only one read or write operation is allowed at a time. Therefore, there is a minimal performance impact to the 
timing of the log source.

It works great as long as the push volume doesn't exceed the ability of the buffer to stream out at Serial port speeds. 
The buffer is of circular type - meaning that if volume of data to push is more than available space of the buffer, 
the older characters will be overwritten to make room for incoming log data.  