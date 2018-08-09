/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "utility.h"

#include <android-base/logging.h>
#include <fs_mgr_dm_linear.h>
#include <liblp/liblp.h>

#include "fastboot_device.h"

using namespace android::fs_mgr;
using android::base::unique_fd;
using android::hardware::boot::V1_0::Slot;

static bool OpenPhysicalPartition(const std::string& name, PartitionHandle* handle) {
    std::optional<std::string> path = FindPhysicalPartition(name);
    if (!path) {
        return false;
    }
    *handle = PartitionHandle(*path);
    return true;
}

static bool OpenLogicalPartition(const std::string& name, const std::string& slot,
                                 PartitionHandle* handle) {
    std::optional<std::string> path = FindPhysicalPartition(LP_METADATA_PARTITION_NAME);
    if (!path) {
        return false;
    }
    uint32_t slot_number = SlotNumberForSlotSuffix(slot);
    std::string dm_path;
    if (!CreateLogicalPartition(path->c_str(), slot_number, name, true, &dm_path)) {
        LOG(ERROR) << "Could not map partition: " << name;
        return false;
    }
    auto closer = [name]() -> void { DestroyLogicalPartition(name); };
    *handle = PartitionHandle(dm_path, std::move(closer));
    return true;
}

bool OpenPartition(FastbootDevice* device, const std::string& name, PartitionHandle* handle) {
    // We prioritize logical partitions over physical ones, and do this
    // consistently for other partition operations (like getvar:partition-size).
    if (LogicalPartitionExists(name, device->GetCurrentSlot())) {
        if (!OpenLogicalPartition(name, device->GetCurrentSlot(), handle)) {
            return false;
        }
    } else if (!OpenPhysicalPartition(name, handle)) {
        LOG(ERROR) << "No such partition: " << name;
        return false;
    }

    unique_fd fd(TEMP_FAILURE_RETRY(open(handle->path().c_str(), O_WRONLY | O_EXCL)));
    if (fd < 0) {
        PLOG(ERROR) << "Failed to open block device: " << handle->path();
        return false;
    }
    handle->set_fd(std::move(fd));
    return true;
}

std::optional<std::string> FindPhysicalPartition(const std::string& name) {
    std::string path = "/dev/block/by-name/" + name;
    if (access(path.c_str(), R_OK | W_OK) < 0) {
        return {};
    }
    return path;
}

static const LpMetadataPartition* FindLogicalPartition(const LpMetadata& metadata,
                                                       const std::string& name) {
    for (const auto& partition : metadata.partitions) {
        if (GetPartitionName(partition) == name) {
            return &partition;
        }
    }
    return nullptr;
}

bool LogicalPartitionExists(const std::string& name, const std::string& slot_suffix,
                            bool* is_zero_length) {
    auto path = FindPhysicalPartition(LP_METADATA_PARTITION_NAME);
    if (!path) {
        return false;
    }

    uint32_t slot_number = SlotNumberForSlotSuffix(slot_suffix);
    std::unique_ptr<LpMetadata> metadata = ReadMetadata(path->c_str(), slot_number);
    if (!metadata) {
        return false;
    }
    const LpMetadataPartition* partition = FindLogicalPartition(*metadata.get(), name);
    if (!partition) {
        return false;
    }
    if (is_zero_length) {
        *is_zero_length = (partition->num_extents == 0);
    }
    return true;
}

bool GetSlotNumber(const std::string& slot, Slot* number) {
    if (slot.size() != 1) {
        return false;
    }
    if (slot[0] < 'a' || slot[0] > 'z') {
        return false;
    }
    *number = slot[0] - 'a';
    return true;
}
