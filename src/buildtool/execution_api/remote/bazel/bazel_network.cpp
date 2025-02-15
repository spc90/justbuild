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

#include "src/buildtool/execution_api/remote/bazel/bazel_network.hpp"

#include "src/buildtool/execution_api/remote/bazel/bazel_client_common.hpp"
#include "src/buildtool/execution_api/remote/bazel/bazel_response.hpp"
#include "src/buildtool/logging/logger.hpp"

namespace {

[[nodiscard]] auto ReadDirectory(
    gsl::not_null<BazelNetwork const*> const& network,
    bazel_re::Digest const& digest) noexcept
    -> std::optional<bazel_re::Directory> {
    auto blobs = network->ReadBlobs({digest}).Next();
    if (blobs.size() == 1) {
        return BazelMsgFactory::MessageFromString<bazel_re::Directory>(
            blobs.at(0).data);
    }
    Logger::Log(LogLevel::Error,
                "Directory {} not found in CAS",
                NativeSupport::Unprefix(digest.hash()));
    return std::nullopt;
}

[[nodiscard]] auto ReadGitTree(
    gsl::not_null<BazelNetwork const*> const& network,
    bazel_re::Digest const& digest) noexcept
    -> std::optional<GitRepo::tree_entries_t> {
    auto blobs = network->ReadBlobs({digest}).Next();
    if (blobs.size() == 1) {
        auto const& content = blobs.at(0).data;
        return GitRepo::ReadTreeData(
            content,
            HashFunction::ComputeTreeHash(content).Bytes(),
            /*is_hex_id=*/false);
    }
    Logger::Log(LogLevel::Error,
                "Tree {} not found in CAS",
                NativeSupport::Unprefix(digest.hash()));
    return std::nullopt;
}

[[nodiscard]] auto TreeToStream(
    gsl::not_null<BazelNetwork const*> const& network,
    bazel_re::Digest const& tree_digest,
    gsl::not_null<FILE*> const& stream) noexcept -> bool {
    if (Compatibility::IsCompatible()) {
        if (auto dir = ReadDirectory(network, tree_digest)) {
            if (auto data = BazelMsgFactory::DirectoryToString(*dir)) {
                auto const& str = *data;
                std::fwrite(str.data(), 1, str.size(), stream);
                return true;
            }
        }
    }
    else {
        if (auto entries = ReadGitTree(network, tree_digest)) {
            if (auto data = BazelMsgFactory::GitTreeToString(*entries)) {
                auto const& str = *data;
                std::fwrite(str.data(), 1, str.size(), stream);
                return true;
            }
        }
    }
    return false;
}

[[nodiscard]] auto BlobToStream(
    gsl::not_null<BazelNetwork const*> const& network,
    bazel_re::Digest const& blob_digest,
    gsl::not_null<FILE*> const& stream) noexcept -> bool {
    auto reader = network->IncrementalReadSingleBlob(blob_digest);
    auto data = reader.Next();
    while (data and not data->empty()) {
        auto const& str = *data;
        std::fwrite(str.data(), 1, str.size(), stream);
        data = reader.Next();
    }
    return data.has_value();
}

}  // namespace

BazelNetwork::BazelNetwork(std::string instance_name,
                           std::string const& host,
                           Port port,
                           ExecutionConfiguration const& exec_config) noexcept
    : instance_name_{std::move(instance_name)},
      exec_config_{exec_config},
      cas_{std::make_unique<BazelCasClient>(host, port)},
      ac_{std::make_unique<BazelAcClient>(host, port)},
      exec_{std::make_unique<BazelExecutionClient>(host, port)} {}

auto BazelNetwork::IsAvailable(bazel_re::Digest const& digest) const noexcept
    -> bool {
    return cas_
        ->FindMissingBlobs(instance_name_,
                           std::vector<bazel_re::Digest>{digest})
        .empty();
}

auto BazelNetwork::IsAvailable(std::vector<bazel_re::Digest> const& digests)
    const noexcept -> std::vector<bazel_re::Digest> {
    return cas_->FindMissingBlobs(instance_name_, digests);
}

template <class T_Iter>
auto BazelNetwork::DoUploadBlobs(T_Iter const& first,
                                 T_Iter const& last) noexcept -> bool {
    try {
        auto num_blobs = gsl::narrow<std::size_t>(std::distance(first, last));

        std::vector<bazel_re::Digest> digests{};
        digests.reserve(num_blobs);

        auto begin = first;
        auto current = first;
        std::size_t transfer_size{};
        while (current != last) {
            auto const& blob = *current;
            transfer_size += blob.data.size();
            if (transfer_size > kMaxBatchTransferSize) {
                if (begin == current) {
                    if (cas_->UpdateSingleBlob(instance_name_, blob)) {
                        digests.emplace_back(blob.digest);
                    }
                    ++current;
                }
                else {
                    for (auto& digest : cas_->BatchUpdateBlobs(
                             instance_name_, begin, current)) {
                        digests.emplace_back(std::move(digest));
                    }
                }
                begin = current;
                transfer_size = 0;
            }
            else {
                ++current;
            }
        }
        if (begin != current) {
            for (auto& digest :
                 cas_->BatchUpdateBlobs(instance_name_, begin, current)) {
                digests.emplace_back(std::move(digest));
            }
        }

        if (digests.size() == num_blobs) {
            return true;
        }
    } catch (...) {
    }

    Logger::Log(LogLevel::Warning, "Failed to update all blobs");
    return false;
}

auto BazelNetwork::UploadBlobs(BlobContainer const& blobs,
                               bool skip_find_missing) noexcept -> bool {
    if (skip_find_missing) {
        return DoUploadBlobs(blobs.begin(), blobs.end());
    }

    // find digests of blobs missing in CAS
    auto missing_digests =
        cas_->FindMissingBlobs(instance_name_, blobs.Digests());

    if (not missing_digests.empty()) {
        // update missing blobs
        auto missing_blobs = blobs.RelatedBlobs(missing_digests);
        return DoUploadBlobs(missing_blobs.begin(), missing_blobs.end());
    }
    return true;
}

auto BazelNetwork::ExecuteBazelActionSync(
    bazel_re::Digest const& action) noexcept
    -> std::optional<BazelExecutionClient::ExecutionOutput> {
    auto response =
        exec_->Execute(instance_name_, action, exec_config_, true /*wait*/);

    if (response.state !=
            BazelExecutionClient::ExecutionResponse::State::Finished or
        not response.output) {
        Logger::Log(LogLevel::Error,
                    "Failed to execute action with execution id {}.",
                    action.hash());
        return std::nullopt;
    }

    return response.output;
}

auto BazelNetwork::BlobReader::Next() noexcept -> std::vector<BazelBlob> {
    std::size_t size{};
    std::vector<BazelBlob> blobs{};

    try {
        while (current_ != ids_.end()) {
            auto blob_size = gsl::narrow<std::size_t>(current_->size_bytes());
            size += blob_size;
            // read if size is 0 (unknown) or exceeds transfer size
            if (blob_size == 0 or size > kMaxBatchTransferSize) {
                // perform read of range [begin_, current_)
                if (begin_ == current_) {
                    auto blob = cas_->ReadSingleBlob(instance_name_, *begin_);
                    if (blob) {
                        blobs.emplace_back(std::move(*blob));
                    }
                    ++current_;
                }
                else {
                    blobs =
                        cas_->BatchReadBlobs(instance_name_, begin_, current_);
                }
                begin_ = current_;
                break;
            }
            ++current_;
        }

        if (begin_ != current_) {
            blobs = cas_->BatchReadBlobs(instance_name_, begin_, current_);
            begin_ = current_;
        }
    } catch (std::exception const& e) {
        Logger::Log(LogLevel::Error, "Reading blobs failed with: {}", e.what());
        Ensures(false);
    }

    return blobs;
}

auto BazelNetwork::ReadBlobs(std::vector<bazel_re::Digest> ids) const noexcept
    -> BlobReader {
    return BlobReader{instance_name_, cas_.get(), std::move(ids)};
}

auto BazelNetwork::IncrementalReadSingleBlob(bazel_re::Digest const& id)
    const noexcept -> ByteStreamClient::IncrementalReader {
    return cas_->IncrementalReadSingleBlob(instance_name_, id);
}

auto BazelNetwork::GetCachedActionResult(
    bazel_re::Digest const& action,
    std::vector<std::string> const& output_files) const noexcept
    -> std::optional<bazel_re::ActionResult> {
    return ac_->GetActionResult(
        instance_name_, action, false, false, output_files);
}

auto BazelNetwork::RecursivelyReadTreeLeafs(
    bazel_re::Digest const& tree_digest,
    std::filesystem::path const& parent,
    bool request_remote_tree) const noexcept
    -> std::optional<std::pair<std::vector<std::filesystem::path>,
                               std::vector<Artifact::ObjectInfo>>> {
    std::optional<DirectoryMap> dir_map{std::nullopt};
    if (Compatibility::IsCompatible() and request_remote_tree) {
        // Query full tree from remote CAS. Note that this is currently not
        // supported by Buildbarn revision c3c06bbe2a.
        auto dirs =
            cas_->GetTree(instance_name_, tree_digest, kMaxBatchTransferSize);

        // Convert to Directory map
        dir_map = DirectoryMap{};
        dir_map->reserve(dirs.size());
        for (auto& dir : dirs) {
            try {
                dir_map->emplace(ArtifactDigest::Create<ObjectType::File>(
                                     dir.SerializeAsString()),
                                 std::move(dir));
            } catch (...) {
                return std::nullopt;
            }
        }
    }

    std::vector<std::filesystem::path> paths{};
    std::vector<Artifact::ObjectInfo> infos{};

    auto store_info = [&paths, &infos](auto path, auto info) {
        paths.emplace_back(path);
        infos.emplace_back(info);
        return true;
    };

    if (ReadObjectInfosRecursively(dir_map, store_info, parent, tree_digest)) {
        return std::make_pair(std::move(paths), std::move(infos));
    }

    return std::nullopt;
}

auto BazelNetwork::ReadDirectTreeEntries(
    bazel_re::Digest const& tree_digest,
    std::filesystem::path const& parent) const noexcept
    -> std::optional<std::pair<std::vector<std::filesystem::path>,
                               std::vector<Artifact::ObjectInfo>>> {
    std::vector<std::filesystem::path> paths{};
    std::vector<Artifact::ObjectInfo> infos{};

    auto store_info = [&paths, &infos](auto path, auto info) {
        paths.emplace_back(path);
        infos.emplace_back(info);
        return true;
    };

    if (Compatibility::IsCompatible()) {
        // read from CAS
        if (auto dir = ReadDirectory(this, tree_digest)) {
            if (not BazelMsgFactory::ReadObjectInfosFromDirectory(
                    *dir, [&store_info, &parent](auto path, auto info) {
                        return store_info(parent / path, info);
                    })) {
                return std::nullopt;
            }
        }
    }
    else {
        if (auto entries = ReadGitTree(this, tree_digest)) {
            if (not BazelMsgFactory::ReadObjectInfosFromGitTree(
                    *entries, [&store_info, &parent](auto path, auto info) {
                        return store_info(parent / path, info);
                    })) {
                return std::nullopt;
            }
        }
    }

    return std::make_pair(std::move(paths), std::move(infos));
}

// NOLINTNEXTLINE(misc-no-recursion)
auto BazelNetwork::ReadObjectInfosRecursively(
    std::optional<DirectoryMap> const& dir_map,
    BazelMsgFactory::InfoStoreFunc const& store_info,
    std::filesystem::path const& parent,
    bazel_re::Digest const& digest) const noexcept -> bool {
    if (Compatibility::IsCompatible()) {
        // read from in-memory Directory map
        if (dir_map) {
            if (dir_map->contains(digest)) {
                return BazelMsgFactory::ReadObjectInfosFromDirectory(
                    dir_map->at(digest),
                    [this, &dir_map, &store_info, &parent](auto path,
                                                           auto info) {
                        return IsTreeObject(info.type)
                                   ? ReadObjectInfosRecursively(dir_map,
                                                                store_info,
                                                                parent / path,
                                                                info.digest)
                                   : store_info(parent / path, info);
                    });
            }
        }

        // fallback read from CAS
        if (auto dir = ReadDirectory(this, digest)) {
            return BazelMsgFactory::ReadObjectInfosFromDirectory(
                *dir,
                [this, &dir_map, &store_info, &parent](auto path, auto info) {
                    return IsTreeObject(info.type)
                               ? ReadObjectInfosRecursively(dir_map,
                                                            store_info,
                                                            parent / path,
                                                            info.digest)
                               : store_info(parent / path, info);
                });
        }
    }
    else {
        if (auto entries = ReadGitTree(this, digest)) {
            return BazelMsgFactory::ReadObjectInfosFromGitTree(
                *entries,
                [this, &dir_map, &store_info, &parent](auto path, auto info) {
                    return IsTreeObject(info.type)
                               ? ReadObjectInfosRecursively(dir_map,
                                                            store_info,
                                                            parent / path,
                                                            info.digest)
                               : store_info(parent / path, info);
                });
        }
    }
    return false;
}

auto BazelNetwork::DumpToStream(
    Artifact::ObjectInfo const& info,
    gsl::not_null<FILE*> const& stream) const noexcept -> bool {
    return IsTreeObject(info.type) ? TreeToStream(this, info.digest, stream)
                                   : BlobToStream(this, info.digest, stream);
}
