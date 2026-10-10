#pragma once
#include "fluid_types.h"
#include <cstdint>
#include <exception>
#include <vector>

class FluidSimulation {
public:
    static constexpr std::size_t capacity = 8000;
    static constexpr float radius = 2.8f;
    static constexpr std::size_t parallel_threshold = 1024;
    FluidSimulation(float width, float height, unsigned workers = 0);
    // Zero selects the machine's performance-core count; one selects serial physics.
    void set_workers(unsigned workers);
    unsigned workers() const { return workers_; }
    void step(float dt, const std::vector<WindowRect>& windows, bool pouring, FluidPoint nozzle,
              FluidPoint gravity = {0, 950});
    void clear();
    FluidPoint place_hose(FluidPoint pointer, const std::vector<WindowRect>& windows) const;
    const std::vector<FluidVertex>& vertices() const { return render_vertices_; }
    std::size_t size() const { return particles_.size(); }

private:
    struct Particle { FluidPoint position; FluidPoint previous; FluidPoint velocity; };
    struct Neighbor { int index; FluidPoint delta; float distance; float q; };
    struct alignas(128) WorkerScratch {
        std::vector<Neighbor> neighbors;
        std::exception_ptr failure;
    };
    struct Density { float pressure; float near_pressure; };
    struct PairShift { int index; FluidPoint shift; };
    float width_;
    float height_;
    float emission_ = 0;
    std::uint32_t random_state_ = 0x4127a35u;
    std::vector<Particle> particles_;
    std::vector<FluidVertex> render_vertices_;
    std::vector<int> grid_heads_;
    std::vector<int> grid_next_;
    std::vector<FluidPoint> correction_;
    std::vector<WorkerScratch> worker_scratch_;
    std::vector<std::vector<PairShift>> pair_shifts_;
    unsigned workers_ = 1;
    int grid_width_;
    int grid_height_;
    float random();
    void rebuild_grid();
    void collide(Particle& particle, const std::vector<WindowRect>& windows);
    Density collect_neighbors(std::size_t i, std::vector<Neighbor>& cache) const;
    static FluidPoint neighbor_shift(const Neighbor& neighbor, Density density, float dt);
    void apply_shift(std::size_t i, int j, FluidPoint shift);
    void relax_density(float dt);
};

bool run_physics_checks();
bool run_parallel_physics_checks();
