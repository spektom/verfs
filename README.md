VerFS
=====

Simple versioning file system as Linux kernel module.


### Usage example ###

Once this Kernel module is loaded, every file deletion operation will create a backup copy:

```
  $ touch new
  $ rm new
  $ ls new*
  new#1
```

### How this stuff works ###

This Kernel module patches *sys_call_table* by replacing _open_ and _unlink_ system calls with customized versions.


### Requirements ###

1. Kernel >= 2.6.7
2. Kernel sources placed in /usr/src/linux-&lt;version&gt;
3. GCC

### Installation ###

```bash
make && make install
```

### Loading module ###

```bash
modprobe verfs
```

### Unloading module ###

```bash
rmmod verfs
```

