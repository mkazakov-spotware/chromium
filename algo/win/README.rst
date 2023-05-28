Prerequisits
============

-  Windows 10 or newer on x86_64 or ARM_64 machine
-  checkout of ``spotware/lkgr`` branch
-  At lest 8GB of RAM, at lest 100 Gb of free disk space of NTFS volume
-  10.0.22621.0 Windows 11 SDK, 10.0.22621.755 Windows 11 SDK Debugging Tools
-  Visual Studio 2022 Community with "Desktop development with C++”
   component and the “MFC/ATL support” sub-components for x86, for arm the same and
   ARM64 Tools, MFC.ARM64 sub-components


Instructions
============

Download the depot_tools bundle
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Download the depot_tools bundle
https://storage.googleapis.com/chrome-infra/depot_tools.zip and extract it
somewhere on the disk, don't use drag-and-drop

Add depot tools to PATH env var:
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Assuming you unzipped the bundle to C:\src\depot_tools, open:
Open Control Panel → System and Security → System → Advanced system settings
Modify the PATH system variable and put C:\src\depot_tools at the front.
Also, add a DEPOT_TOOLS_WIN_TOOLCHAIN environment variable in the same way, and set it to 0.
This tells depot_tools to use your locally installed Visual Studio.

Update depot tools and install python:
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Don't use Cygwin, PowerShell, msys, git-windows, wine or any special environments,
use plain CMD instead:

.. code:: cmd

   > gclient


+---------------------------------------+------------------------------+
| Hardcoded env vars                    | Value                        |
+=======================================+==============================+
| DOTNET_gcServer                       | 1                            |
+---------------------------------------+------------------------------+
| DOTNET_gcConcurrent                   | 1                            |
+---------------------------------------+------------------------------+
| DOTNET_GCCpuGroup                     | 1                            |
+---------------------------------------+------------------------------+
| DOTNET_Thread_UseAllCpuGroups         | 1                            |
+---------------------------------------+------------------------------+

Install dotnet SDK 6.0.15 and find libhostfxr.dylib, e.g.
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code:: shell

   $ find /usr/local/share/dotnet -name 'libhostfxr.dylib'

Open env.sh in a text editor and set \__CT_HOSTFXR_PATH to the path
found Check if ENDPOINT vars belong to the dotnet assembly to be invoked
by the host

Environment variables in env.sh:
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

+----------------------+-----------------------+-----------------------+
| Env var name         | Value                 | Description           |
+======================+=======================+=======================+
| DOTNET_SYSTEM_GLO    | 1                     | allows to escape      |
| BALIZATION_INVARIANT |                       | extra dependences     |
+----------------------+-----------------------+-----------------------+
| COMPl                | 0                     | allows to escape      |
| us_EnableDiagnostics |                       | extra communication   |
+----------------------+-----------------------+-----------------------+
| \__CT_PRODUCT_PATH   | ./out                 | relative path to      |
|                      |                       | build directory       |
+----------------------+-----------------------+-----------------------+
| \__CT_HOSTFXR_PATH   | /fxr/6.               | absolute path to fxr  |
|                      | 0.15/libhostfxr.dylib | library               |
+----------------------+-----------------------+-----------------------+
| \__CT_A              | testing_app           | folder with dotnet    |
| LGOHOST_ENDPOINT_DIR |                       | assembly              |
+----------------------+-----------------------+-----------------------+
| \__CT_A              | testing_app.dll       | name of assembly      |
| LGOHOST_ENDPOINT_ASM |                       |                       |
+----------------------+-----------------------+-----------------------+
| \__CT_ALGO           | testing_a             | name of runtime       |
| HOST_ENDPOINT_CONFIG | pp.runtimeconfig.json | config                |
+----------------------+-----------------------+-----------------------+
| \__CT_AL             | testing_app.Program,  | class and namespace   |
| GOHOST_ENDPOINT_TYPE | testing_app           |                       |
+----------------------+-----------------------+-----------------------+
| \__CT_ALGO           | ReverseLine           | method to be invoked  |
| HOST_ENDPOINT_METHOD |                       |                       |
+----------------------+-----------------------+-----------------------+

Source env.sh:
~~~~~~~~~~~~~~

.. code:: shell

   $ source ./env.sh

Build the project for current host:
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code:: shell

   $ make

Build the project for ARM64 on x86_64:
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code:: shell

   $ ARCH=aarch64 make

Build the project for x86_64 on ARM64:
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code:: shell

   $ ARCH=x86_64 make

Run sandbox:
~~~~~~~~~~~~

.. code:: shell

   $ ./out/mac_algohost
