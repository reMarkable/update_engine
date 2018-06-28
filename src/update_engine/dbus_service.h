// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PLATFORM_UPDATE_ENGINE_DBUS_SERVICE_H__
#define CHROMEOS_PLATFORM_UPDATE_ENGINE_DBUS_SERVICE_H__

#include <inttypes.h>
#include "src/update_engine/dbus_constants.h"

#include "src/update_engine/update_attempter.h"

struct Manager {
    struct AttemptUpdate {
        typedef Manager Interface;
        inline static const std::string name() {
            return "AttemptUpdate";
        }
    };
    struct ResetStatus {
        typedef Manager Interface;
        inline static const std::string name() {
            return "ResetStatus";
        }
    };
    struct GetStatus {
        typedef Manager Interface;
        inline static const std::string name() {
            return "GetStatus";
        }
    };

    struct Signals {
        struct UpdateStatus {
            typedef Manager Interface;
            typedef std::tuple<int64_t, double, std::string, std::string, int64_t> ArgumentType;
            inline static const std::string name() {
                return "UpdateStatus";
            }
        };
    };
};

namespace core { namespace dbus { namespace traits {
template<> struct Service<Manager> {
    inline static const std::string interface_name() {
        return chromeos_update_engine::kUpdateEngineServiceInterface;
    }
};
}}}

//gboolean update_engine_service_emit_status_update(
//    UpdateEngineService* self,
//    gint64 last_checked_time,
//    gdouble progress,
//    const gchar* current_operation,
//    const gchar* new_version,
//    gint64 new_size);
//
//G_END_DECLS

#endif  // CHROMEOS_PLATFORM_UPDATE_ENGINE_DBUS_SERVICE_H__
