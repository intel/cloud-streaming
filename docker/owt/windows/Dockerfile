ARG IMAGE=build:latest
FROM $IMAGE

WORKDIR C:/buildhome

RUN git clone https://github.com/open-webrtc-toolkit/owt-client-native.git src && cd src && \
  git checkout 4fb6299b

RUN ( \
  echo solutions = [ & \
  echo { & \
  echo    "managed": False, & \
  echo    "name": "src", & \
  echo    "url": "https://github.com/open-webrtc-toolkit/owt-client-native.git", & \
  echo    "custom_deps": {}, & \
  echo    "deps_file": "DEPS", & \
  echo    "safesync_url": "", & \
  echo    "custom_vars": {"checkout_instrumented_libraries": False}, & \
  echo }, & \
  echo ] & \
  echo target_os = [] \
) > .gclient && type .gclient

# If you are building under proxy you should edit below
# boto config and add proxies:
#   ARG $PROXY=some-proxy.com
#   ARG $PROXY_PORT=911
#   RUN ( \
#     echo [Boto] & \
#     echo proxy = $PROXY & \
#     echo proxy_port = $PROXY_PORT & \
#     echo https_validate_certificate = True \
#   ) > .boto && type .boto
RUN ( \
  echo [Boto] & \
  echo https_validate_certificate = True \
) > .boto && type .boto

RUN setx DEPOT_TOOLS_UPDATE 0 && \
  setx DEPOT_TOOLS_WIN_TOOLCHAIN 0 && \
  setx NO_AUTH_BOTO_CONFIG "C:\\buildhome\\.boto"

# Once we will build OWT below we will populate this directory with
# OWT headers and libraries we've built.
COPY build-deps C:/build-deps

# Performance of docker engine is sensitive to layer footprint, i.e. if we will use significant
# disk space, performance will suffer. That's the case for OWT - it checkouts ~9GB of data in
# the best case (--no-history). Thus, to make docker build run faster we pipe as much commands
# as possible and remove what we have cloned.
RUN gclient sync --no-history && \
   "%MSVS_2022%\\Common7\\Tools\\VsDevCmd.bat" && cd src && mkdir dist-x64 && mkdir dist-x86 && mkdir client-dist-x64 && \
   python3 .\\scripts\\build-win.py --gn_gen --sdk --cg_server --fake_audio --output_path dist-x64 --scheme release --arch x64 && \
   python3 .\\scripts\\build-win.py --gn_gen --sdk --cg_server --fake_audio --output_path dist-x86 --scheme release --arch x86 && \
   python3 .\\scripts\\build-win.py --gn_gen --sdk --cg_client --output_path client-dist-x64 --scheme release --arch x64 \
     --ffmpeg_root=C:\build-deps\windows-x86_64 --ssl_root=C:\build-deps\windows-x86_64 && \
   mkdir "C:\build-deps\windows-x86" && \
   mkdir "C:\build-deps\windows-x86\include" && \
   mkdir "C:\build-deps\windows-x86\lib" && \
   xcopy /s "talk\owt\sdk\include\cpp" "C:\build-deps\windows-x86\include" && \
   xcopy /s "talk\owt\sdk\include\cpp" "C:\build-deps\windows-x86_64\include" && \
   copy "dist-x86\libs\owt-release.lib" "C:\build-deps\windows-x86\lib\owt.lib" && \
   copy "dist-x64\libs\owt-release.lib" "C:\build-deps\windows-x86_64\lib\owt.lib" && \
   copy "client-dist-x64\libs\owt-release.lib" "C:\build-deps\windows-x86_64\lib\owt-client.lib" && \
   cd .. && rmdir /s /q src

ENTRYPOINT ["cmd"]
