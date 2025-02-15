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

#ifndef INCLUDED_SRC_BUILDTOOL_EXECUTION_API_LOCAL_LOCAL_RESPONSE_HPP
#define INCLUDED_SRC_BUILDTOOL_EXECUTION_API_LOCAL_LOCAL_RESPONSE_HPP

#include "src/buildtool/execution_api/common/execution_response.hpp"
#include "src/buildtool/execution_api/local/local_action.hpp"
#include "src/buildtool/file_system/file_system_manager.hpp"
#include "src/buildtool/storage/storage.hpp"

/// \brief Response of a LocalAction.
class LocalResponse final : public IExecutionResponse {
    friend class LocalAction;

  public:
    auto Status() const noexcept -> StatusCode final {
        return StatusCode::Success;  // unused
    }
    auto HasStdErr() const noexcept -> bool final {
        return (output_.action.stderr_digest().size_bytes() != 0);
    }
    auto HasStdOut() const noexcept -> bool final {
        return (output_.action.stdout_digest().size_bytes() != 0);
    }
    auto StdErr() noexcept -> std::string final {
        if (auto path = storage_->CAS().BlobPath(output_.action.stderr_digest(),
                                                 /*is_executable=*/false)) {
            if (auto content = FileSystemManager::ReadFile(*path)) {
                return std::move(*content);
            }
        }
        Logger::Log(LogLevel::Debug, "reading stderr failed");
        return {};
    }
    auto StdOut() noexcept -> std::string final {
        if (auto path = storage_->CAS().BlobPath(output_.action.stdout_digest(),
                                                 /*is_executable=*/false)) {
            if (auto content = FileSystemManager::ReadFile(*path)) {
                return std::move(*content);
            }
        }
        Logger::Log(LogLevel::Debug, "reading stdout failed");
        return {};
    }
    auto ExitCode() const noexcept -> int final {
        return output_.action.exit_code();
    }
    auto IsCached() const noexcept -> bool final { return output_.is_cached; };

    auto ActionDigest() const noexcept -> std::string final {
        return action_id_;
    }

    auto Artifacts() const noexcept -> ArtifactInfos final {
        ArtifactInfos artifacts{};
        auto const& action_result = output_.action;
        artifacts.reserve(
            static_cast<std::size_t>(action_result.output_files().size()) +
            static_cast<std::size_t>(
                action_result.output_directories().size()));

        // collect files and store them
        for (auto const& file : action_result.output_files()) {
            try {
                artifacts.emplace(
                    file.path(),
                    Artifact::ObjectInfo{ArtifactDigest{file.digest()},
                                         file.is_executable()
                                             ? ObjectType::Executable
                                             : ObjectType::File});
            } catch (...) {
                return {};
            }
        }

        // collect directories and store them
        for (auto const& dir : action_result.output_directories()) {
            try {
                artifacts.emplace(
                    dir.path(),
                    Artifact::ObjectInfo{ArtifactDigest{dir.tree_digest()},
                                         ObjectType::Tree});
            } catch (...) {
                return {};
            }
        }

        return artifacts;
    };

  private:
    std::string action_id_{};
    LocalAction::Output output_{};
    gsl::not_null<Storage const*> storage_;

    explicit LocalResponse(
        std::string action_id,
        LocalAction::Output output,
        gsl::not_null<Storage const*> const& storage) noexcept
        : action_id_{std::move(action_id)},
          output_{std::move(output)},
          storage_{storage} {}
};

#endif  // INCLUDED_SRC_BUILDTOOL_EXECUTION_API_LOCAL_LOCAL_RESPONSE_HPP
