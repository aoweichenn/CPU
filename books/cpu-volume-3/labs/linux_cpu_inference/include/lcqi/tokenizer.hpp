#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace lcqi {

struct TokenEntry {
    std::string text;
    std::int32_t id = 0;
};

struct TokenizerModel {
    std::int32_t bos_id = -1;
    std::int32_t eos_id = -1;
    std::int32_t unk_id = -1;
    std::string chat_template_hash;
    std::vector<TokenEntry> vocab;
};

[[nodiscard]] TokenizerModel load_tokenizer(const std::filesystem::path& path);

[[nodiscard]] std::vector<std::int32_t> encode_prompt(
    const TokenizerModel& tokenizer,
    std::string_view prompt);

[[nodiscard]] std::uint64_t tokenizer_contract_hash(const TokenizerModel& tokenizer);

}  // namespace lcqi
