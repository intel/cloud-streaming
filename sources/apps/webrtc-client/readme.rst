WebRTC (OWT) Client
===================

.. contents::

.. _FFmpeg: https://www.ffmpeg.org/
.. _OpenSSL: https://www.openssl.org/
.. _OWT: https://github.com/open-webrtc-toolkit/owt-client-native

WebRTC (OWT) client is Windows client application for Cloud Streaming reference
stack. It can be used for both Windows and Android stacks. Supported features:

* AVC, HEVC and AV1 video decoding
* Audio decoding
* Telemetry logging and (optional) rendering

Features specific to Android reference stack (such as camera, microphone, GPS,
etc.) are currently not supported.

Current client implementation only supports full screen mode. To stop the
client, open Windows Task Manager, search for the application and click
"End task".

Client unconditionally dumps telemetry data to ``C:\Temp\ClientStatsLog_<PID>.txt``
file. To increase amount of telemetry data add ``--verbose`` flag to command
line. With ``--show_statistics`` telemetry data will be rendered in a
separate window.

Client usage
------------

::

  webrtc-client.exe [<options>]

Options:

-h, --help
    Print help

-v, --verbose
    Turn on additional logging (default: off)

--peer_server_url url
    P2P server url, http://server.com:8095 or http://server.com:8096
    (default: "")

--sessionid id
    Session ID (default: ``ga``)

--clientid id
    Client ID (default: ``client``)

--show_statistics
    Show telemetry window (default: off). This flag implies ``--verbose``.

--logging
    Output log messages to terminal

--streamdump
    Dump incoming video bitstream to current directory. Output file name
    pattern: ``webrtc_receive_stream_*.ivf``.

Build prerequisites
-------------------

WebRTC (OWT) client has the following build prerequisites:

* `FFmpeg`_ with enabled dx11va decoders
* `OpenSSL`_
* `OWT`_ built with ``--cg_client`` flag

You can refer to the dockerfiles provided with this repository to see how these
dependencies should be built - see `dockers <../../../docker>`_ readme for details.
