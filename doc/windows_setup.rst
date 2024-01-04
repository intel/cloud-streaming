Windows Setup Guide
===================

.. contents::

.. _Git: https://git-scm.com
.. _Meson: https://mesonbuild.com
.. _MSYS2: https://www.msys2.org
.. _Windows SDK: https://developer.microsoft.com/en-us/windows/downloads/windows-sdk/
.. _Windows WDK: https://learn.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk

.. _FFmpeg: https://www.ffmpeg.org/
.. _OpenSSL: https://www.openssl.org/
.. _OWT: https://github.com/open-webrtc-toolkit/owt-client-native.git

.. |intel-flex| replace:: Intel® Data Center GPU Flex Series
.. _intel-flex: https://ark.intel.com/content/www/us/en/ark/products/series/230021/intel-data-center-gpu-flex-series.html

Overview
--------

This project provides the following ingredients of Intel Cloud Gaming Reference
Stack for Windows OS:

* **Indirect Display Driver (IDD)** to enable virtual display devices over
  headless display adapters

* **IDD setup tool** to install, uninstall and tune virtual display devices and
  drivers.

* **Screen capture and video encoding sample** application to capture and encode
  Windows desktop output and dump bitstream to file

* `OWT`_ based screen capture streaming server and client
  server

* Tool to enumerate available adapters and displays on the system

Prerequisites
-------------

To build the project configure system wide proxies if you work behind a proxy
server and install the following software:

* `Git`_
* `Meson`_
* Visual Studio 2022 or later
* v143 toolchain
* `Windows SDK`_
* `Windows WDK`_

To run the project:

* IDD driver requires Windows 10, version 1903 or newer

  * **Note:** considering this requirement, IDD driver can be used with Windows
    Server 2022, but not with Windows Server 2019

* Screen capture and video encoding should work for any Intel GPU

  * **Note:** project was validated for |intel-flex|_

Build dependencies
------------------

Some components of this project has build dependencies from 3rd party software
libraries. These build dependencies are optional in a sense that missing them
will skip configuration of the components which depend on them. The following
table lists dependencies of each component to help you understand what you
will need to build desired project configuration.

.. _enum-adapters: ../sources/apps/enum-adapters/readme.rst
.. _IDD driver: ../sources/drivers/idd/readme.rst
.. _idd-setup-tool: ../sources/apps/idd-setup-tool/readme.rst
.. _screen-grab: ../sources/apps/screen-grab/readme.rst
.. _screen-capture: ../sources/streamer/server/screen-capture/readme.rst
.. _webrtc-client: ../sources/apps/webrtc-client/readme.rst

+-------------------+-----------------------------------------------------+
| Component         | Dependencies                                        |
+===================+=====================================================+
| `enum-adapters`_  | n/a                                                 |
+-------------------+-----------------------------------------------------+
| `IDD driver`_     | `Windows WDK`_                                      |
+-------------------+-----------------------------------------------------+
| `idd-setup-tool`_ | n/a                                                 |
+-------------------+-----------------------------------------------------+
| `screen-grab`_    | * FFmpeg n6.1 or later with QSV encoders enabled    |
|                   | * libvpl                                            |
+-------------------+-----------------------------------------------------+
| `screen-capture`_ | * FFmpeg n6.1 or later with QSV encoders enabled    |
|                   | * libvpl                                            |
|                   | * OWT built with ``--cg_server`` flag               |
|                   | * nlohmann_json                                     |
|                   | * sdl2                                              |
|                   | * sioclient                                         |
+-------------------+-----------------------------------------------------+
| `webrtc-client`_  | * FFmpeg n6.1 or later with enabled dx11va decoders |
|                   | * OpenSSL                                           |
|                   | * OWT built with ``--cg_client`` flag               |
|                   | * imgui                                             |
|                   | * implot                                            |
|                   | * rapidjson                                         |
|                   | * sioclient                                         |
+-------------------+-----------------------------------------------------+

Project uses meson subprojects to manage dependencies whenever possible.
However, some dependencies must be built manually and correctly placed on the
filesystem. These are:

* `FFmpeg`_
* `OpenSSL`_
* `OWT`_

To help with build process we provide a set of `docker configurations <../docker/readme.rst>`_.
You can either use them directly or refer to them for build instructions.

**Tips:**

* IDD driver, IDD setup tool and enum adapters tool depend only on Windows SDK
  and WDK. If you need only these components you can avoid building other
  dependencies.

* `screen-grab`_ sample should be sufficient to review capture and encoding
  part of the project. To build it you need only FFmpeg and libvpl, i.e.
  you can avoid building OpenSSL and heavy-weight OWT.

Build IDD driver
----------------

Build of IDD driver is separate from other components. It can be built from
`sources/drivers/idd/IddSampleDriver.sln <../sources/drivers/idd/IddSampleDriver.sln>`_
solution with Microsoft Visual Studio 2022 or later. For details see
`IDD driver readme <../sources/drivers/idd/readme.rst>`_.

Build project with ninja backend
--------------------------------

Other components use `Meson`_ build system. Default meson backend is ninja.
Perform the following steps to build the project:

1. Clone Cloud Streaming repository::

     git clone https://github.com/projectceladon/cloud-streaming.git && cd cloud-streaming

#. Open x64 Native Tool Command Prompt for your version of Microsoft Visual
   Studio (2022 or later)

#. If behind proxy, configure proxy settings as follows::

     set http_proxy=http://some-proxy.com:911
     set https_proxy=http://some-proxy.com:911

#. Configure the project with meson as follows (use full path instead of
   ``%PREFIX%`` to set installation path)::

     # go to the top level git project folder
     cd cloud-gaming

     # configure the project
     meson setup --wrap-mode=forcefallback ^
       --prefix=%PREFIX% ^
       -Dprebuilt-path=C:\path\to\prebuilt\dependencies ^
       _build

#. Build and install the project::

     meson compile -C _build
     meson install -C _build

#. If IDD driver is needed, consider to copy it (``.inf``, ``.cat``, ``.dll``)
   to ``_build/bin/idd`` folder which is ``idd-setup-tool`` default search
   location.

You can customize your build configuration according to your needs. Refer to
https://mesonbuild.com/Builtin-options.html for details on the ``meson setup``
options. Below table provides excerpt from meson documentation on the most
useful options:

+--------------------------------------------------+--------------------------------------------------------------------+
| Option                                           | Comments                                                           |
+==================================================+====================================================================+
| ``--buildtype={debug, debugoptimized, release}`` | Build type to use. Default is **debug**.                           |
+--------------------------------------------------+--------------------------------------------------------------------+
| ``--backend={ninja, vs}``                        | Backend to use. Default is **ninja**. ``vs`` will generate VS      |
|                                                  | solutions.                                                         |
+--------------------------------------------------+--------------------------------------------------------------------+
| ``--vsenv``                                      | Activate VS environment (64-bit). Useful if you don't want         |
|                                                  | to start VS command prompt. For example, if you build under MSYS2. |
+--------------------------------------------------+--------------------------------------------------------------------+
| ``-Db_ndebug={true, false, if-release}``         | Enable or disable asserts. Default is **if-release** (for our      |
|                                                  | project).                                                          |
+--------------------------------------------------+--------------------------------------------------------------------+
| ``-Dcpp_args="arg1,arg2,..."``                   | Additional C++ args to pass to compiler. Use ``c_args`` for C.     |
+--------------------------------------------------+--------------------------------------------------------------------+
| ``-Dcpp_link_args="arg1,arg2,..."``              | Additional C++ args to pass to linker. Use ``c_link_args`` for C.  |
+--------------------------------------------------+--------------------------------------------------------------------+

Note that if you build under MSYS2 it is strongly recommended to use
``--wrap-mode=forcefallback`` setup option otherwise meson might pick
dependencies from MSYS2 and project might fail to build.

Generate Visual Studio Solution
-------------------------------

Microsoft Visual Studio solution is generated by meson if Visual Studio
backend is used - add ``--backend=vs`` to meson setup command line::

  meson setup --wrap-mode=forcefallback ^
    --backend=vs
    --prefix=%PREFIX% ^
    _build

Solution will be available under ``_build`` folder. You can open it with
Visual Studio and build the project. Note that you can still use meson
build and install commands if needed (meson will call MSBuild instead
of ninja)::

  meson compile -C _build
  meson install -C _build

Using MSYS2
-----------

`MSYS2`_ provides environment to build, install and run native Windows
software. At the moment this build environment is not fully supported. You
can't use MSYS2 toolchain to build the project. Some libraries available
under MSYS2 (like sdl2) might cause build failure.

However, you can use MSYS2 installation of meson to build the project
following below tips. Note that you still need to install Microsoft
Visual Studio and Windows SDK.

1. Install the following software under meson::

     # Assuming you run MSYS2 MINGW64 environment shell
     pacman -S \
       git \
       mingw-w64-x86_64-ca-certificates \
       mingw-w64-x86_64-meson \
       mingw-w64-x86_64-python3

#. Build and place project dependencies somewhere on the filesystem.
   **Important:** it is strongly recommended to build **all** project
   dependencies if you build under MSYS2. Otherwise meson might find
   dependencies from MSYS2 and build might fail.

#. Configure the project with meson as follows (use full path instead of
   ``$PREFIX`` to set installation path)::

     # go to the top level git project folder
     cd cloud-gaming

     # configure the project
     meson setup --wrap-mode=forcefallback ^
       --prefix=$PREFIX ^
       -Dprebuilt-path=/c/path/to/prebuilt/dependencies ^
       _build

#. Build and install the project::

     meson compile -C _build
     meson install -C _build

Note ``--wrap-mode=forcefallback`` setup option - it forces meson to fetch
dependencies defined under subfolders instead of using them from MSYS2
installation which might cause build failure.

Run software
------------

Refer to each component readme for details on how to use it:

* `Indirect Display Driver <../sources/drivers/idd/readme.rst>`_
* `IDD Setup Tool <../sources/apps/idd-setup-tool/readme.rst>`_
* `Video Adapters Enumeration Tool <../sources/apps/enum-adapters/readme.rst>`_
* `Screen Grab Tool <../sources/apps/screen-grab/readme.rst>`_
* `Screen Capture Streaming Tool <../sources/streamer/server/screen-capture/readme.rst>`_
* `WebRTC (OWT) Client <../sources/apps/webrtc-client/readme.rst>`_
