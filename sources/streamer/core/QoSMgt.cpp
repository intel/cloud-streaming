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

#include "QoSMgt.h"
#include "encoder-common.h"
#include <memory>

// Queue the cursor data into the server
int queue_qos(QosInfo qosinfo)
{
    std::shared_ptr<QosInfo> sQosInfo = std::make_shared<QosInfo>(qosinfo);
    encoder_send_qos(sQosInfo);
    return 0;
}
