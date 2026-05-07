#include "mamba_model.h"

MambaModel::MambaModel(int64_t d_model, int64_t n_layers, 
                       int64_t d_state, int64_t d_conv, int64_t expand)
    : d_model_(d_model), n_layers_(n_layers) {
    
    // Input normalization
    input_norm = register_module("input_norm", torch::nn::LayerNorm(torch::nn::LayerNormOptions(d_model)));
    
    // Create Mamba layers
    layers = torch::nn::ModuleList{};
    for (int64_t i = 0; i < n_layers_; ++i) {
        auto layer = std::make_shared<MambaLayer>(d_model, d_state, d_conv, expand);
        layers->push_back(layer);
    }
    
    // Output normalization
    output_norm = register_module("output_norm", torch::nn::LayerNorm(torch::nn::LayerNormOptions(d_model)));
}

void MambaModel::reset_state(int64_t batch_size, int64_t seq_len) {
    // Reset state in all layers
    for (auto& layer : *layers) {
        auto mamba_layer = std::dynamic_pointer_cast<MambaLayer>(layer);
        if (mamba_layer) {
            mamba_layer->reset_state(batch_size, seq_len);
        }
    }
}

torch::Tensor MambaModel::forward(torch::Tensor x) {
    // x: [batch, seq, d_model]
    
    // Input normalization
    auto out = input_norm->forward(x);
    
    // Pass through Mamba layers with residual connections
    for (auto& layer : *layers) {
        auto mamba_layer = std::dynamic_pointer_cast<MambaLayer>(layer);
        if (mamba_layer) {
            auto residual = out;
            out = mamba_layer->forward(out);
            out = out + residual;  // Residual connection
        }
    }
    
    // Output normalization
    out = output_norm->forward(out);
    
    return out;
}