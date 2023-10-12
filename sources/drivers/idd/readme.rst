Indirect Display Driver
=======================

.. contents::

.. |intel-flex| replace:: Intel® Data Center GPU Flex Series
.. _intel-flex: https://ark.intel.com/content/www/us/en/ark/products/series/230021/intel-data-center-gpu-flex-series.html

.. _Windows SDK: https://developer.microsoft.com/en-us/windows/downloads/windows-sdk/
.. _Windows WDK: https://learn.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk
.. _devcon: https://learn.microsoft.com/en-us/windows-hardware/drivers/devtest/devcon

.. _IddSampleDriver.sln: ./IddSampleDriver.sln
.. _idd_io.h: ./uapi/idd_io.h
.. _IddCxAdapterSetRenderAdapter: https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/iddcx/nf-iddcx-iddcxadaptersetrenderadapter

.. _idd-setup-tool: ../../apps/idd-setup-tool

Software Requirements
---------------------

To build the driver use `IddSampleDriver.sln`_ Visual Studio solution.
For the successful build you need:

* Microsoft Visual Studio 2022 or later
* `Windows SDK`_
* `Windows WDK`_

While the project as a whole might have different requirements, IDD driver
itself requires the following version of Windows operating system to work
properly:

* Windows 10, version 1903 or newer

This requirement is implied by usage of certain Windows driver API
such as `IddCxAdapterSetRenderAdapter`_.

Hardware Requirements
---------------------

Indirect Display Driver (IDD) is designed to provide virtual display over
headless display adapters. Supported display adapters:

* |intel-flex|_

Requirement over supported display adapters is implied by `idd-setup-tool`_ -
a tool for installation and tuning of IDD virtual display devices. IDD driver
does not have limitations on what physical display adapter it can be paired
with. If IDD driver is installed without use of  `idd-setup-tool`_ (for example
with Microsoft `devcon` utility), virtual display to physical display adapter
pairing needs to be done manually by calling IDD driver API defined in
`idd_io.h`_ header file.

Usage
-----

IDD driver build produces the following output files:

* IddSampleDriver.dll - driver itself
* IddSampleDriver.inf - driver setup information file
* iddsampledriver.cat - driver catalog file

**Built driver is not signed.** In order to install the driver you need to
sign it or install the driver in Windows Test mode.

Driver signing process is not covered in this documentation. To enable 
Windows test mode, execute the following as Administrator and reboot::

  bcdedit /set testsigning on

After reboot you can install unsigned IDD driver. We recommend to use
`idd-setup-tool`_ utility to install, uninstall and customize installed IDD
driver. Basic install can be done with::

  idd-setup-tool install --location=/path/to/idd/driver

**Note:** If location argument is not specified, `idd-setup-tool`_ will use
the following location for idd driver: ``idd\`` folder relative to the tool
executable.

When installing for the first time you will be prompted to trust the
publisher - click accept. `idd-setup-tool`_ will create as many virtual
displays as there are physical display adapters on the system and pair each
IDD virtual display to physical adapter. You can customize installation
by changing number of virtual displays, their initial resolution and other
settings. Refer for `idd-setup-tool`_ documentation for details.

To uninstall all IDD devices and drivers execute::

  idd-setup-tool uninstall

Registry Keys
-------------

IDD driver support the following registry keys which control its behavior:

+------------------+-----------+---------------+---------------------------+-------------------------------------+
| Key              | Type      | Default value | Supported values          | Description                         |
+==================+===========+===============+===========================+=====================================+
| IddCustomControl | REG_DWORD | 1             | * ``1`` : 1080p Monitor   | Controls maximum resolution         |
|                  |           |               | * ``2`` : 1440p Monitor   | available on IDD device             |
|                  |           |               | * ``4`` : 2160p Monitor   |                                     |
+------------------+-----------+---------------+---------------------------+-------------------------------------+
| IddMonitorNumber | REG_DWORD | 1             | * ``1`` : 1 monitor       | Controls number of exposed monitors |
|                  |           |               | * ``2`` : 2 monitors      |                                     |
+------------------+-----------+---------------+---------------------------+-------------------------------------+
| IddCursorControl | REG_DWORD | 0             | * ``0`` : Software cursor | Controls exposed cursor type        |
|                  |           |               | * ``1`` : Hardware cursor |                                     |
+------------------+-----------+---------------+---------------------------+-------------------------------------+

These registry keys are available for all IDD device instances at the following
location in the registry:

* ``HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Enum\ROOT\DISPLAY\0000\Device Parameters``
* ``HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Enum\ROOT\DISPLAY\0001\Device Parameters``
* ``HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Enum\ROOT\DISPLAY\0002\Device Parameters``
* and so on for each IDD instance

You can change registry settings from the command shell::

  reg add "HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Enum\ROOT\DISPLAY\0000\Device Parameters" ^
    /v IddCustomControl /t REG_DWORD /d 2 /f

Above example will change maximum resolution available on IDD device ``0000``
to 1440p. In order for the change to take effect you need to disable and
enable back this device to trigger IDD driver reload. You can do that either
from the Windows Device Manager (targeting specific device) or from command
line using `devcon`_ (targeting all IDD devices)::

  devcon.exe disable "root\iddsampledriver"
  devcon.exe enable "root\iddsampledriver"

Known issues
------------

Intel® Data Center GPU Flex Series
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Applications that use DirectX 12 API for rendering may have visual artifacts
if IDD virtual display is configured to work with |intel-flex|_ display adapters.
To workaround this issue set ``IndirectDisplaySupport`` DWORD registry key to 1
for |intel-flex|_ devices::

  reg add "HKLM\SYSTEM\CurrentControlSet\Control\Class\{4d36e968-e325-11ce-bfc1-08002be10318}\0000" ^
    /v IndirectDisplaySupport /t REG_DWORD /d 0x1 /f

Repeat this for all |intel-flex|_ devices present on a system. Then first disable IDD
devices, next disable |intel-flex|_ devices, after that enable them back in
the reverse order, i.e. first enable |intel-flex|_ devices then IDDs.
