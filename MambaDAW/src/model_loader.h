#ifndef MODEL_LOADER_H
#define MODEL_LOADER_H

#include <torch/torch.h>
#include <string>
#include <mutex>

class ModelLoader {
public:
    static ModelLoader& getInstance() {
        static ModelLoader instance;
        return instance;
    }

    // Load the TorchScript model from the given path
    bool loadModel(const std::string& path);
    
    // Get the loaded model
    torch::jit::script::Module getModel() const;
    
    // Check if model is loaded
    bool isModelLoaded() const;
    
    // Reset the model (useful for reloading)
    void reset();

private:
    ModelLoader() = default;
    ~ModelLoader() = default;
    ModelLoader(const ModelLoader&) = delete;
    ModelLoader& operator=(const ModelLoader&) = delete;
    
    torch::jit::script::Module model_;
    mutable std::mutex mutex_;
    bool model_loaded_ = false;
};

#endif // MODEL_LOADER_H