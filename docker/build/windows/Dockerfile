ARG IMAGE=mcr.microsoft.com/windows/servercore:ltsc2019
FROM $IMAGE

WORKDIR C:/buildtools/
COPY docker/build/windows/packages.config C:/buildtools/packages.config

RUN powershell.exe -Command "$env:chocolateyVersion='1.4.0'; Invoke-WebRequest https://chocolatey.org/install.ps1 -UseBasicParsing | Invoke-Expression"

RUN choco install -y --no-progress packages.config

RUN git config --global user.email "no.one@intel.com" && \
  git config --global user.name "No One"

ENV MSVS_2022="C:\\Program Files\\Microsoft Visual Studio\\2022\\Professional"
RUN curl -fL https://aka.ms/vs/17/release/vs_professional.exe -o vs_professional.exe && \
  ( vs_professional.exe --quiet --wait --norestart --nocache --locale en-US --noUpdateInstaller \
    --add Microsoft.VisualStudio.Workload.NativeDesktop \
    --add Microsoft.VisualStudio.Component.VC.ATL \
    --add Microsoft.VisualStudio.Component.VC.CoreIde \
    --add Microsoft.VisualStudio.Component.VC.CMake.Project \
    --add Microsoft.VisualStudio.Component.VC.Tools.x86.x64 \
    --add Microsoft.VisualStudio.Component.Windows10SDK.20348 \
  || IF "%ERRORLEVEL%"=="3010" EXIT 0 ) && \
  del vs_professional.exe && \
  type "%MSVS_2022%\Common7\IDE\devenv.isolation.ini"

# Downloading: Windows 10 SDK version 2104 (10.0.20348.0)
# See: https://developer.microsoft.com/en-us/windows/downloads/sdk-archive/
RUN curl -fL https://go.microsoft.com/fwlink/?linkid=2164145 -o winsdksetup.exe && \
  winsdksetup.exe /features OptionId.WindowsDesktopDebuggers /q /norestart && \
  del winsdksetup.exe

RUN setx path "C:\\buildtools\\depot_tools;%path%"

RUN git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git && cd depot_tools && \
  git checkout d85c5bc && \
  gclient && \
  git reset --hard d85c5bc

ENTRYPOINT ["cmd"]
