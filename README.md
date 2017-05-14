logfuse
=======
logfuse is a pass-through logging filesystem for FUSE for macOS.

It is distributed under the MIT licence.


Usage
-----
	sudo ./logfuse /Volumes/test -omodules=threadid:subdir,subdir=/tmp/somewhere -oallow_other,native_xattr,volname=Testing

This will mount /tmp/somewhere as /Volumes/test, visible on the desktop as a "Testing" volume.

The "Testing" volume will appear to contain the files from /tmp/somwhere. Any filesystem operations
on that volume will be logged to Console.app.
