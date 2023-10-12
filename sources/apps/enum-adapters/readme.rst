Video Adapters Enumeration Tool
===============================

.. |EnumAdapters| replace:: IDXGIFactory1::EnumAdapters1
.. _EnumAdapters: https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgifactory1-enumadapters1

.. |D3DKMTEnumAdapters| replace:: D3DKMTEnumAdapters
.. _D3DKMTEnumAdapters: https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dkmthk/nf-d3dkmthk-d3dkmtenumadapters

``enum-adapters`` tool enumerates display adapters and displays using specified API.
Supported options:

--help
    Print help.

--api <dxgi|d3dkmt>
    Enumerate adapters using |EnumAdapters|_ DXGI API (``--api dxgi``) or
    |D3DKMTEnumAdapters|_ Windows Display Driver API (``--api d3dkmt``).
    Default: ``dxgi``.

--debug
    Additionally print debug messages.

--luid <high:lo>
    Print adapter index for the specified adapter luid. Luid can be specified
    as decimal or hex (use ``0x`` prefix for hex).

--show <basic|details|off>
    Display verbose information about adapters. Default: ``basic``.
