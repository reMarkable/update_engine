// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/update_engine/dbus_service.h"

#include <string>

#include <glog/logging.h>

#include "src/update_engine/marshal.glibmarshal.h"
#include "src/update_engine/omaha_request_params.h"
#include "src/update_engine/utils.h"

using std::string;
