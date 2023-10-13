IDD Setup Tool
==============

.. contents::

.. _IDD: ../../drivers/idd

.. |intel-flex| replace:: Intel® Data Center GPU Flex Series
.. _intel-flex: https://ark.intel.com/content/www/us/en/ark/products/series/230021/intel-data-center-gpu-flex-series.html

idd-setup-tool is a tool to install, uninstall and tune virtual `IDD`_ devices
and displays. Tool supports the following commands:

+--------------+--------------+--------------+
| `install`_   | `uninstall`_ | `set`_       |
+--------------+--------------+--------------+
| `enable`_    | `disable`_   | `pair`_      |
+--------------+--------------+--------------+
| `rearrange`_ | `show`_      |              |
+--------------+--------------+--------------+

**NOTE:** `install`_ and `set`_ commands do not work under non-interactive
shells.

Some commands allow to select displays/adapters with patterns.
Supported patterns are:

+----------------+--------------------------------------------------------------+
| Pattern        | Mapping                                                      |
+================+==============================================================+
| ``msft``       | Microsoft Basic Adapter or Display                           |
+----------------+--------------------------------------------------------------+
| ``virtio``     | Red Hat VirtIO GPU DOD controller and Red Hat QXL controller |
+----------------+--------------------------------------------------------------+
| ``flex``       | |intel-flex|_ adapter                                        |
+----------------+--------------------------------------------------------------+
| ``non-flex``   | All except |intel-flex|_ adapter                             |
+----------------+--------------------------------------------------------------+
| ``idd``        | `IDD`_ device or display                                     |
+----------------+--------------------------------------------------------------+
| ``non-idd``    | All except `IDD`_ device or display                          |
+----------------+--------------------------------------------------------------+
| ``<pattern>N`` | Select specified adapter/display by its index, for example   |
|                | ``idd1``.                                                    |
+----------------+--------------------------------------------------------------+

Global options
--------------

General usage for the tool is the following::

  idd-setup-tool.exe <command> [<options>] [<global-options>]

The following options apply to all commands supported by the tool.

-h, --help
    Print help

-v, --verbose
    Turn on additional logging (default: off)

-y, --yes
    Assume "yes" on all prompts (default: off)

--delay=<milliseconds>
    Applies delay (in milliseconds) after every action that
    changes display or adapter states (default: ``2000ms``)

Command Reference
-----------------

install
~~~~~~~

Usage::

  idd-setup-tool.exe install [<options>] [<global-options>]

This command installs `IDD`_ virtual display device for each IDD compatible
physical display adapter available on the system. Supported adapters:

* |intel-flex|_

If IDD driver is not signed installation will fail. You either need to sign the
driver (signing process is not covered in this documentation) or enable Windows
test mode with the following command then reboot (run from cmd shell as
Administrator)::

  bcdedit /set testsigning on

After reboot you should be able to install unsigned IDD driver. `install`_
command will first uninstall any previously installed IDD virtual displays.
After installation each IDD virtual display will be paired with the compatible
physical display adapter. If more than one physical display adapter is
present on the system, the command will install one virtual display for
each compatible physical display adapter.

Using ``--resolution`` and ``--scale`` options you can configure IDD virtual
display resolution and scaling respectively.

Using ``--disable-adapter`` and ``--disable-display`` you can disable specific
display adapters or displays. These options are useful to disable non-IDD
display adapters or displays such as Microsoft Basic Display Adapter.

Examples:

* Install `IDD`_ virtual display(s) with 1080p resolution and 100% scaling and
  disable Microsoft Basic Display Adapter and corresponding display::

    idd-setup-tool.exe install -y --trust ^
      --resolution=1920x1080 --scale=100 --rearrange ^
      --disable-adapter=msft --disable-display=msft

Options:

--disable-adapter=<VALUE,VALUE,...>
    Disable specified display adapter (options: ``msft``, ``idd``, ``flex``)

--disable-display=<VALUE,VALUE,...>
    Disable specified display (options: ``non-flex``, ``msft``,
    ``idd``, ``virtio``, ``non-idd``)

--location=</path/to/idd>
    Location of IDD driver (.inf, .cat, .dll) to install
    (default: ``idd`` folder relative to idd-setup-tool executable)

--rearrange
    Rearrange displays horizontally and set leftmost as primary

--resolution=<WxH>
    Configure resolution for the display (default: use system default)

--scale=<SCALE%>
    Configure scaling for the display (default: use system default)

--trust
    Extract IDD certificate (from IDD driver .cat file) and add it to the
    trustred store. These actions are not performed by default.

install command limitations
^^^^^^^^^^^^^^^^^^^^^^^^^^^

* This command requires interactive shell to work properly.

* Pairing of IDD virtual display with physical display adapters does not
  preserve over reboot. To workaround the issue rerun the pairing with the
  following command::

    idd-setup-tool.exe pair

uninstall
~~~~~~~~~

Usage::

  idd-setup-tool.exe uninstall [<options>] [<global-options>]

This command uninstalls IDD virtual display devices and optionally enables
display adapters/displays that were previously disabled.

Options:

--enable-adapter=<VALUE,VALUE,...>
    Enable specified display adapter (options: ``msft``, ``idd``)

--enable-display=<VALUE,VALUE,...>
    Enable specified display (options: ``msft``, ``idd``, ``virtio``,
    ``non-idd``)

set
~~~

Usage::

  idd-setup-tool.exe set <settings>... [<global-options>]

This command applies specified settings to displays available on the system.
If the ordering in which settings are applied is important, this command
should be executed multiple times in the desired order. Default
configuration order is resolution then scaling.

Settings:

+----------------------+---------------------------------------------+
| ``resolution=<WxH>`` | Configure resolution for available displays |
+----------------------+---------------------------------------------+
| ``scale=<SCALE%>``   | Configure scaling for available displays    |
+----------------------+---------------------------------------------+

set command limitations
^^^^^^^^^^^^^^^^^^^^^^^

* This command requires interactive shell to work properly.

enable
~~~~~~

Usage::

  idd-setup-tool.exe enable <settings>... [<global-options>]


This command enables specified adapters and/or displays.

Settings:

+-------------------------------+---------------------------------------------------+
| ``adapter=<VALUE,VALUE,...>`` | Enable specified display adapter (patterns:       |
|                               | ``msft``, ``idd``, ``flex``, ``<pattern>N``)      |
+-------------------------------+---------------------------------------------------+
| ``display=<VALUE,VALUE,...>`` | Enable specified display (patterns: ``non-flex``, |
|                               | ``msft``, ``idd``, ``virtio``, ``non-idd``,       |
|                               | ``<pattern>N``)                                   |
+-------------------------------+---------------------------------------------------+

disable
~~~~~~~

Usage::

  idd-setup-tool.exe disable <settings>... [<global-options>]

This command disables specified adapters and/or displays.

Settings:

+-------------------------------+---------------------------------------------------+
| ``adapter=<VALUE,VALUE,...>`` | Disable specified display adapter (patterns:      |
|                               | ``msft``, ``idd``, ``flex``, ``<pattern>N``)      |
+-------------------------------+---------------------------------------------------+
| ``display=<VALUE,VALUE,...>`` | Disable specified display (options: ``non-flex``, |
|                               | ``msft``, ``idd``, ``virtio``, ``non-idd``,       |
|                               | ``<pattern>N``)                                   |
+-------------------------------+---------------------------------------------------+

pair
~~~~

Usage::

  idd-setup-tool.exe pair [<options>] [<global-options>]

Iterates over IDD compatible display adapters and pairs them with IDD virtual
displays. If there are more installed IDD virtual displays than IDD compatible
display adapters, the process starts over so some display adapters will be
assigned more that one IDD virtual display.

We strongly recommend to rerun pairing explicitly anytime the following occurs:

* when an IDD device is disabled/enabled
* when any display adapter is disabled/enabled
* when system is rebooted (see `pair command limitations`_)

pair command limitations
^^^^^^^^^^^^^^^^^^^^^^^^

* This command may re-enable any previously disabled displays. It is
  recommended to check and disable any unwanted displays after running this
  command.

* Pairing of IDD virtual displays with physical display adapters is not
  preserved after reboot. Rerun the pairing after reboot as needed.

rearrange
~~~~~~~~~

This command rearranges displays horizontally, and sets the leftmost display
as the primary.

show
~~~~

Usage::

  idd-setup-tool.exe show [<options>] [<global-options>]

This command prints various adapter and display information.

Options:

--count=<yes|no>
    Print number of IDD compatible adapters on the system (default: ``yes``)

--adapters=<yes|no>
    Print adapters information (default: ``yes``)

--displays=<yes|no>
    Print displays information (default: ``yes``)
