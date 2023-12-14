Screen Grab Tool
================

.. contents::

.. _libvpl: https://github.com/intel/libvpl
.. _FFmpeg: https://www.ffmpeg.org/

Screen Grab is a tool to capture and encode Windows dektop. Tool supports
encoding with AVC, HEVC and AV1. Encoded bitstream is saved in a file in a raw
video format.

Usage
-----

::

  screen-grab.exe [<options>] <output-file>

Options:

-h, --help
    Print help

--loglevel <error|warning|info|debug|none>
    If logging level is set application will write logs to
    ``screen-grab-log.txt`` file unless level is set to ``none``.
    (default: none)

--display <display>
    Display output to grab. By default - grab primary display.

-n <int>
    Capture specified number of frames, then exit. Use ``-1`` for infinte. Use
    CTRL+C to terminate capture (default: "-1")

--codec <avc|h264|hevc|h265|av1>
    Video codec to use for encoding (default: ``avc``)

--profile <profile>
    Video codec profile to use. Value depends on a codec (default: ``main`` for
    all codecs):

    * For AVC: ``baseline``, ``main``, ``high``
    * For HEVC: ``main``, ``main10``, ``mainsp``, ``rext``, ``scc``
    * For AV1: ``main``

--bitrate
    Encoding bitrate (default: ``3000000``).

--fps
    Encoding framerate (default: ``60``)

--gop
    GOP size (default: ``60``).

Build prerequisites
-------------------

Screen Grab Tool has the following build prerequisites:

* `libvpl`_
* `FFmpeg`_ with enabled QSV plugins (aka ffmpeg-qsv)

You can refer to the dockerfiles provided with this repository to see how these
dependencies should be built - see `dockers <../../../docker>`_ readme for details.
