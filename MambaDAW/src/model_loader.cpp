#include "model_loader.h"
#include <iostream>

bool ModelLoader::loadModel(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        // Deserialize the ScriptModule from a file using torch::jit::load.
        model_ = torch::jit::load(path);
        model_.to(torch::kCPU);
        model_.eval();  // Set to evaluation mode
        model_loaded_ = true;
        return true;
    } catch (const c10::Error& e) {
        std::cerr << "Error loading the model: " << e.what() << std::endl;
        model_loaded_ = false;
        return false;
    }
}

torch::jit::script::Module ModelLoader::getModel() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return model_;
}

bool ModelLoader::isModelLoaded() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return model_loaded_;
}

void ModelLoader::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    model_ = torch::jit::script::Module();
    model_loaded_ = false;
}