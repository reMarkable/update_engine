// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/omaha_request_params.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/utsname.h>

#include <map>
#include <string>
#include <vector>
#include <fstream>

#include "update_engine/simple_key_value_store.h"
#include "update_engine/system_state.h"
#include "update_engine/prefs_interface.h"
#include "update_engine/utils.h"

#define CALL_MEMBER_FN(object, member) ((object).*(member))

using std::map;
using std::string;
using std::vector;

namespace chromeos_update_engine {

const char *const OmahaRequestParams::kAppId(
    "{98DA7DF2-4E3E-4744-9DE6-EC931886ABAB}");
const char *const OmahaRequestParams::kOsVersion("codex");
const char *const OmahaRequestParams::kOsPlatform("reMarkable");
const char *const OmahaRequestParams::kDefaultChannel("Prod");
const char *const kProductionOmahaUrl(
    "https://get-updates.cloud.remarkable.engineering/service/update2");

bool OmahaRequestParams::Init(bool interactive)
{
    os_version_ = OmahaRequestParams::kOsVersion;
    if ( GetOemValue("ID", "") == "codex" ) {
        os_version_ += " " + GetOemValue("VERSION_ID", "");
    }

    os_platform_ = OmahaRequestParams::kOsPlatform;
    if (utils::GetMachineModel().find("reMarkable 2.0") != string::npos) {
        os_platform_ = "reMarkable2";
    }


    app_id_ = GetConfValue("REMARKABLE_RELEASE_APPID", OmahaRequestParams::kAppId);
    app_channel_ = GetConfValue("GROUP", kDefaultChannel);
    app_lang_ = "en-US";
    app_version_ = GetConfValue("REMARKABLE_RELEASE_VERSION", "");

    os_sp_ = app_version_ + "_" + GetMachineType();
    os_board_ = GetConfValue("REMARKABLE_RELEASE_BOARD", "");

    oemid_ = GetSerialnumber();
    if (oemid_.empty()) {
        LOG(ERROR) << "Unable to get serialnumber";
    }

    oemversion_ = GetOemValue("VERSION_ID", "");

    if (!system_state_->prefs()->GetString(kPrefsAlephVersion, &alephversion_)) {
        alephversion_.assign(app_version_);
        system_state_->prefs()->SetString(kPrefsAlephVersion, alephversion_);
    }

    bootid_ = utils::GetBootId();
    machineid_ = utils::GetMachineId();
    arch_ = GetMachineType();
    update_url_ = GetConfValue("SERVER", kProductionOmahaUrl);
    interactive_ = interactive;
    session_uuid_ = utils::GetUuid();

    LOG(INFO) << "Current group set to " << app_channel_;

    // deltas are only okay if the /.nodelta file does not exist.  if we don't
    // know (i.e. stat() returns some unexpected error), then err on the side of
    // caution and say deltas are not okay.
    // FIXME: re-enable this when we switch to read-only roots
//  delta_okay_ = (access((root_ + "/.nodelta").c_str(), F_OK) < 0) &&
//	  (errno == ENOENT);
    delta_okay_ = false;

    return true;
}

string OmahaRequestParams::SearchConfValue(const vector<string> &files,
        const string &key,
        const string &default_value) const
{
    for (vector<string>::const_iterator it = files.begin();
            it != files.end(); ++it) {
        string file_data;

        if (!utils::ReadFile(root_ + *it, &file_data)) {
            continue;
        }

        map<string, string> data = simple_key_value_store::ParseString(file_data);

        if (data.count(key)) {
            const string &value = data[key];
            return value;
        }
    }

    // not found
    return default_value;
}

string OmahaRequestParams::GetSerialnumber() const
{
    string name;

    if (os_platform_ == "reMarkable2") {
        name = "/dev/mmcblk2boot1";
    } else {
        name = "/dev/mmcblk1boot1";
    }

    std::ifstream file(name);

    if (!file.is_open()) {
        LOG(ERROR) << "Unable to open: " << name;
        return string();
    }

    // The device serial number is written using QDataStream.
    // Byte-order for field length is big-endian.
    uint32_t fieldLengthBigEndian;
    if (!file.read(reinterpret_cast<char*>(&fieldLengthBigEndian), sizeof fieldLengthBigEndian)) {
        LOG(ERROR) << "Error parsing serial number stream";
        return string();
    }

    uint32_t fieldLength = __bswap_32(fieldLengthBigEndian);

    // Set a simple max length for the serial number to validate a little
    if (fieldLength > 128) {
        LOG(ERROR) << "Too long field";
        return string();
    }
    std::vector<char> serialNumber(fieldLength);

    if (!file.read(serialNumber.data(), serialNumber.size())) {
        LOG(ERROR) << "Error parsing serial number stream";
        return string();
    }
    for (const char c : serialNumber) {
        // This is stuffed straight into the XML, so sanitize it
        if ((c < '0' && c != '-') ||
            (c > '9' && c < 'A') ||
            (c > 'Z' && c < 'a') ||
            (c > 'z' && c != '{' && c != '}')) {
            LOG(ERROR) << "Invalid character 0x" << std::hex << uint32_t(c) << " in serial";
            return string();
        }
        LOG(ERROR) << "Error parsing serial number stream";
    }

    file.close();
    return string(serialNumber.begin(), serialNumber.end());
}

string OmahaRequestParams::GetConfValue(const string &key,
                                        const string &default_value) const
{
    vector<string> files;
    files.push_back("/etc/remarkable.conf");
    files.push_back("/usr/share/remarkable/update.conf");
    files.push_back("/usr/share/remarkable/release");
    return SearchConfValue(files, key, default_value);
}

string OmahaRequestParams::GetOemValue(const string &key,
                                       const string &default_value) const
{
    vector<string> files;
    files.push_back("/etc/os-release");
    files.push_back("/usr/share/oem/os-release");
    string ret = SearchConfValue(files, key, default_value);

    if (ret.front() == '"' && ret.back() == '"') {
        ret = ret.substr(1, ret.length() - 2);
    }

    return ret;
}

string OmahaRequestParams::GetMachineType() const
{
    struct utsname buf;
    string ret;

    if (uname(&buf) == 0) {
        ret = buf.machine;
    }

    return ret;
}

void OmahaRequestParams::set_root(const std::string &root)
{
    root_ = root;
    Init(false);
}

}  // namespace chromeos_update_engine
