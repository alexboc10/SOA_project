##############################################################################

Course: Advanced Operative Systems
A.Y.: 2020/2021
Author: Alessandro Boccini (0277414)
Description: subsystem of tag-based threads data exchange

###############################################################################

This project has been developed in the context of Ubuntu 20.10 OS, installed
on a VM (VMware). The Linux version kernel is 5.8.

The module initialization can be performed executing install.sh script. It
compiles the files, installs the module and executes dmesg.

The module termination can be performed executing uninstall.sh script. It
uninstalls the module and executes dmesg.

The subsystem is handled in tag_service.c file. This is the entry and the exit point of
the corresponding module. There are 3 directories:
  - lib: all the function libraries
  - include: all the header files referring to libraries
  - syscalls: the 4 syscalls requested by the service

In include/ directory the files contain data structure useful for the service.

The TST (Tag Service Table) is the memory location for all the active tag services.
The syscalls allocation follows the course examples about syscall table hacking.
The TST_handler.c file contains all the needed logic for allocation and opening
service and for the sending and receiving operations.

The management of thread queues and RCU lists is based on course examples (personalized to
accomplish the project puroposes).

The user/ directory is an example of user-side service usage. It allocates some services,
opens them, send and receiving messages, removes services and awakes threads. For testing
purposes some actions generates errors, proving the coherent behaviour of the system.

For the usage of char device it is necessary to choose the Major number bound to it, 
expressed as constant in include/constants.h. If The value is 0, the system will choose
it.
