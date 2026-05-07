#ifndef MAMBA_MODEL_H
#define MAMBA_MODEL_H

#include <torch/torch.h>
#include "mamba_layer.h"

class MambaModel : public torch::nn::Module {
public:
    explicit MambaModel(int64_t d_model, int64_t n_layers = 4, 
                       int64_t d_state = 16, int64_t d_conv = 4, int64_t expand = 2);
    
    torch::Tensor forward(torch::Tensor x);
    
    // For resetting state between sequences
    void reset_state(int64_t batch_size, int64_t seq_len);
    
private:
    int64_t d_model_;
    int64_t n_layers_;
    
    // Input normalization
    torch::nn::LayerNorm input_norm{nullptr};
    
    // Mamba layers
    torch::nn::ModuleList layers;
    
    // Output normalization
    torch::nn::LayerNorm output_norm{nullptr};
};

#endif // MAMBA_MODEL_H