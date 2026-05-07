use std::ffi::{CStr, CString};
use std::os::raw::{c_char, c_uint};
use serde::{Deserialize, Serialize};
use serde_json::json;

// Define the FFI types matching the C header
#[repr(C)]
pub struct ArrangeSection {
    pub section_type: *const c_char,
    pub bar_offset: c_uint,
    pub bar_length: c_uint,
    pub energy_rms: f32,
    pub spectral_flux: f32,
    pub crest_factor: f32,
}

// We don't need to define ArrangeAnalysis because we are returning a JSON string.

// The function that will be called from C
#[no_mangle]
pub extern "C" fn ara_analyze_arrangement(
    sections: *const ArrangeSection,
    section_count: c_uint,
    genre: *const c_char,
    bpm: c_uint,
) -> *mut c_char {
    // Convert the input sections to a Rust Vec
    let sections_vec = unsafe {
        std::slice::from_raw_parts(sections, section_count as usize)
            .iter()
            .map(|s| Section {
                section_type: unsafe { CStr::from_ptr(s.section_type) }.to_string_lossy().into_owned(),
                bar_offset: s.bar_offset,
                bar_length: s.bar_length,
                energy_rms: s.energy_rms,
                spectral_flux: s.spectral_flux,
                crest_factor: s.crest_factor,
            })
            .collect::<Vec<Section>>()
    };

    // Convert genre and bpm to Rust types
    let genre_str = unsafe { CStr::from_ptr(genre) }.to_string_lossy().into_owned();
    let bpm_val = bpm as u32;

    // Build the user prompt using the template and the data
    let user_prompt = build_user_prompt(&genre_str, bpm_val, &sections_vec);

    // Call the Claude API
    let api_key = std::env::var("ANTHROPIC_API_KEY").expect("ANTHROPIC_API_KEY not set");
    let client = reqwest::blocking::Client::new();
    let res = client
        .post("https://api.anthropic.com/v1/messages")
        .header("x-api-key", api_key)
        .header("anthropic-version", "2023-06-01")
        .header("content-type", "application/json")
        .body(json!({
            "model": "claude-sonnet-4-20250514",
            "max_tokens": 1000,
            "system": super::prompts::ARRANGEMENT_INTELLIGENCE,
            "messages": [
                {
                    "role": "user",
                    "content": user_prompt
                }
            ]
        }))
        .send()
        .expect("Failed to send request to Claude API")
        .text()
        .expect("Failed to get response from Claude API");

    // Parse the response to extract the text content (assuming the response is in the expected format)
    // The Claude API returns a JSON with a "content" array of objects, each having a "type" and "text".
    // We assume the first content block is the text we want.
    let response_json: serde_json::Value = serde_json::from_str(&res).expect("Failed to parse Claude API response");
    let text_content = response_json["content"][0]["text"]
        .as_str()
        .expect("Expected text content in Claude API response")
        .to_string();

    // Convert the text content to a C string that the caller must free
    CString::new(text_content).expect("Failed to create CString").into_raw()
}

#[no_mangle]
pub extern "C" fn ara_free_analysis(ptr: *mut c_char) {
    if !ptr.is_null() {
        unsafe {
            let _ = CString::from_raw(ptr);
        }
    }
}

// Helper struct to hold a section for prompt building
#[derive(Serialize, Deserialize)]
struct Section {
    section_type: String,
    bar_offset: u32,
    bar_length: u32,
    energy_rms: f32,
    spectral_flux: f32,
    crest_factor: f32,
}

// Build the user prompt from the template and the data
fn build_user_prompt(genre: &str, bpm: u32, sections: &[Section]) -> String {
    // Format the sections array as a JSON string for the prompt
    let sections_json = serde_json::to_string(sections).expect("Failed to serialize sections");

    // Replace the placeholders in the template
    let template = super::prompts::ARRANGEMENT_INTELLIGENCE_USER_TEMPLATE;
    template
        .replace("{genre}", genre)
        .replace("{bpm}", &bpm.to_string())
        .replace("{sections}", &sections_json)
}

// We need to include the prompts module. Since we are in a separate crate, we can copy the prompts
// or we can create a separate crate for prompts. For simplicity, we'll copy the relevant parts here.
// In a real project, you might want to create a shared crate for prompts.
mod prompts {
    pub const ARRANGEMENT_INTELLIGENCE: &str = r#"
You are an arrangement intelligence engine embedded as an ARA2 plugin inside a
professional DAW. You have direct read access to every audio region, region
sequence, and musical context in the session via the ARA SDK — you are not
guessing at the arrangement, you are reading it.

Your role is that of a senior record producer with credits across commercial
releases. You analyze arrangements with clinical precision and commercial
ruthlessness. You do not flatter. You identify structural weaknesses, missed
tension opportunities, and energy arc problems the way a mix engineer identifies
phase issues — as technical facts, not opinions.

ANALYSIS PRINCIPLES:
- Tension is earned through contrast, not volume.
- Every section must justify its bar count relative to its neighbors.
- The energy arc must have at least one deliberate valley before the final peak.
- Transitions are load-bearing architecture — a bad transition kills momentum
  regardless of how strong the surrounding sections are.
- Commercial structure is not formulaic; it is listener-tested. Deviations must
  be intentional and defensible.
- If two consecutive sections share the same energy level and density, one of
  them is redundant.

OUTPUT RULES:
- Return ONLY valid JSON. No markdown. No preamble. No explanation.
- Be specific. "The pre-chorus at bar 12 needs 2 bars cut" beats "consider
  shortening the pre-chorus."
- Prioritize suggestions by commercial impact, not ease of implementation.
- Section ratings must be earned: "strong" requires both energy appropriateness
  AND structural necessity.
"#;

    pub const ARRANGEMENT_INTELLIGENCE_USER_TEMPLATE: &str = r#"
ANALYZE THIS ARRANGEMENT:
Genre: {genre}
BPM: {bpm} | Key: {key}
Total bars: {total_bars}

SECTIONS (ARA region sequence):
{sections}

Return EXACTLY this JSON:
{
  "arrangement_score": <0-100>,
  "tension_verdict": "<one sentence, brutally specific>",
  "commercial_verdict": "<one sentence>",
  "structure_type": "<e.g. ABABCB, verse-heavy, climax-deferred>",
  "arc_type": "classic_hill|double_peak|flat|ascending|volatile|sag_middle",
  "section_notes": [
    { "id": <1-indexed>, "rating": "strong|weak|tension|neutral", "note": "<max 12 words>" }
  ],
  "transition_issues": [
    { "from": "<type>", "to": "<type>", "issue": "<specific, max 12 words>" }
  ],
  "suggestions": [
    {
      "priority": <1-5, 1=critical>,
      "section": "<type + position>",
      "type": "CUT|ADD|REARRANGE|REWRITE|ENERGY|DYNAMICS",
      "action": "<specific, actionable, one sentence>",
      "reasoning": "<producer logic, max 15 words>"
    }
  ],
  "missing_element": "<what this arrangement lacks, one sentence, or null>
}
"#;
}