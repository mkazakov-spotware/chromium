Prerequisits
============

-  Windows 10 or newer on x86_64 machine
-  At least 8GB of RAM, 16GB is recommended, at least 100 Gb of free disk space of NTFS volume
-  10.0.22621.0 Windows 11 SDK, 10.0.22621.755 Windows 11 SDK Debugging Tools
-  Visual Studio 2022 Community with "Desktop development with C++”
   component and the “MFC/ATL support” sub-components for x86, for arm the same plus
   ARM64 Tools, MFC.ARM64 sub-components (MFC arm64 latest, MSVC arm64 build tools)
-  Dotnet SDK 6.0.15


Instructions
============

Download the depot_tools bundle
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Download the depot_tools bundle
https://storage.googleapis.com/chrome-infra/depot_tools.zip and extract it
somewhere on the disk, don't use drag-and-drop

Add depot tools to ``PATH`` env var:
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Assuming you unzipped the bundle to ``C:\\src\\depot_tools``, open:
Open Control Panel → System and Security → System → Advanced system settings
Modify the PATH system variable and put ``C:\\src\\depot_tools`` at the front.
Also, add a ``DEPOT_TOOLS_WIN_TOOLCHAIN`` environment variable in the same way, and set it to ``0``.
This tells depot_tools to use your locally installed Visual Studio.

Update depot tools and install python:
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Don't use Cygwin shell, PowerShell, Msys shell, git-windows, Wine or any special environments,
use plain CMD instead:

.. code:: batch

   > gclient

Create ``chromium`` folder for the checkout and change to it:
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
.. code:: batch

   > mkdir chromium & cd chromium

Create gclient config:
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
.. code:: batch

   > gclient config --name=src https://github.com/alexrttr/chromium.git

Make a shallow clone of repository:
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code:: batch

   > git clone --depth 120 -b spotware/lkgr https://github.com/alexrttr/chromium.git src

Run synchronization process:
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code:: batch

   > gclient sync --no-history --revision 95106575c28dfac96540ffe988707f002dad6c3b

Where ``95106575c28dfac96540ffe988707f002dad6c3b`` is the latest commit on this branch

Move to ``src`` folder:
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code:: batch

   > cd src

Generate ninja projects:
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code:: batch

   > mkdir out

For x86_64:

.. code:: batch

   > mkdir out\x86_64
   > gn gen --filters="algo\win:launcher;algo\win:the_one" out\x86_64

For x86:

.. code:: batch

   > mkdir out\x86
   > gn gen --filters="algo\win:launcher;algo\win:the_one" args="target_cpu=\"x86\"" out\x86

For arm64:

.. code:: batch

   > mkdir out\arm64
   > gn gen --filters="algo\win:launcher;algo\win:the_one" args="target_cpu=\"arm64\"" out\arm64

Build the sandbox projects:
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

For arm64:

.. code:: batch

   > autoninja -C out\arm64 algo\win:the_one algo\win:launcher

Build dotnet testing project:
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code:: batch

   > mkdir out\testing_app
   > dotnet build algo\linux\testing_app\testing_app.csproj -o out\testing_app

Modify environment variables according to your environment:
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In algo\\win\\env.bat:

+----------------------+-----------------------+-----------------------+
| Env var name         | Value                 | Description           |
+======================+=======================+=======================+
|  __CT_PRODUCT_PATH   | C:\\Users\\admin\\    | absolute path to      |
|                      |  chromium\\src\\out   | build directory       |
+----------------------+-----------------------+-----------------------+
|  __CT_HOSTFXR_PATH   | C:\\Program Files     | absolute path to fxr  |
|                      |  \\dotnet\\hostfxr.dll| library               |
+----------------------+-----------------------+-----------------------+
|  __CT_DOTNET_PATH    | C:\\Program Files     | absolute path to      |
|                      |  \\dotnet             | dotnet installation   |
+----------------------+-----------------------+-----------------------+
|  __CT_DOTNET         |           1           | server based garbage  |
|        _gcServer     |                       | collection            |
+----------------------+-----------------------+-----------------------+
|  __CT_DOTNET         |           1           | background garbage    |
|       _gcConcurrent  |                       | collection            |
+----------------------+-----------------------+-----------------------+
|  __CT_DOTNET         |           1           | multiple CPU groups   |
|      _GCCpuGroup     |                       | support               |
+----------------------+-----------------------+-----------------------+
|  __CT_DOTNET_Thread  |           1           | all CPU groups        |
|      _UseAllCpuGroups|                       | are used              |
+----------------------+-----------------------+-----------------------+

In algo\\win\\revstr_env.bat:

+----------------------+-----------------------+-----------------------+
| Env var name         | Value                 | Description           |
+======================+=======================+=======================+
|  __CT_ALGOHOST       | testing_app           | folder with dotnet    |
|        _ENDPOINT_DIR |                       | assembly              |
+----------------------+-----------------------+-----------------------+
|  __CT_ALGOHOST       | testing_app.dll       | name of assembly      |
|        _ENDPOINT_ASM |                       |                       |
+----------------------+-----------------------+-----------------------+
|  __CT_ALGOHOST       | testing_app           | name of runtime       |
|     _ENDPOINT_CONFIG |   .runtimeconfig.json | config                |
+----------------------+-----------------------+-----------------------+
|  __CT_ALGOHOST       | testing_app.Program,  | class and namespace   |
|       _ENDPOINT_TYPE | testing_app           |                       |
+----------------------+-----------------------+-----------------------+
|  __CT_ALGOHOST       | ReverseLine           | method to be invoked  |
|     _ENDPOINT_METHOD |                       |                       |
+----------------------+-----------------------+-----------------------+
|  __CT_ALGOHOST       | Preload               | preload method name   |
|     _PRELOAD_METHOD  |                       |                       |
+----------------------+-----------------------+-----------------------+


Open ``env.bat`` and ``revstr_env.bat`` in a text editor and update corresponding environment varialbles.

Source ``env.bat`` and ``revstr_env.bat``:
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code:: batch

   > algo\win\env.bat
   > algo\win\revstr_env.bat

Run the project:
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

For x86:

.. code:: batch

   > out\x86\launcher.exe
