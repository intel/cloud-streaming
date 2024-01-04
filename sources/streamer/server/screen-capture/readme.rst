Screen Capture Streaming Tool
=============================

.. contents::

.. _FFmpeg: https://www.ffmpeg.org/
.. _OWT: https://github.com/open-webrtc-toolkit/owt-client-native

Screen Capture is a tool to capture, encode and stream Windows desktop using
WebRTC (OWT). It differs from `Screen Grab Tool <../../../apps/screen-grab/readme.rst>`_
by having streaming capability while ``screen-grab`` can only dump encoded
bitstream to a file. Use `WebRTC (OWT) Client <../../../apps/screen-grab/readme.rst>`_
application to receive captured stream and interact with remote server.

Screen Capture tool supports encoding with AVC, HEVC and AV1.

Usage
-----

::

  screen-capture.exe [<options>] config_file

Options:

-h, --help
    Print help

--logfile file
    File to use for logging. ``PID`` in the file name will be substituted
    with process ID.

--loglevel <error|warning|info|debug>
    Loglevel (default: ``info``)

--video-codec <avc|h264|hevc|h265|av1>
    Video codec to use for encoding (default: ``avc``)

--video-bitrate bitrate
    Encoding bitrate.

--pix_fmt <yuv420p|yuv444p>
    Use ``yuv420p`` or ``yuv444p`` output format for hevc stream
    (default: ``yuv420p``)

--server-peer-id id
    Server (session) ID (default: ``ga``)

--client-peer-id id
    Client ID (default: ``client``)

--display display
    Display output to capture. If this option is not specified tool captures
    output of the main display (the first display connected to the first
    adapter in DXGI order).

Config file can be used to set other options. For example, use
``signaling-server-host`` and ``signaling-server-host`` to specify P2P server
location (``localhost`` and ``8095`` by default).

`enum-adapters <../../../apps/enum-adapters/readme.rst>`_ tool can be used to
check available displays.

Examples
--------

* Capture main display output as AVC stream::

    ./bin/screen-capture.exe ./config/server.desktop.webRTC.conf

* Capture main display output as HEVC 444 8-bit stream (444 8-bit is currently supported only for HEVC)::

    ./bin/screen-capture.exe --pix_fmt "yuv444p" ./config/server.desktop.webRTC.hevc.conf

* Capture ``\\.\DISPLAY1`` output as AVC stream::

    ./bin/screen-capture.exe --display "\\.\DISPLAY1" ./config/server.desktop.webRTC.conf

* Capture ``\\.\DISPLAY1`` and ``\\.\DISPLAY2`` outputs as AVC streams. Use two
  separate command prompts to run below commands in parallel. Mind that you
  must specify different server credentials for the 2nd stream. Use 2 separate
  clients to connect to each stream with appropriate credentials::

    ./bin/screen-capture.exe --display "\\.\DISPLAY1" ^
      ./config/server.desktop.webRTC.conf

    ./bin/screen-capture.exe --display "\\.\DISPLAY2" ^
      --server-peer-id "ga1" --client-peer-id "client1" ^
      ./config/server.desktop.webRTC.conf

Build prerequisites
-------------------

Screen Capture Streaming tool has the following build prerequisites:

* `FFmpeg`_ with enabled QSV encoders
* `OWT`_ built with ``--cg_server`` flag

You can refer to the dockerfiles provided with this repository to see how these
dependencies should be built - see `dockers <../../../../docker>`_ readme for
details.
