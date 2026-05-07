#ifndef MAMBA_LAYER_H
#define MAMBA_LAYER_H

#include <torch/torch.h>

class MambaLayer : public torch::nn::Module {
public:
    explicit MambaLayer(int64_t d_model, int64_t d_state = 16, int64_t d_conv = 4, int64_t expand = 2);
    
    torch::Tensor forward(torch::Tensor x);
    
    // For resetting state between sequences
    void reset_state(int64_t batch_size, int64_t seq_len);
    
private:
    int64_t d_model_;
    int64_t d_state_;
    int64_t d_conv_;
    int64_t expand_;
    int64_t d_inner_;
    
    // Linear projections
    torch::nn::Linear in_proj{nullptr};
    torch::nn::Linear out_proj{nullptr};
    
    // Convolutional block
    torch::nn::Conv1d conv1d{nullptr};
    
    // SSM parameters
    torch::Tensor A_log;  // Diagonal state matrix (log for stability)
    torch::Tensor B;      // Input projection matrix
    torch::Tensor C;      // Output projection matrix
    torch::Tensor D;      // Skip connection
    
    // State tensor for recurrence
    torch::Tensor state_;
};

#endif // MAMBA_LAYER_H