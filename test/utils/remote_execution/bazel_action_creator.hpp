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

#ifndef INCLUDED_SRC_TEST_UTILS_REMOTE_EXECUTION_ACTION_CREATOR_HPP
#define INCLUDED_SRC_TEST_UTILS_REMOTE_EXECUTION_ACTION_CREATOR_HPP

#include <memory>
#include <string>
#include <vector>

#include "gsl/gsl"
#include "src/buildtool/crypto/hash_function.hpp"
#include "src/buildtool/execution_api/remote/bazel/bazel_cas_client.hpp"
#include "src/buildtool/execution_api/remote/config.hpp"

[[nodiscard]] static inline auto CreateAction(
    std::string const& instance_name,
    std::vector<std::string> const& args,
    std::map<std::string, std::string> const& env_vars,
    std::map<std::string, std::string> const& properties) noexcept
    -> std::unique_ptr<bazel_re::Digest> {
    auto const& info = RemoteExecutionConfig::RemoteAddress();

    auto platform = std::make_unique<bazel_re::Platform>();
    for (auto const& [name, value] : properties) {
        bazel_re::Platform_Property property;
        property.set_name(name);
        property.set_value(value);
        *(platform->add_properties()) = property;
    }

    std::vector<BazelBlob> blobs;

    bazel_re::Command cmd;
    cmd.set_allocated_platform(platform.release());
    std::copy(
        args.begin(), args.end(), pb::back_inserter(cmd.mutable_arguments()));

    std::transform(std::begin(env_vars),
                   std::end(env_vars),
                   pb::back_inserter(cmd.mutable_environment_variables()),
                   [](auto const& name_value) {
                       bazel_re::Command_EnvironmentVariable env_var_message;
                       env_var_message.set_name(name_value.first);
                       env_var_message.set_value(name_value.second);
                       return env_var_message;
                   });

    auto cmd_data = cmd.SerializeAsString();
    auto cmd_id = ArtifactDigest::Create<ObjectType::File>(cmd_data);
    blobs.emplace_back(cmd_id, cmd_data, /*is_exec=*/false);

    bazel_re::Directory empty_dir;
    auto dir_data = empty_dir.SerializeAsString();
    auto dir_id = ArtifactDigest::Create<ObjectType::Tree>(dir_data);
    blobs.emplace_back(dir_id, dir_data, /*is_exec=*/false);

    bazel_re::Action action;
    action.set_allocated_command_digest(
        gsl::owner<bazel_re::Digest*>{new bazel_re::Digest{cmd_id}});
    action.set_do_not_cache(false);
    action.set_allocated_input_root_digest(
        gsl::owner<bazel_re::Digest*>{new bazel_re::Digest{dir_id}});

    auto action_data = action.SerializeAsString();
    auto action_id = ArtifactDigest::Create<ObjectType::File>(action_data);
    blobs.emplace_back(action_id, action_data, /*is_exec=*/false);

    BazelCasClient cas_client(info->host, info->port);

    if (cas_client.BatchUpdateBlobs(instance_name, blobs.begin(), blobs.end())
            .size() == blobs.size()) {
        return std::make_unique<bazel_re::Digest>(action_id);
    }
    return nullptr;
}

#endif  // INCLUDED_SRC_TEST_UTILS_REMOTE_EXECUTION_ACTION_CREATOR_HPP
