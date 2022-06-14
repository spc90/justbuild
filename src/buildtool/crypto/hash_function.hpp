#ifndef INCLUDED_SRC_BUILDTOOL_CRYPTO_HASH_FUNCTION_HPP
#define INCLUDED_SRC_BUILDTOOL_CRYPTO_HASH_FUNCTION_HPP

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

#include "src/buildtool/crypto/hash_generator.hpp"

/// \brief Hash function used for the entire buildtool.
class HashFunction {
  public:
    enum class JustHash : std::uint8_t {
        Native,     ///< SHA1 for plain hashes, and Git for blobs and trees.
        Compatible  ///< SHA256 for all hashes.
    };

    /// \brief Set globally used hash type.
    static void SetHashType(JustHash type) {
        [[maybe_unused]] auto _ = HashType(type);
    }

    /// \brief Compute a plain hash.
    [[nodiscard]] static auto ComputeHash(std::string const& data) noexcept
        -> HashGenerator::HashDigest {
        return ComputeTaggedHash(data);
    }

    /// \brief Compute a blob hash.
    [[nodiscard]] static auto ComputeBlobHash(std::string const& data) noexcept
        -> HashGenerator::HashDigest {
        static auto const kBlobTagCreator =
            [](std::string const& data) -> std::string {
            return {"blob " + std::to_string(data.size()) + '\0'};
        };
        return ComputeTaggedHash(data, kBlobTagCreator);
    }

    /// \brief Compute a tree hash.
    [[nodiscard]] static auto ComputeTreeHash(std::string const& data) noexcept
        -> HashGenerator::HashDigest {
        static auto const kTreeTagCreator =
            [](std::string const& data) -> std::string {
            return {"tree " + std::to_string(data.size()) + '\0'};
        };
        return ComputeTaggedHash(data, kTreeTagCreator);
    }

    /// \brief Obtain incremental hasher for computing plain hashes.
    [[nodiscard]] static auto Hasher() noexcept -> HashGenerator::Hasher {
        switch (HashType()) {
            case JustHash::Native:
                return HashGenerator{HashGenerator::HashType::SHA1}
                    .IncrementalHasher();
            case JustHash::Compatible:
                return HashGenerator{HashGenerator::HashType::SHA256}
                    .IncrementalHasher();
        }
    }

  private:
    static constexpr auto kDefaultType = JustHash::Native;

    [[nodiscard]] static auto HashType(
        std::optional<JustHash> type = std::nullopt) -> JustHash {
        static JustHash type_{kDefaultType};
        if (type) {
            type_ = *type;
        }
        return type_;
    }

    [[nodiscard]] static auto ComputeTaggedHash(
        std::string const& data,
        std::function<std::string(std::string const&)> const& tag_creator =
            {}) noexcept -> HashGenerator::HashDigest {
        auto hasher = Hasher();
        if (tag_creator and HashType() == JustHash::Native) {
            hasher.Update(tag_creator(data));
        }
        hasher.Update(data);
        return *std::move(hasher).Finalize();
    }
};

#endif  // INCLUDED_SRC_BUILDTOOL_CRYPTO_HASH_FUNCTION_HPP
