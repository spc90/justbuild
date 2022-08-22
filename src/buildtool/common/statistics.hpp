#ifndef INCLUDED_SRC_BUILDTOOL_COMMON_STATISTICS_HPP
#define INCLUDED_SRC_BUILDTOOL_COMMON_STATISTICS_HPP

#include <atomic>

class Statistics {
  public:
    [[nodiscard]] static auto Instance() noexcept -> Statistics& {
        static Statistics instance{};
        return instance;
    }

    void Reset() noexcept {
        num_actions_queued_ = 0;
        num_actions_executed_ = 0;
        num_actions_cached_ = 0;
        num_actions_flaky_ = 0;
        num_actions_flaky_tainted_ = 0;
        num_rebuilt_actions_compared_ = 0;
        num_rebuilt_actions_missing_ = 0;
        num_trees_analysed_ = 0;
    }
    void IncrementActionsQueuedCounter() noexcept { ++num_actions_queued_; }
    void IncrementActionsExecutedCounter() noexcept { ++num_actions_executed_; }
    void IncrementActionsCachedCounter() noexcept { ++num_actions_cached_; }
    void IncrementActionsFlakyCounter() noexcept { ++num_actions_flaky_; }
    void IncrementActionsFlakyTaintedCounter() noexcept {
        ++num_actions_flaky_tainted_;
    }
    void IncrementRebuiltActionMissingCounter() noexcept {
        ++num_rebuilt_actions_missing_;
    }
    void IncrementRebuiltActionComparedCounter() noexcept {
        ++num_rebuilt_actions_compared_;
    }
    void IncrementExportsCachedCounter() noexcept { ++num_exports_cached_; }
    void IncrementExportsUncachedCounter() noexcept { ++num_exports_uncached_; }
    void IncrementExportsNotEligibleCounter() noexcept {
        ++num_exports_not_eligible_;
    }
    void IncrementTreesAnalysedCounter() noexcept { ++num_trees_analysed_; }
    [[nodiscard]] auto ActionsQueuedCounter() const noexcept -> int {
        return num_actions_queued_;
    }
    [[nodiscard]] auto ActionsExecutedCounter() const noexcept -> int {
        return num_actions_executed_;
    }
    [[nodiscard]] auto ActionsCachedCounter() const noexcept -> int {
        return num_actions_cached_;
    }
    [[nodiscard]] auto ActionsFlakyCounter() const noexcept -> int {
        return num_actions_flaky_;
    }
    [[nodiscard]] auto ActionsFlakyTaintedCounter() const noexcept -> int {
        return num_actions_flaky_tainted_;
    }
    [[nodiscard]] auto RebuiltActionMissingCounter() const noexcept -> int {
        return num_rebuilt_actions_missing_;
    }
    [[nodiscard]] auto RebuiltActionComparedCounter() const noexcept -> int {
        return num_rebuilt_actions_compared_;
    }
    [[nodiscard]] auto ExportsCachedCounter() const noexcept -> int {
        return num_exports_cached_;
    }
    [[nodiscard]] auto ExportsUncachedCounter() const noexcept -> int {
        return num_exports_uncached_;
    }
    [[nodiscard]] auto ExportsNotEligibleCounter() const noexcept -> int {
        return num_exports_not_eligible_;
    }
    [[nodiscard]] auto TreesAnalysedCounter() const noexcept -> int {
        return num_trees_analysed_;
    }

  private:
    std::atomic<int> num_actions_queued_{};
    std::atomic<int> num_actions_executed_{};
    std::atomic<int> num_actions_cached_{};
    std::atomic<int> num_actions_flaky_{};
    std::atomic<int> num_actions_flaky_tainted_{};
    std::atomic<int> num_rebuilt_actions_missing_{};
    std::atomic<int> num_rebuilt_actions_compared_{};
    std::atomic<int> num_exports_cached_{};
    std::atomic<int> num_exports_uncached_{};
    std::atomic<int> num_exports_not_eligible_{};
    std::atomic<int> num_trees_analysed_{};
};

#endif  // INCLUDED_SRC_BUILDTOOL_COMMON_STATISTICS_HPP
