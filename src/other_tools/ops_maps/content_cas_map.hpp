// Copyright 2022 Huawei Cloud Computing Technology Co., Ltd.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef INCLUDED_SRC_OTHER_TOOLS_OPS_MAPS_CONTENT_CAS_MAP_HPP
#define INCLUDED_SRC_OTHER_TOOLS_OPS_MAPS_CONTENT_CAS_MAP_HPP

#include <string>

#include "nlohmann/json.hpp"
#include "src/buildtool/multithreading/async_map_consumer.hpp"
#include "src/other_tools/just_mr/utils.hpp"
#include "src/utils/cpp/hash_combine.hpp"

struct ArchiveContent {
    std::string content; /* key */
    std::optional<std::string> distfile;
    std::string fetch_url;
    std::optional<std::string> sha256;
    std::optional<std::string> sha512;
    // name of repository for which work is done; used in progress reporting
    std::string origin;
    // flag deciding whether progress reporting is needed for key
    bool origin_from_distdir;

    [[nodiscard]] auto operator==(const ArchiveContent& other) const -> bool {
        return content == other.content;
    }
};

// Used in callers of ContentCASMap which need extra fields
struct ArchiveRepoInfo {
    ArchiveContent archive;
    std::string repo_type;
    std::string subdir;

    [[nodiscard]] auto operator==(const ArchiveRepoInfo& other) const -> bool {
        return archive == other.archive && subdir == other.subdir &&
               repo_type == other.repo_type;
    }
};

/// \brief Maps the content hash of an archive to an "exists" status flag.
using ContentCASMap = AsyncMapConsumer<ArchiveContent, bool>;

[[nodiscard]] auto CreateContentCASMap(JustMR::PathsPtr const& just_mr_paths,
                                       JustMR::CAInfoPtr const& ca_info,
                                       std::size_t jobs) -> ContentCASMap;

namespace std {
template <>
struct hash<ArchiveContent> {
    [[nodiscard]] auto operator()(const ArchiveContent& ct) const noexcept
        -> std::size_t {
        return std::hash<std::string>{}(ct.content);
    }
};

// Used in callers of ContentCASMap which need extra fields
template <>
struct hash<ArchiveRepoInfo> {
    [[nodiscard]] auto operator()(const ArchiveRepoInfo& ct) const noexcept
        -> std::size_t {
        size_t seed{};
        hash_combine<ArchiveContent>(&seed, ct.archive);
        hash_combine<std::string>(&seed, ct.subdir);
        hash_combine<std::string>(&seed, ct.repo_type);
        return seed;
    }
};
}  // namespace std

#endif  // INCLUDED_SRC_OTHER_TOOLS_OPS_MAPS_CONTENT_CAS_MAP_HPP
