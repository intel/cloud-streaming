Cloud Streaming
===============

.. contents::

Overview
--------

This repository provides coherent software, drivers and utilities
to setup streaming and video encoding services in the cloud to enable
end-users with remote access to Android and Windows cloud instances.

The following ingredients are included:

* For Windows:

  * **Indirect Display Driver (IDD)** to enable virtual display devices over
    headless display adapters

  * **Screen capture software** to capture, encode and stream Windows
    desktop output

* For Android (below software runs on Linux):

  * **Streaming software** - `OWT`_ based streaming service

  * **Video Encoding software** - `FFmpeg <https://ffmpeg.org/>`_ based
    video encoding service to assist streaming counterpart

  * Docker configurations for the easy setup

* **Helper and test scripts and docker configurations**

  * Helper `OWT`_ build docker containers for Windows and Linux
  * P2P service docker configuration
  * Chrome browser test client docker configuration (for Android)

Streaming and Video Encoding services for Android are parts of Intel Cloud
Streaming for Android OS reference stack and require setup for Android in
Container. For details see `Celadon documentation <https://www.intel.com/content/www/us/en/developer/topic-technology/open/celadon/overview.html>`_.

.. _OWT: https://github.com/open-webrtc-toolkit/owt-client-native.git

Prerequisites
-------------

.. |intel-flex| replace:: Intel® Data Center GPU Flex Series
.. _intel-flex: https://ark.intel.com/content/www/us/en/ark/products/series/230021/intel-data-center-gpu-flex-series.html

* For Android:

  * Video encoding service requires |intel-flex|_

  * Streaming and Video Encoding services are assumed to be running under same
    bare metal or virtual host

  * Android in Container is assumed to be properly installed and running under
    the same bare metal or virtual host as Streaming and Video Encoding services

* For Windows:

  * IDD driver requires Windows 10, version 1903 or newer

    * **Note:** considering this requirement, IDD driver can be used with Windows
      Server 2022, but not with Windows Server 2019

  * Screen capture and video encoding should work for any Intel GPU

    * **Note:** project was validated for |intel-flex|_

* Docker configurations require at least 100GB free disk space available for docker

Setup Instructions
------------------

* `Windows Setup Guide <./doc/windows_setup.rst>`_

* `Android Setup Guide <./doc/android_setup.rst>`_
