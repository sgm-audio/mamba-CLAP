#include "mamba_layer.h"
#include <cmath>

MambaLayer::MambaLayer(int64_t d_model, int64_t d_state, int64_t d_conv, int64_t expand)
    : d_model_(d_model), d_state_(d_state), d_conv_(d_conv), expand_(expand) {
    d_inner_ = expand * d_model;
    
    // Input projection: projects to three branches (for x, z, and the gate)
    in_proj = register_module("in_proj", torch::nn::Linear(d_model, d_inner_ * 2, false));
    // Output projection
    out_proj = register_module("out_proj", torch::nn::Linear(d_inner_, d_model, false));
    
    // Depthwise convolution
    conv1d = register_module("conv1d", torch::nn::Conv1d(
        torch::nn::Conv1dOptions(d_inner_, d_inner_, d_conv)
            .groups(d_inner_)
            .padding(d_conv - 1)
    ));
    
    // SSM parameters
    // A is initialized to be negative (stable)
    A_log = register_parameter("A_log", torch::randn({d_inner_, d_state}));
    B = register_parameter("B", torch::randn({d_inner_, d_state}));
    C = register_parameter("C", torch::randn({d_state_, d_inner_}));
    D = register_parameter("D", torch::randn({d_inner_}));
    
    // Initialize state
    reset_state(1, 1); // Will be properly sized in forward
}

void MambaLayer::reset_state(int64_t batch_size, int64_t seq_len) {
    // State shape: [batch, d_inner, d_state]
    state_ = torch::zeros({batch_size, d_inner_, d_state_}, torch::kFloat);
}

std::tuple<torch::Tensor, torch::Tensor, torch::Tensor> MambaLayer::proj_xyz(torch::Tensor x) {
    // x: [batch, seq, d_model]
    auto projected = in_proj->forward(x); // [batch, seq, 2 * d_inner]
    // Split into two branches: x and z (for the gate)
    auto xz = torch::split(projected, d_inner_, /*dim*/-1);
    auto x_branch = xz[0]; // [batch, seq, d_inner]
    auto z_branch = xz[1]; // [batch, seq, d_inner]
    
    // For simplicity, we'll use x_branch as the main branch and z_branch for gating later
    // In a full Mamba, there's also a y branch from the convolution, but we'll combine
    return std::make_tuple(x_branch, z_branch, torch::Tensor());
}

torch::Tensor MambaLayer::discrete_state_update(torch::Tensor x_t) {
    // x_t: [batch, d_inner]
    // Discretize A and B using Euler method (simplified)
    auto dt = 1.0f; // Assuming unit time step
    auto A = -torch::exp(A_log); // [d_inner, d_state]
    auto dA = torch::eye(d_state_, torch::kFloat) + dt * A.t(); // [d_state, d_state] (transposed for convenience)
    auto dB = dt * B.t(); // [d_state, d_inner] -> [d_inner, d_state] after transpose?
    
    // Actually, we want: state' = A * state + B * x
    // Discretized: state[k+1] = (I + dt*A) * state[k] + dt*B * x[k]
    // We'll keep state as [batch, d_inner, d_state] and x_t as [batch, d_inner]
    
    // For each batch and each feature in d_inner, we have a state vector of size d_state
    // state_: [batch, d_inner, d_state]
    // We want: state_new = state * dA^T + x_t * dB^T
    // But let's do a simpler step-wise update for clarity (less efficient but correct)
    
    // We'll do a loop over the d_inner dimension (not efficient but clear)
    auto new_state = state_.clone();
    for (int64_t i = 0; i < d_inner_; ++i) {
        auto state_i = state_.select(1, i); // [batch, d_state]
        auto x_i = x_t.select(1, i);        // [batch]
        // state_i_new = state_i * A[i,:]^T + x_i * B[i,:]^T
        // A[i,:] is [d_state], B[i,:] is [d_state]
        auto A_i = A.index({i, torch::indexing::Slice()}); // [d_state]
        auto B_i = B.index({i, torch::indexing::Slice()}); // [d_state]
        
        // state_i_new = state_i * A_i + x_i.unsqueeze(-1) * B_i
        new_state.select(1, i) = state_i * A_i.unsqueeze(0) + 
                                 (x_i.unsqueeze(-1) * B_i.unsqueeze(0));
    }
    
    // Compute y = C * state + D * x
    auto y = torch::zeros_like(x_t);
    for (int64_t i = 0; i < d_inner_; ++i) {
        auto state_i = new_state.select(1, i); // [batch, d_state]
        auto C_i = C.index({torch::indexing::Slice(), i}); // [d_state]
        // y += state_i * C_i
        y += state_i * C_i.unsqueeze(0);
        // y += D[i] * x_t[:, i]
        y += D.index({i}) * x_t.select(1, i);
    }
    
    state_ = new_state;
    return y;
}

torch::Tensor MambaLayer::forward(torch::Tensor x) {
    // x: [batch, seq, d_model]
    int64_t batch = x.size(0);
    int64_t seq_len = x.size(1);
    
    // Initialize state if needed
    if (state_.sizes() != torch::IntArrayRef{batch, d_inner_, d_state_}) {
        reset_state(batch, seq_len);
    }
    
    // Step 1: Input projection
    auto [x_branch, z_branch, _] = proj_xyz(x); // Each [batch, seq, d_inner]
    
    // Step 2: Convolution
    // We need to transpose for Conv1d: [batch, d_inner, seq]
    auto x_transposed = x_branch.transpose(1, 2);
    auto conv_out = conv1d->forward(x_transposed); // [batch, d_inner, seq + d_conv - 1]
    // Remove the extra padding from the convolution (we added d_conv-1)
    conv_out = conv_out.narrow(2, 0, seq_len); // [batch, d_inner, seq]
    conv_out = torch::silu(conv_out); // Activation
    // Transpose back: [batch, seq, d_inner]
    conv_out = conv_out.transpose(1, 2);
    
    // Step 3: Gating
    auto gate = torch::silu(z_branch); // [batch, seq, d_inner]
    auto gated_out = conv_out * gate; // [batch, seq, d_inner]
    
    // Step 4: SSM recurrence (step-wise for simplicity)
    // We'll process each time step and update the state
    auto ssm_out = torch::zeros_like(gated_out);
    for (int64_t t = 0; t < seq_len; ++t) {
        auto x_t = gated_out.select(1, t); // [batch, d_inner]
        auto y_t = discrete_state_update(x_t); // [batch, d_inner]
        ssm_out.select(1, t) = y_t;
    }
    
    // Step 5: Output projection
    auto output = out_proj->forward(ssm_out); // [batch, seq, d_model]
    
    return output;
}