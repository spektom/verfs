VerFS
=====
Simple versioning file system as Linux kernel module.

Requirements:
-------------

1. Kernel >= 2.6.7
2. GCC

Usage
------
To compile the module, run:

  `make`

To install, run:

  `make install`

Module can be loaded using:

  `modprobe verfs`

Once module is loaded, every file remove operation will create a backup copy:

<pre>
  $ touch new
  $ rm new
  $ ls new*
  new#1
</pre>

To unload module, run:

  `rmmod verfs`

