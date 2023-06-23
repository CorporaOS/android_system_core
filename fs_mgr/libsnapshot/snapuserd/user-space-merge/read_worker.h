// Copyright (C) 2023 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include "snapuserd_core.h"

namespace android {
namespace snapshot {

class ReadWorker : public Worker {
  public:
    ReadWorker(const std::string& cow_device, const std::string& backing_device,
               const std::string& control_device, const std::string& misc_name,
               const std::string& base_path_merge, std::shared_ptr<SnapshotHandler> snapuserd);

    bool Run();
    bool Init() override;

  private:
    // Functions interacting with dm-user
    bool ProcessIORequest();
    bool WriteDmUserPayload(size_t size);
    bool DmuserReadRequest();
    void RespondIOError();

    bool ProcessCowOp(const CowOperation* cow_op);
    bool ProcessXorOp(const CowOperation* cow_op);
    bool ProcessOrderedOp(const CowOperation* cow_op);
    bool ProcessCopyOp(const CowOperation* cow_op);

    bool ReadAlignedSector(sector_t sector, size_t sz);
    bool ReadUnalignedSector(sector_t sector, size_t size);
    int ReadUnalignedSector(sector_t sector, size_t size,
                            std::vector<std::pair<sector_t, const CowOperation*>>::iterator& it);

    XorSink xorsink_;
    bool header_response_ = false;
};

}  // namespace snapshot
}  // namespace android
