#ifndef INCLUDED_SRC_BUILDTOOL_EXECUTION_API_REMOTE_BAZEL_BAZEL_API_HPP
#define INCLUDED_SRC_BUILDTOOL_EXECUTION_API_REMOTE_BAZEL_BAZEL_API_HPP

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "gsl-lite/gsl-lite.hpp"
#include "src/buildtool/common/artifact_digest.hpp"
#include "src/buildtool/execution_api/bazel_msg/bazel_common.hpp"
#include "src/buildtool/execution_api/common/execution_api.hpp"
#include "src/buildtool/execution_api/common/local_tree_map.hpp"

// forward declaration for actual implementations
class BazelNetwork;
struct ExecutionConfiguration;

/// \brief Bazel implementation of the abstract Execution API.
class BazelApi final : public IExecutionApi {
  public:
    BazelApi(std::string const& instance_name,
             std::string const& host,
             Port port,
             ExecutionConfiguration const& exec_config) noexcept;
    BazelApi(BazelApi const&) = delete;
    BazelApi(BazelApi&& other) noexcept;
    auto operator=(BazelApi const&) -> BazelApi& = delete;
    auto operator=(BazelApi&&) -> BazelApi& = delete;
    ~BazelApi() final;

    auto CreateAction(
        ArtifactDigest const& root_digest,
        std::vector<std::string> const& command,
        std::vector<std::string> const& output_files,
        std::vector<std::string> const& output_dirs,
        std::map<std::string, std::string> const& env_vars,
        std::map<std::string, std::string> const& properties) noexcept
        -> IExecutionAction::Ptr final;

    [[nodiscard]] auto RetrieveToPaths(
        std::vector<Artifact::ObjectInfo> const& artifacts_info,
        std::vector<std::filesystem::path> const& output_paths) noexcept
        -> bool final;

    [[nodiscard]] auto RetrieveToFds(
        std::vector<Artifact::ObjectInfo> const& artifacts_info,
        std::vector<int> const& fds) noexcept -> bool final;

    [[nodiscard]] auto Upload(BlobContainer const& blobs,
                              bool skip_find_missing) noexcept -> bool final;

    [[nodiscard]] auto UploadTree(
        std::vector<DependencyGraph::NamedArtifactNodePtr> const&
            artifacts) noexcept -> std::optional<ArtifactDigest> final;

    [[nodiscard]] auto IsAvailable(ArtifactDigest const& digest) const noexcept
        -> bool final;

    [[nodiscard]] auto IsAvailable(std::vector<ArtifactDigest> const& digests)
        const noexcept -> std::vector<ArtifactDigest> final;

  private:
    std::shared_ptr<BazelNetwork> network_;
    std::shared_ptr<LocalTreeMap> tree_map_;
};

#endif  // INCLUDED_SRC_BUILDTOOL_EXECUTION_API_REMOTE_BAZEL_BAZEL_API_HPP
