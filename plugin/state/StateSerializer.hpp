#pragma once
// StateSerializer — bridges CLAP plugin state ↔ ARA document model.
//
// Scope separation:
//   CLAP state  = per-plugin-instance blob (attached to the FX chain slot).
//                 Saved/loaded via clap_plugin_state extension.
//                 Contains: all ParameterBank values + model path + version.
//
//   ARA state   = per-project document blob (attached to the ARA document).
//                 Saved/loaded via ARADocumentController::doStoreObjectsToArchive.
//                 Contains: per-source analysis cache + arrangement analysis JSON.
//
// When to sync:
//   • On CLAP state load: update ParameterBank → post AudioCommand::ParamChanged
//     for each changed param → also notify ARADocumentController if analysis
//     quality params changed (model variant, context length).
//   • On ARA archive load: restore source analysis cache; no CLAP params touched.
//
// Binary format (CLAP state blob):
//   [4 bytes magic "MBFX"]
//   [4 bytes version (uint32_t)]
//   [kParamCount × 4 bytes float  — parameter values in ParamId order]
//   [4 bytes path_len]
//   [path_len bytes UTF-8 model root path]
//
// Version migration:
//   MigrationTable maps old version → transform function applied before restore.

#include <clap/clap.h>
#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <span>

namespace mamba::core { class ParameterBank; }

namespace mamba::state {

static constexpr uint32_t kStateVersion = 1;
static constexpr uint32_t kStateMagic   = 0x4D424658; // "MBFX"

struct StateBlob {
    uint32_t     version{kStateVersion};
    std::vector<float> param_values;    // kParamCount floats
    std::string  model_root;            // absolute or relative path
};

// Migration function: transforms a StateBlob from an old version to the next.
using MigrationFn = std::function<void(StateBlob&)>;

class StateSerializer {
public:
    explicit StateSerializer(core::ParameterBank& params);

    // Serialise current parameter values + model path to CLAP output stream.
    // Called from main thread (plugin not processing).
    bool save(const clap_ostream_t* stream) const;

    // Deserialise CLAP input stream; apply migrations; update ParameterBank.
    bool load(const clap_istream_t* stream);

    void set_model_root(std::string path) { model_root_ = std::move(path); }
    const std::string& model_root()  const noexcept { return model_root_; }

private:
    bool write_blob(const clap_ostream_t* stream, const StateBlob& blob) const;
    bool read_blob (const clap_istream_t* stream, StateBlob& out) const;

    // Apply version migrations in sequence until blob.version == kStateVersion.
    static void migrate(StateBlob& blob);

    // Migration table: index = from_version.
    static const MigrationFn kMigrations[];

    core::ParameterBank& params_;
    std::string          model_root_;
};

} // namespace mamba::state
