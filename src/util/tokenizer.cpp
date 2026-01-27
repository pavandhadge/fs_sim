#include <string>
#include <vector>
#include <sstream>
#include "util/tokenizer.h"

std::vector<std::string> tokenize_path(const std::string& input, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(input);
    std::string token;

    while (std::getline(ss, token, delimiter)) {
        tokens.push_back(token);
    }

    return tokens;
}

