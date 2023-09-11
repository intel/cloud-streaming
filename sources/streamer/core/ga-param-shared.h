// Copyright (C) 2022 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions
// and limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

/**
* @file   ga-param-shared.h
*
* @brief  Declaration of parameter memory shared classes
*
*/

#ifndef __GA_PARAM_SHARED_H__
#define __GA_PARAM_SHARED_H__

#include "ga-common.h"
#include "ga-param-shared-structure.h"
#include <iostream>
#include <string>

/**
 * @brief ga_param_shared class
 *
 * ga_param_shared class is responsible for sharing parameters between
 * applications and modules using file mapping object shared memory.
 *
 */
class EXPORT ga_param_shared
{
public:
  /**
   * @brief param_shared_s data structure which defines parameters to share.
   */
  typedef struct param_shared_s param_shared_s;

  static const char* event_name_hook_ready;

public:
  ga_param_shared(uint64_t pid, uint32_t desired_access);
  ~ga_param_shared();

  ga_param_shared() = delete;
  void* operator new(std::size_t) = delete;

  bool is_valid() const;

  bool set_param_shared(const param_shared_s& params);

  bool set_config_pathname(const std::string config_pathname);
  std::string get_config_pathname() const;
  bool set_ga_root_path(const std::string root_path);
  std::string get_ga_root_path() const;
  bool set_game_dir(const std::string game_dir);
  std::string get_game_dir() const;
  bool set_game_exe(const std::string game_exe);
  std::string get_game_exe() const;
  bool set_game_argv(const std::string game_argv);
  std::string get_game_argv() const;
  bool set_hook_type(const std::string hook_type);
  std::string get_hook_type() const;
  bool set_codec_format(const std::string codec_format);
  std::string get_codec_format() const;
  bool set_server_peer_id(const std::string server_id);
  std::string get_server_peer_id() const;
  bool set_client_peer_id(const std::string client_id);
  std::string get_client_peer_id() const;
  bool set_logfile(const std::string logfile);
  std::string get_logfile() const;
  bool set_loglevel(const Severity level);
  Severity get_loglevel() const;
  bool set_luid(const LUID luid);
  LUID get_luid() const;
  bool set_tcae(const bool enable);
  bool get_tcae() const;
  bool set_ltr(const bool enable);
  bool get_ltr() const;
  bool set_ltrinterval(const std::string ltr_interval);
  std::string get_ltrinterval() const;
  bool set_present(const bool enable);
  bool get_present() const;
  bool set_width(const int width);
  int get_width() const;
  bool set_height(const int height);
  int get_height() const;
  bool set_video_bitrate(const std::string video_bitrate);
  bool set_encode_width(const int encode_width);
  int get_encode_width() const;
  bool set_encode_height(const int encode_height);
  int get_encode_height() const;
  std::string get_video_bitrate() const;

  static std::string get_event_name_with_pid(const char* event_name, int32_t pid);

protected:
  bool create_param_shared_mem(const std::string named, uint32_t desired_access);

private:
  HANDLE map_file_handle_ = nullptr;        /**< @brief Mapped file handle */
  param_shared_s* shared_param_ = nullptr;  /**< @brief shared parameters */
};

#endif  // __GA_PARAM_SHARED_H__
