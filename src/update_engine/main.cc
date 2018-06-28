// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gflags/gflags.h>
#include <glib.h>
#include <glog/logging.h>
#include <core/dbus/dbus.h>
#include <core/dbus/asio/executor.h>
#include <core/dbus/service.h>

#include "src/update_engine/certificate_checker.h"
#include "src/update_engine/dbus_constants.h"
#include "src/update_engine/dbus_interface.h"
#include "src/update_engine/dbus_service.h"
#include "src/update_engine/real_system_state.h"
#include "src/update_engine/subprocess.h"
#include "src/update_engine/terminator.h"
#include "src/update_engine/update_attempter.h"
#include "src/update_engine/update_check_scheduler.h"
#include "src/update_engine/utils.h"

extern "C" {
#include "src/update_engine/update_engine.dbusserver.h"
}

namespace dbus = core::dbus;

DEFINE_bool(foreground, false,
            "Don't daemon()ize; run in foreground.");


namespace chromeos_update_engine {

gboolean UpdateBootFlags(void* arg) {
  reinterpret_cast<UpdateAttempter*>(arg)->UpdateBootFlags();
  return FALSE;  // Don't call this callback again
}

gboolean BroadcastStatus(void* arg) {
  reinterpret_cast<UpdateAttempter*>(arg)->BroadcastStatus();
  return FALSE;  // Don't call this callback again
}

}

int main(int argc, char** argv) {
  // Disable glog's default behavior of logging to files.
  FLAGS_logtostderr = true;
  GFLAGS_NAMESPACE::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);

  chromeos_update_engine::Terminator::Init();
  chromeos_update_engine::Subprocess::Init();

  if (!FLAGS_foreground)
    PLOG_IF(FATAL, daemon(0, 0) == 1) << "daemon() failed";

  LOG(INFO) << "reMarkable Update Engine starting";

  // Ensure that all written files have safe permissions.
  // This is a mask, so we _block_ execute for the owner, and ALL
  // permissions for other users.
  // Done _after_ log file creation.
  umask(S_IXUSR | S_IRWXG | S_IRWXO);

  // Create the single GMainLoop
  GMainLoop* loop = g_main_loop_new(g_main_context_default(), FALSE);

  chromeos_update_engine::RealSystemState real_system_state;
  LOG_IF(ERROR, !real_system_state.Initialize())
      << "Failed to initialize system state.";
  chromeos_update_engine::UpdateAttempter *update_attempter =
      real_system_state.update_attempter();
  CHECK(update_attempter);

  // Sets static members for the certificate checker.
  chromeos_update_engine::CertificateChecker::set_system_state(
      &real_system_state);
  chromeos_update_engine::OpenSSLWrapper openssl_wrapper;
  chromeos_update_engine::CertificateChecker::set_openssl_wrapper(
      &openssl_wrapper);


  dbus::Bus::Ptr bus = std::make_shared<dbus::Bus>(dbus::WellKnownBus::session);
  bus->install_executor(core::dbus::asio::make_executor(bus));
  std::thread t {std::bind(&dbus::Bus::run, bus)};

  dbus::Service::Ptr service = dbus::Service::add_service<Manager>(bus);
  CHECK(service);

  dbus::Object::Ptr object = service->add_object_for_path(dbus::types::ObjectPath(chromeos_update_engine::kUpdateEngineServicePath));
  CHECK(object);

  object->install_method_handler<Manager::AttemptUpdate>([=](const dbus::Message::Ptr&) {
      update_attempter->CheckForUpdate(true);
  });
  object->install_method_handler<Manager::ResetStatus>([=](const dbus::Message::Ptr&) {
      update_attempter->ResetStatus();
  });
  object->install_method_handler<Manager::GetStatus>([=](const dbus::Message::Ptr &m) {
      int64_t last_checked_time;
      double progress;
      std::string current_operation;
      std::string new_version;
      int64_t new_size;
      update_attempter->GetStatus(&last_checked_time, &progress, &current_operation, &new_version, &new_size);

      dbus::Message::Ptr reply = dbus::Message::make_method_return(m);
      reply->writer() << last_checked_time
                      << progress
                      << current_operation
                      << new_version
                      << new_size;
      bus->send(reply);
  });

  // Schedule periodic update checks.
  chromeos_update_engine::UpdateCheckScheduler scheduler(update_attempter,
                                                         &real_system_state);
  scheduler.Run();

  // Update boot flags after 45 seconds.
  g_timeout_add_seconds(45,
                        &chromeos_update_engine::UpdateBootFlags,
                        update_attempter);

  // Broadcast the update engine status on startup to ensure consistent system
  // state on crashes.
  g_idle_add(&chromeos_update_engine::BroadcastStatus, update_attempter);

  // Run the main loop until exit time:
  g_main_loop_run(loop);

  // Cleanup:
  g_main_loop_unref(loop);

  LOG(INFO) << "reMarkable Update Engine terminating";
  return 0;
}
