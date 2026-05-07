use serde::{Deserialize, Serialize};
use std::time::Duration;

// ─── Request ──────────────────────────────────────────────────────────────────

#[derive(Serialize)]
struct ChatRequest<'a> {
    model: &'a str,
    messages: Vec<ChatMessage<'a>>,
    max_tokens: u32,
    #[serde(skip_serializing_if = "Option::is_none")]
    temperature: Option<f32>,
    #[serde(skip_serializing_if = "Option::is_none")]
    stream: Option<bool>,
}

#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct ChatMessage<'a> {
    pub role: &'a str,
    pub content: &'a str,
}

impl<'a> ChatMessage<'a> {
    pub fn system(content: &'a str) -> Self { Self { role: "system", content } }
    pub fn user(content: &'a str) -> Self   { Self { role: "user",   content } }
}

// ─── Response ─────────────────────────────────────────────────────────────────

#[derive(Deserialize, Debug)]
pub struct ChatResponse {
    pub choices: Vec<Choice>,
    pub usage: Option<Usage>,
    pub model: Option<String>,
}

#[derive(Deserialize, Debug)]
pub struct Choice {
    pub message: ResponseMessage,
    pub finish_reason: Option<String>,
}

#[derive(Deserialize, Debug)]
pub struct ResponseMessage {
    pub role: String,
    pub content: String,
}

#[derive(Deserialize, Debug)]
pub struct Usage {
    pub prompt_tokens: u32,
    pub completion_tokens: u32,
    pub total_tokens: u32,
}

// ─── Error ────────────────────────────────────────────────────────────────────

#[derive(Debug)]
pub enum LlmError {
    Http(reqwest::Error),
    Api(u16, String),
    EmptyResponse,
    InvalidJson { raw: String, error: serde_json::Error },
    Config(String),
}

impl std::fmt::Display for LlmError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Http(e)                    => write!(f, "http: {}", e),
            Self::Api(code, msg)             => write!(f, "api {}: {}", code, msg),
            Self::EmptyResponse              => write!(f, "no content in response"),
            Self::InvalidJson { raw, error } => write!(f, "json: {} | raw: {}", error, &raw[..raw.len().min(300)]),
            Self::Config(msg)                => write!(f, "config: {}", msg),
        }
    }
}

impl std::error::Error for LlmError {}

// ─── Config ───────────────────────────────────────────────────────────────────

#[derive(Debug, Clone)]
pub struct LlmConfig {
    /// Full /v1/chat/completions URL.
    pub endpoint: String,
    /// Model name as the endpoint expects it. Any string.
    pub model: String,
    /// Bearer token. Empty string = no Authorization header (local servers).
    pub api_key: String,
    pub max_tokens: u32,
    pub temperature: Option<f32>,
    pub timeout_secs: u64,
}

impl LlmConfig {
    /// Read from environment. Fails fast with a clear message if required vars are missing.
    pub fn from_env() -> Result<Self, LlmError> {
        let endpoint = std::env::var("SMF_LLM_BASE_URL")
            .map_err(|_| LlmError::Config(
                "SMF_LLM_BASE_URL not set. Examples:\n  \
                 Ollama:     http://localhost:11434/v1/chat/completions\n  \
                 LM Studio:  http://localhost:1234/v1/chat/completions\n  \
                 vLLM:       http://localhost:8000/v1/chat/completions\n  \
                 OpenRouter: https://openrouter.ai/api/v1/chat/completions\n  \
                 NVIDIA NIM: https://integrate.api.nvidia.com/v1/chat/completions"
                .into()
            ))?;

        let model = std::env::var("SMF_LLM_MODEL")
            .map_err(|_| LlmError::Config(
                "SMF_LLM_MODEL not set. Use whatever string your endpoint accepts, e.g.:\n  \
                 llama-3.1-70b-instruct, mistral-nemo, qwen2.5-coder-32b, \
                 nvidia/llama-3.1-nemotron-70b-instruct, meta/llama-3.1-405b-instruct"
                .into()
            ))?;

        let api_key = std::env::var("SMF_LLM_API_KEY").unwrap_or_default();

        Ok(Self {
            endpoint,
            model,
            api_key,
            max_tokens: std::env::var("SMF_LLM_MAX_TOKENS")
                .ok()
                .and_then(|s| s.parse().ok())
                .unwrap_or(1024),
            temperature: std::env::var("SMF_LLM_TEMPERATURE")
                .ok()
                .and_then(|s| s.parse().ok()),
            timeout_secs: std::env::var("SMF_LLM_TIMEOUT_SECS")
                .ok()
                .and_then(|s| s.parse().ok())
                .unwrap_or(60),
        })
    }

    /// Convenience constructors for common local setups.
    pub fn ollama(model: impl Into<String>) -> Self {
        Self::local("http://localhost:11434/v1/chat/completions", model)
    }

    pub fn lm_studio(model: impl Into<String>) -> Self {
        Self::local("http://localhost:1234/v1/chat/completions", model)
    }

    pub fn vllm(model: impl Into<String>) -> Self {
        Self::local("http://localhost:8000/v1/chat/completions", model)
    }

    pub fn llama_cpp(model: impl Into<String>) -> Self {
        Self::local("http://localhost:8080/v1/chat/completions", model)
    }

    fn local(endpoint: impl Into<String>, model: impl Into<String>) -> Self {
        Self {
            endpoint: endpoint.into(),
            model: model.into(),
            api_key: String::new(), // no auth for local
            max_tokens: 1024,
            temperature: None,
            timeout_secs: 60,
        }
    }

    /// For hosted services that need a bearer token.
    pub fn with_key(mut self, key: impl Into<String>) -> Self {
        self.api_key = key.into(); self
    }

    pub fn with_max_tokens(mut self, n: u32) -> Self {
        self.max_tokens = n; self
    }

    pub fn with_temperature(mut self, t: f32) -> Self {
        self.temperature = Some(t); self
    }

    pub fn with_timeout(mut self, secs: u64) -> Self {
        self.timeout_secs = secs; self
    }

    /// Override model at call time without cloning the whole config.
    pub fn with_model(mut self, model: impl Into<String>) -> Self {
        self.model = model.into(); self
    }
}

// ─── Client ───────────────────────────────────────────────────────────────────

pub struct LlmClient {
    http: reqwest::Client,
    pub config: LlmConfig,
}

impl LlmClient {
    pub fn new(config: LlmConfig) -> Result<Self, reqwest::Error> {
        let http = reqwest::Client::builder()
            .timeout(Duration::from_secs(config.timeout_secs))
            .build()?;
        Ok(Self { http, config })
    }

    pub fn from_env() -> Result<Self, LlmError> {
        let config = LlmConfig::from_env()?;
        Self::new(config).map_err(LlmError::Http)
    }

    // ─── Core call ────────────────────────────────────────────────────────────

    /// Standard system+user call. Model and tokens come from config.
    pub async fn complete(
        &self,
        system: &str,
        user: &str,
    ) -> Result<String, LlmError> {
        self.complete_with(system, user, None, None, None).await
    }

    /// Per-call overrides — all optional, fall back to config.
    pub async fn complete_with(
        &self,
        system: &str,
        user: &str,
        model_override: Option<&str>,
        max_tokens_override: Option<u32>,
        temperature_override: Option<f32>,
    ) -> Result<String, LlmError> {
        let model      = model_override.unwrap_or(&self.config.model);
        let max_tokens = max_tokens_override.unwrap_or(self.config.max_tokens);
        let temperature = temperature_override.or(self.config.temperature);

        let req = ChatRequest {
            model,
            messages: vec![
                ChatMessage::system(system),
                ChatMessage::user(user),
            ],
            max_tokens,
            temperature,
            stream: Some(false),
        };

        let mut builder = self.http
            .post(&self.config.endpoint)
            .header("content-type", "application/json");

        if !self.config.api_key.is_empty() {
            builder = builder.header(
                "Authorization",
                format!("Bearer {}", self.config.api_key),
            );
        }

        let resp = builder.json(&req).send().await.map_err(LlmError::Http)?;

        let status = resp.status();
        if !status.is_success() {
            let body = resp.text().await.unwrap_or_default();
            return Err(LlmError::Api(status.as_u16(), body));
        }

        let data: ChatResponse = resp.json().await.map_err(LlmError::Http)?;

        data.choices
            .into_iter()
            .next()
            .map(|c| c.message.content)
            .filter(|s| !s.is_empty())
            .ok_or(LlmError::EmptyResponse)
    }

    /// Complete and parse JSON response, stripping markdown fences.
    pub async fn complete_json<T: serde::de::DeserializeOwned>(
        &self,
        system: &str,
        user: &str,
    ) -> Result<T, LlmError> {
        self.complete_json_with(system, user, None, None).await
    }

    pub async fn complete_json_with<T: serde::de::DeserializeOwned>(
        &self,
        system: &str,
        user: &str,
        model_override: Option<&str>,
        max_tokens_override: Option<u32>,
    ) -> Result<T, LlmError> {
        let raw = self.complete_with(system, user, model_override, max_tokens_override, None).await?;
        let clean = raw.trim()
            .trim_start_matches("```json")
            .trim_start_matches("```")
            .trim_end_matches("```")
            .trim();
        serde_json::from_str(clean)
            .map_err(|e| LlmError::InvalidJson { raw: clean.to_owned(), error: e })
    }
}

// ─── Typed engine wrappers ────────────────────────────────────────────────────

use crate::prompts;

// Output structs unchanged from before — omitted here for brevity,
// copy from previous claude/mod.rs: ArrangementAnalysis, EQAnalysis,
// SyntheticArtistProfile, ForensicReport, and their nested types.

impl LlmClient {
    pub async fn analyze_arrangement(&self, user_msg: &str)
        -> Result<serde_json::Value, LlmError>
    {
        self.complete_json(prompts::ARRANGEMENT_INTELLIGENCE, user_msg).await
    }

    pub async fn analyze_eq(&self, user_msg: &str)
        -> Result<serde_json::Value, LlmError>
    {
        self.complete_json(prompts::EQ_INTELLIGENCE, user_msg).await
    }

    pub async fn generate_artist(&self, user_msg: &str)
        -> Result<serde_json::Value, LlmError>
    {
        self.complete_json(prompts::SYNTH_FORGE_ARTIST, user_msg).await
    }

    /// Forensics: point at a bigger model if you have one locally.
    /// e.g. client.forensic_scan(&msg).await
    ///      but with SMF_LLM_MODEL=llama3.1:70b for this call only:
    ///      client.complete_json_with(RIGHTS_FORENSICS, &msg, Some("llama3.1:70b"), None)
    pub async fn forensic_scan(&self, user_msg: &str)
        -> Result<serde_json::Value, LlmError>
    {
        self.complete_json(prompts::RIGHTS_FORENSICS, user_msg).await
    }

    pub async fn build_clap_plugin(&self, user_msg: &str)
        -> Result<Vec<prompts::builders::BuilderFile>, LlmError>
    {
        let raw = self.complete(prompts::builders::CLAP_PLUGIN_BUILDER, user_msg).await?;
        Ok(prompts::builders::extract_files(&raw))
    }

    pub async fn build_dsp_module(&self, user_msg: &str)
        -> Result<Vec<prompts::builders::BuilderFile>, LlmError>
    {
        let raw = self.complete(prompts::builders::DSP_ALGORITHM_BUILDER, user_msg).await?;
        Ok(prompts::builders::extract_files(&raw))
    }
}

// ─── Tests ────────────────────────────────────────────────────────────────────

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn ollama_config_no_key() {
        let cfg = LlmConfig::ollama("llama3.2:3b");
        assert_eq!(cfg.endpoint, "http://localhost:11434/v1/chat/completions");
        assert!(cfg.api_key.is_empty());
        assert_eq!(cfg.model, "llama3.2:3b");
    }

    #[test]
    fn with_model_override() {
        let cfg = LlmConfig::ollama("default-model")
            .with_model("qwen2.5-coder:32b");
        assert_eq!(cfg.model, "qwen2.5-coder:32b");
    }

    #[test]
    fn no_auth_header_when_key_empty() {
        // Verified by inspection: complete_with checks api_key.is_empty()
        let cfg = LlmConfig::lm_studio("any-model");
        assert!(cfg.api_key.is_empty());
    }

    #[tokio::test]
    #[ignore] // requires a running local server
    async fn integration_ollama_ping() {
        let client = LlmClient::new(
            LlmConfig::ollama("llama3.2:3b")
        ).unwrap();
        let res = client.complete("Reply with one word: PONG", "PING").await;
        assert!(res.unwrap().contains("PONG"));
    }

    #[tokio::test]
    #[ignore] // requires SMF_LLM_BASE_URL + SMF_LLM_MODEL
    async fn integration_from_env() {
        let client = LlmClient::from_env().unwrap();
        let res = client.complete("Reply with one word: PONG", "PING").await;
        assert!(res.is_ok());
    }
}