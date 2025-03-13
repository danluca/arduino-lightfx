# Filesystem Task

FilesystemTask is a _synchronized_ wrapper around LittleFS filesystem that uses a dedicated task for accessing LittleFS and filesystem. LittleFS is known not to be thread safe.

It allows multi-thread calls by essentially funneling them through the dedicated task one at a time, allowing LittleFS to operate on the same thread/task.

