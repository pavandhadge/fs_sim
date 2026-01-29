#include "util/tokenizer.h"
#include <sstream>

std::vector<std::string> tokenize_path(const std::string& input, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::stringstream ss(input);

    while(std::getline(ss, token, delimiter)) {
        if (!token.empty()) { // GEMINI FIX: Skip empty tokens (fixes // issues)
            tokens.push_back(token);
        }
    }
    return tokens;
}
