#pragma once

#include <string>

class AuthService {
public:
    std::string HashPassword(const std::string& password) const;
    std::string GenerateToken(const std::string& username) const;
    std::string ParseUsernameFromToken(const std::string& token) const;

private:
    static std::string Sha256(const std::string& input);
};
