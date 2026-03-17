#include "util/tokenizer.h"
#include <sstream>

std::vector<std::string> tokenize_path(const std::string& input, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::stringstream ss(input);

    while(std::getline(ss, token, delimiter)) {
        if (!token.empty() && token != "/") { // Skip empty tokens and stray slashes
            tokens.push_back(token);
        }
    }
    return tokens;
}
